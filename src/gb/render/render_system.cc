// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_system.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "gb/base/context_builder.h"
#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "gb/image/image_file.h"
#include "gb/render/render_backend.h"
#include "gb/render/render_pipeline.h"
#include "gb/render/render_resource_chunks.h"
#include "gb/resource/resource_chunks.h"
#include "gb/resource/resource_file_reader.h"
#include "gb/resource/resource_file_writer.h"
#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_system.h"

namespace gb {

namespace fb = flatbuffers;
using fb::FlatBufferBuilder;

namespace {

fb::Offset<fb::Vector<fb::Offset<fbs::Binding>>> WriteBindings(
    FlatBufferBuilder* builder, absl::Span<const Binding> bindings) {
  return builder->CreateVector<fb::Offset<fbs::Binding>>(
      bindings.size(), [builder, &bindings](size_t i) {
        fb::Offset<fb::String> fb_constants_name;
        if (bindings[i].constants_type != nullptr) {
          fb_constants_name = builder->CreateSharedString(
              bindings[i].constants_type->GetName());
        }

        fbs::BindingBuilder binding_builder(*builder);
        binding_builder.add_shaders(
            static_cast<uint8_t>(bindings[i].shader_types.GetMask()));
        binding_builder.add_set(ToFbs(bindings[i].set));
        binding_builder.add_index(bindings[i].index);
        binding_builder.add_type(ToFbs(bindings[i].binding_type));
        binding_builder.add_volatility(ToFbs(bindings[i].volatility));
        binding_builder.add_constants_name(fb_constants_name);
        return binding_builder.Finish();
      });
}

// Reads "fb_bindings" into "bindings", returning false if any binding has an
// unknown type or constants type. The bindings are not otherwise validated.
bool ReadBindings(RenderSystem* render_system,
                  const fb::Vector<fb::Offset<fbs::Binding>>* fb_bindings,
                  std::vector<Binding>* bindings) {
  if (fb_bindings == nullptr) {
    return true;
  }
  const int count = static_cast<int>(fb_bindings->size());
  bindings->resize(count);
  for (int i = 0; i < count; ++i) {
    auto* fb_binding = fb_bindings->Get(i);
    if (fb_binding == nullptr) {
      LOG(ERROR) << "Null binding at index " << i;
      return false;
    }
    Binding& binding = (*bindings)[i];
    binding.SetShaders(ShaderTypes(fb_binding->shaders()));
    binding.SetLocation(FromFbs(fb_binding->set()), fb_binding->index());
    switch (fb_binding->type()) {
      case fbs::BindingType_Texture:
        binding.SetTexture();
        break;
      case fbs::BindingType_TextureArray:
        binding.SetTextureArray();
        break;
      case fbs::BindingType_VolumeTexture:
        binding.SetVolumeTexture();
        break;
      case fbs::BindingType_Constants: {
        if (fb_binding->constants_name() == nullptr) {
          LOG(ERROR) << "Unspecified constants name for binding at index " << i;
          return false;
        }
        const char* constants_name = fb_binding->constants_name()->c_str();
        const RenderDataType* constants_type =
            render_system->GetConstantsType(constants_name);
        if (constants_type == nullptr) {
          LOG(ERROR) << "Constants type \"" << constants_name
                     << "\" for binding at index " << i << " is not registered";
          return false;
        }
        binding.SetConstants(constants_type, FromFbs(fb_binding->volatility()));
      } break;
      default:
        LOG(ERROR) << "Unhandled binding type: "
                   << EnumNameBindingType(fb_binding->type());
        return false;
    }
  }
  return true;
}

fb::Offset<fb::Vector<fb::Offset<fbs::BindingDataEntry>>> WriteBindingData(
    RenderInternal access_token, FlatBufferBuilder* builder,
    BindingSet binding_set, absl::Span<const Binding> bindings,
    const BindingData* binding_data) {
  std::vector<fb::Offset<fbs::BindingDataEntry>> fb_binding_data;
  fb_binding_data.reserve(bindings.size());
  for (const auto& binding : bindings) {
    if (binding.set != binding_set) {
      continue;
    }
    ResourceId texture_id = 0;
    ResourceId texture_array_id = 0;
    ResourceId volume_texture_id = 0;
    fb::Offset<fb::Vector<uint8_t>> fb_constants_data;
    switch (binding.binding_type) {
      case BindingType::kTexture: {
        const auto* texture = binding_data->GetTexture(binding.index);
        if (texture != nullptr) {
          texture_id = texture->GetResourceId();
        }
      } break;
      case BindingType::kTextureArray: {
        const auto* texture_array =
            binding_data->GetTextureArray(binding.index);
        if (texture_array != nullptr) {
          texture_array_id = texture_array->GetResourceId();
        }
      } break;
      case BindingType::kVolumeTexture: {
        const auto* volume_texture =
            binding_data->GetVolumeTexture(binding.index);
        if (volume_texture != nullptr) {
          volume_texture_id = volume_texture->GetResourceId();
        }
      } break;
      case BindingType::kConstants: {
        std::vector<uint8_t> buffer(binding.constants_type->GetSize());
        binding_data->GetInternal(access_token, binding.index,
                                  binding.constants_type->GetType(),
                                  buffer.data());
        fb_constants_data = builder->CreateVector(buffer);
      } break;
      default:
        LOG(ERROR) << "Unspecified binding type: "
                   << static_cast<int>(binding.binding_type);
    }

    fbs::BindingDataEntryBuilder binding_entry_builder(*builder);
    binding_entry_builder.add_index(binding.index);
    binding_entry_builder.add_type(ToFbs(binding.binding_type));
    binding_entry_builder.add_texture_id(texture_id);
    binding_entry_builder.add_texture_array_id(texture_array_id);
    binding_entry_builder.add_volume_texture_id(volume_texture_id);
    binding_entry_builder.add_constants_data(fb_constants_data);
    fb_binding_data.emplace_back(binding_entry_builder.Finish());
  }
  if (fb_binding_data.empty()) {
    return {};
  }
  return builder->CreateVector(fb_binding_data);
}

// Reads "fb_binding_data" into "binding_data", which holds the bindings in
// "bindings" for its set. Returns false if an entry doesn't match the binding
// at its index, or references a resource that isn't loaded.
bool ReadBindingData(
    RenderInternal access_token, const FileResources* file_resources,
    absl::Span<const Binding> bindings,
    const fb::Vector<fb::Offset<fbs::BindingDataEntry>>* fb_binding_data,
    BindingData* binding_data) {
  if (fb_binding_data == nullptr) {
    // No binding data is valid.
    return true;
  }
  const int count = static_cast<int>(fb_binding_data->size());
  for (int i = 0; i < count; ++i) {
    const auto* fb_binding_data_entry = fb_binding_data->Get(i);
    if (fb_binding_data_entry == nullptr) {
      LOG(ERROR) << "Null binding data entry at index " << i;
      return false;
    }
    const int index = fb_binding_data_entry->index();
    const Binding* binding = nullptr;
    for (const Binding& set_binding : bindings) {
      if (set_binding.set == binding_data->GetSet() &&
          set_binding.index == index) {
        binding = &set_binding;
        break;
      }
    }
    if (binding == nullptr ||
        binding->binding_type != FromFbs(fb_binding_data_entry->type())) {
      LOG(ERROR) << "Binding data of type "
                 << EnumNameBindingType(fb_binding_data_entry->type())
                 << " at index " << index << " does not match a binding";
      return false;
    }
    switch (binding->binding_type) {
      case BindingType::kTexture: {
        const ResourceId texture_id = fb_binding_data_entry->texture_id();
        if (texture_id == 0) {
          // No texture is valid.
          break;
        }
        auto* texture = file_resources->GetResource<Texture>(texture_id);
        if (texture == nullptr) {
          LOG(ERROR) << "Referenced binding data texture at index " << index
                     << " not loaded. ID=" << texture_id;
          return false;
        }
        binding_data->SetTexture(index, texture);
      } break;
      case BindingType::kTextureArray: {
        const ResourceId texture_array_id =
            fb_binding_data_entry->texture_array_id();
        if (texture_array_id == 0) {
          // No texture_array is valid.
          break;
        }
        auto* texture_array =
            file_resources->GetResource<TextureArray>(texture_array_id);
        if (texture_array == nullptr) {
          LOG(ERROR) << "Referenced binding data texture array at index "
                     << index << " not loaded. ID=" << texture_array_id;
          return false;
        }
        binding_data->SetTextureArray(index, texture_array);
      } break;
      case BindingType::kVolumeTexture: {
        const ResourceId volume_texture_id =
            fb_binding_data_entry->volume_texture_id();
        if (volume_texture_id == 0) {
          // No volume texture is valid.
          break;
        }
        auto* volume_texture =
            file_resources->GetResource<VolumeTexture>(volume_texture_id);
        if (volume_texture == nullptr) {
          LOG(ERROR) << "Referenced binding data volume texture at index "
                     << index << " not loaded. ID=" << volume_texture_id;
          return false;
        }
        binding_data->SetVolumeTexture(index, volume_texture);
      } break;
      case BindingType::kConstants: {
        const auto* fb_constants_data = fb_binding_data_entry->constants_data();
        if (fb_constants_data == nullptr) {
          // No constants data is valid. This results in default values.
          break;
        }
        const RenderDataType* constants_type = binding->constants_type;
        if (static_cast<int>(fb_constants_data->size()) !=
            constants_type->GetSize()) {
          LOG(ERROR) << "Constants size " << fb_constants_data->size()
                     << " does not match expected size "
                     << constants_type->GetSize() << " at index " << index
                     << " for binding data";
          return false;
        }
        binding_data->SetInternal(access_token, index,
                                  constants_type->GetType(),
                                  fb_constants_data->data());
      } break;
      default:
        LOG(ERROR) << "Unhandled binding type "
                   << static_cast<int>(binding->binding_type)
                   << " at binding data index " << i;
        return false;
    }
  }
  return true;
}

// Adds "bindings" to "unique_bindings" with duplicates removed. Returns false
// if any binding is invalid, or differs from another at the same location.
bool GetUniqueBindings(absl::Span<const Binding> bindings,
                       std::vector<Binding>* unique_bindings) {
  absl::flat_hash_map<std::tuple<BindingSet, int>, int> binding_indices;
  for (const Binding& binding : bindings) {
    if (!binding.IsValid()) {
      LOG(ERROR) << "Invalid binding: set=" << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return false;
    }
    auto [it, added] =
        binding_indices.try_emplace({binding.set, binding.index},
                                    static_cast<int>(unique_bindings->size()));
    if (added) {
      unique_bindings->push_back(binding);
      continue;
    }
    if (binding != (*unique_bindings)[it->second]) {
      LOG(ERROR) << "Duplicate incompatible binding: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return false;
    }
  }
  return true;
}

// Adds "bindings" to "shader_bindings" for a shader of type "type", with
// duplicates removed. Returns false if any binding is invalid, isn't used by
// the shader type, or differs from another at the same location.
bool GetShaderBindings(ShaderType type, absl::Span<const Binding> bindings,
                       std::vector<Binding>* shader_bindings) {
  for (const Binding& binding : bindings) {
    if (!binding.shader_types.IsSet(type)) {
      LOG(ERROR) << "Invalid shader type for binding: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return false;
    }
  }
  return GetUniqueBindings(bindings, shader_bindings);
}

// Returns true if a texture of the specified size is within the limits, and
// logs an error naming "what" otherwise.
bool IsValidTextureSize(std::string_view what, int width, int height) {
  if (width <= 0 || width > kMaxTextureWidth || height <= 0 ||
      height > kMaxTextureHeight) {
    LOG(ERROR) << "Invalid " << what << " dimensions " << width << " by "
               << height << ", which must be from 1 by 1 to "
               << kMaxTextureWidth << " by " << kMaxTextureHeight;
    return false;
  }
  return true;
}

// Returns true if a texture array of the specified size is within the limits,
// and logs an error naming "what" otherwise.
bool IsValidTextureArraySize(std::string_view what, int count, int width,
                             int height) {
  if (count <= 0 || count > kMaxTextureArrayCount) {
    LOG(ERROR) << "Invalid " << what << " count " << count
               << ", which must be from 1 to " << kMaxTextureArrayCount;
    return false;
  }
  if (!IsValidTextureSize(what, width, height)) {
    return false;
  }
  const int64_t total_pixel_count =
      static_cast<int64_t>(count) * width * height;
  if (total_pixel_count > kMaxTextureArrayPixels) {
    LOG(ERROR) << "Invalid " << what << " with " << total_pixel_count
               << " pixels, more than the maximum of "
               << kMaxTextureArrayPixels;
    return false;
  }
  return true;
}

// Returns the texels saved in a texture chunk: its "texels", or for one saved
// before formats were added, its "pixels". Returns null, and logs an error
// naming "what", if there are fewer than "size_in_bytes" bytes.
const void* GetChunkTexels(std::string_view what,
                           const fb::Vector<uint8_t>* texels,
                           const fb::Vector<uint32_t>* pixels,
                           int64_t size_in_bytes) {
  const void* data = nullptr;
  int64_t data_size = 0;
  if (texels != nullptr) {
    data = texels->data();
    data_size = texels->size();
  } else if (pixels != nullptr) {
    data = pixels->data();
    data_size =
        int64_t{pixels->size()} * static_cast<int64_t>(sizeof(uint32_t));
  }
  if (data == nullptr || data_size < size_in_bytes) {
    LOG(ERROR) << "Failed to create " << what << " as it contains " << data_size
               << " bytes of texels, but " << size_in_bytes << " are required.";
    return nullptr;
  }
  return data;
}

// Returns true if a volume texture of the specified size is within the limits,
// and logs an error otherwise.
bool ValidateVolumeTextureSize(int width, int height, int depth) {
  if (width <= 0 || width > kMaxVolumeTextureSize || height <= 0 ||
      height > kMaxVolumeTextureSize || depth <= 0 ||
      depth > kMaxVolumeTextureSize) {
    LOG(ERROR) << "Invalid volume texture dimensions " << width << " by "
               << height << " by " << depth << ", which must each be from 1 to "
               << kMaxVolumeTextureSize;
    return false;
  }
  const int64_t texel_count = int64_t{width} * height * depth;
  if (texel_count > kMaxVolumeTextureTexels) {
    LOG(ERROR) << "Invalid volume texture with " << texel_count
               << " texels, more than the maximum of "
               << kMaxVolumeTextureTexels;
    return false;
  }
  return true;
}

fb::Offset<fbs::SamplerOptions> WriteSamplerOptions(
    FlatBufferBuilder* builder, const SamplerOptions& options) {
  fbs::SamplerOptionsBuilder sampler_options_builder(*builder);
  sampler_options_builder.add_filter(options.filter);
  sampler_options_builder.add_mipmap(options.mipmap);
  sampler_options_builder.add_border(options.border.Packed());
  sampler_options_builder.add_tile_size(options.tile_size);
  sampler_options_builder.add_address_mode(ToFbs(options.address_mode));
  return sampler_options_builder.Finish();
}

// Returns the sampler options saved in a chunk, or the default if there are
// none.
SamplerOptions ReadSamplerOptions(
    const fbs::SamplerOptions* fb_sampler_options) {
  SamplerOptions sampler_options;
  if (fb_sampler_options != nullptr) {
    sampler_options.SetFilter(fb_sampler_options->filter());
    sampler_options.SetMipmap(fb_sampler_options->mipmap());
    sampler_options.SetTileSize(fb_sampler_options->tile_size());
    sampler_options.SetAddressMode(FromFbs(fb_sampler_options->address_mode()),
                                   Pixel(fb_sampler_options->border()));
  }
  return sampler_options;
}

// Adds the bindings for a material type to "bindings": the bindings of its
// scene type and shaders. Bindings at the same location must be compatible, and
// are combined into one. Returns false if any are not.
bool GetMaterialTypeBindings(RenderSceneType* scene_type, Shader* vertex_shader,
                             Shader* fragment_shader,
                             std::vector<Binding>* bindings) {
  const std::pair<std::string_view, absl::Span<const Binding>> sources[] = {
      {"Scene type", scene_type->GetBindings()},
      {"Vertex shader", vertex_shader->GetBindings()},
      {"Fragment shader", fragment_shader->GetBindings()},
  };
  absl::flat_hash_map<std::tuple<BindingSet, int>, int> binding_indices;
  for (const auto& [source, source_bindings] : sources) {
    for (const Binding& binding : source_bindings) {
      auto [it, added] = binding_indices.try_emplace(
          {binding.set, binding.index}, static_cast<int>(bindings->size()));
      if (added) {
        bindings->push_back(binding);
        continue;
      }
      if (!(*bindings)[it->second].Combine(binding)) {
        LOG(ERROR) << source << " contains incompatible binding: set="
                   << static_cast<int>(binding.set)
                   << ", index=" << binding.index;
        return false;
      }
    }
  }
  return true;
}

}  // namespace

std::unique_ptr<RenderSystem> RenderSystem::Create(Contract contract) {
  ValidatedContext context(std::move(contract));
  if (!context.IsValid()) {
    return nullptr;
  }

  auto render_system = absl::WrapUnique(new RenderSystem(std::move(context)));
  if (!render_system->Init()) {
    return nullptr;
  }
  return render_system;
}

RenderSystem::RenderSystem(ValidatedContext context)
    : context_(std::move(context)) {
  backend_ = context_.GetPtr<RenderBackend>();
  debug_ = context_.GetValue<bool>(kKeyEnableDebug);
  SetRenderAssertEnabled(debug_);
  edit_ = context_.GetValue<bool>(kKeyEnableEdit);
}

bool RenderSystem::Init() {
  resource_writer_ = ResourceFileWriter::Create(context_);
  resource_writer_->RegisterResourceFlatBufferWriter<Texture>(
      kChunkTypeTexture, 1,
      [this](Context* context, Texture* resource, FlatBufferBuilder* builder) {
        return SaveTextureChunk(context, resource, builder);
      });
  resource_writer_->RegisterResourceFlatBufferWriter<TextureArray>(
      kChunkTypeTextureArray, 1,
      [this](Context* context, TextureArray* resource,
             FlatBufferBuilder* builder) {
        return SaveTextureArrayChunk(context, resource, builder);
      });
  resource_writer_->RegisterResourceFlatBufferWriter<VolumeTexture>(
      kChunkTypeVolumeTexture, 1,
      [this](Context* context, VolumeTexture* resource,
             FlatBufferBuilder* builder) {
        return SaveVolumeTextureChunk(context, resource, builder);
      });
  resource_writer_->RegisterResourceFlatBufferWriter<Shader>(
      kChunkTypeShader, 1,
      [this](Context* context, Shader* resource, FlatBufferBuilder* builder) {
        return SaveShaderChunk(context, resource, builder);
      });
  resource_writer_->RegisterResourceFlatBufferWriter<MaterialType>(
      kChunkTypeMaterialType, 1,
      [this](Context* context, MaterialType* resource,
             FlatBufferBuilder* builder) {
        return SaveMaterialTypeChunk(context, resource, builder);
      });
  resource_writer_->RegisterResourceFlatBufferWriter<Material>(
      kChunkTypeMaterial, 1,
      [this](Context* context, Material* resource, FlatBufferBuilder* builder) {
        return SaveMaterialChunk(context, resource, builder);
      });
  resource_writer_->RegisterResourceFlatBufferWriter<Mesh>(
      kChunkTypeMesh, 1,
      [this](Context* context, Mesh* resource, FlatBufferBuilder* builder) {
        return SaveMeshChunk(context, resource, builder);
      });

  resource_reader_ = ResourceFileReader::Create(context_);
  resource_reader_->RegisterResourceFlatBufferChunk<Texture, fbs::TextureChunk>(
      kChunkTypeTexture, 1,
      [this](Context* context, const fbs::TextureChunk* chunk,
             ResourceEntry entry) {
        return LoadTextureChunk(context, chunk, std::move(entry));
      });
  resource_reader_
      ->RegisterResourceFlatBufferChunk<TextureArray, fbs::TextureArrayChunk>(
          kChunkTypeTextureArray, 1,
          [this](Context* context, const fbs::TextureArrayChunk* chunk,
                 ResourceEntry entry) {
            return LoadTextureArrayChunk(context, chunk, std::move(entry));
          });
  resource_reader_
      ->RegisterResourceFlatBufferChunk<VolumeTexture, fbs::VolumeTextureChunk>(
          kChunkTypeVolumeTexture, 1,
          [this](Context* context, const fbs::VolumeTextureChunk* chunk,
                 ResourceEntry entry) {
            return LoadVolumeTextureChunk(context, chunk, std::move(entry));
          });
  resource_reader_->RegisterResourceFlatBufferChunk<Shader, fbs::ShaderChunk>(
      kChunkTypeShader, 1,
      [this](Context* context, const fbs::ShaderChunk* chunk,
             ResourceEntry entry) {
        return LoadShaderChunk(context, chunk, std::move(entry));
      });
  resource_reader_
      ->RegisterResourceFlatBufferChunk<MaterialType, fbs::MaterialTypeChunk>(
          kChunkTypeMaterialType, 1,
          [this](Context* context, const fbs::MaterialTypeChunk* chunk,
                 ResourceEntry entry) {
            return LoadMaterialTypeChunk(context, chunk, std::move(entry));
          });
  resource_reader_
      ->RegisterResourceFlatBufferChunk<Material, fbs::MaterialChunk>(
          kChunkTypeMaterial, 1,
          [this](Context* context, const fbs::MaterialChunk* chunk,
                 ResourceEntry entry) {
            return LoadMaterialChunk(context, chunk, std::move(entry));
          });
  resource_reader_->RegisterResourceFlatBufferChunk<Mesh, fbs::MeshChunk>(
      kChunkTypeMesh, 1,
      [this](Context* context, const fbs::MeshChunk* chunk,
             ResourceEntry entry) {
        return LoadMeshChunk(context, chunk, std::move(entry));
      });

  resource_manager_ = std::make_unique<ResourceManager>();
  resource_manager_->InitGenericLoader(
      [this](Context* context, TypeKey* type, std::string_view name) {
        return resource_reader_->Read(type, name, context);
      });
  resource_manager_->InitLoader<Texture>(
      [this](Context* context, std::string_view name) {
        return LoadTexture(context, name);
      });

  TypeKey::Get<Texture>()->SetTypeName("Texture");
  TypeKey::Get<TextureArray>()->SetTypeName("TextureArray");
  TypeKey::Get<VolumeTexture>()->SetTypeName("VolumeTexture");
  TypeKey::Get<Shader>()->SetTypeName("Shader");
  TypeKey::Get<MaterialType>()->SetTypeName("MaterialType");
  TypeKey::Get<Material>()->SetTypeName("Material");
  TypeKey::Get<Mesh>()->SetTypeName("Mesh");
  auto* resource_system = context_.GetPtr<ResourceSystem>();
  if (!resource_system->Register<Texture, TextureArray, VolumeTexture, Shader,
                                 MaterialType, Material, Mesh>(
          resource_manager_.get())) {
    return false;
  }
  return true;
}

RenderSystem::~RenderSystem() {
  // Shut down the resource manager first, to allow cleanup to happen.
  resource_manager_.reset();
}

const RenderDataType* RenderSystem::DoRegisterConstantsType(
    std::string_view name, TypeKey* type, size_t size) {
  if (auto it = constants_types_.find(name); it != constants_types_.end()) {
    if (it->second->GetType() != type) {
      LOG(ERROR) << "Constants type " << name
                 << " is already registered with a different type.";
      return nullptr;
    }
    return it->second.get();
  }
  auto& constants_type = constants_types_[name];
  constants_type =
      std::make_unique<RenderDataType>(RenderInternal{}, name, type, size);
  return constants_type.get();
}

const VertexType* RenderSystem::DoRegisterVertexType(
    std::string_view name, TypeKey* type, size_t size,
    absl::Span<const ShaderValue> attributes) {
  if (auto it = vertex_types_.find(name); it != vertex_types_.end()) {
    if (it->second->GetType() != type ||
        !absl::c_equal(it->second->GetAttributes(), attributes)) {
      LOG(ERROR) << "Vertex type " << name
                 << " is already registered with a different type or "
                    "attributes.";
      return nullptr;
    }
    return it->second.get();
  }

  // Validate that the type size matches the expected size based on the
  // attributes.
  size_t expected_size = 0;
  size_t expected_align = 1;
  for (int i = 0; i < attributes.size(); ++i) {
    switch (attributes[i]) {
      case ShaderValue::kInt8:
      case ShaderValue::kUint8:
        expected_size += 1;
        break;
      case ShaderValue::kI8Norm2:
      case ShaderValue::kU8Norm2:
      case ShaderValue::kI8Vec2:
      case ShaderValue::kU8Vec2:
        expected_size += 2;
        break;
      case ShaderValue::kInt16:
      case ShaderValue::kUint16:
        if (expected_size % 2 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 2 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 2);
        expected_size += 2;
        break;
      case ShaderValue::kI8Norm3:
      case ShaderValue::kU8Norm3:
      case ShaderValue::kI8Vec3:
      case ShaderValue::kU8Vec3:
        expected_size += 3;
        break;
      case ShaderValue::kColor:
      case ShaderValue::kI8Norm4:
      case ShaderValue::kU8Norm4:
      case ShaderValue::kI8Vec4:
      case ShaderValue::kU8Vec4:
        expected_size += 4;
        break;
      case ShaderValue::kI16Norm2:
      case ShaderValue::kU16Norm2:
      case ShaderValue::kI16Vec2:
      case ShaderValue::kU16Vec2:
        if (expected_size % 2 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 2 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 2);
        expected_size += 4;
        break;
      case ShaderValue::kFloat:
      case ShaderValue::kInt:
      case ShaderValue::kUint:
        if (expected_size % 4 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 4 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 4);
        expected_size += 4;
        break;
      case ShaderValue::kI16Norm3:
      case ShaderValue::kU16Norm3:
      case ShaderValue::kI16Vec3:
      case ShaderValue::kU16Vec3:
        if (expected_size % 2 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 2 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 2);
        expected_size += 6;
        break;
      case ShaderValue::kI16Norm4:
      case ShaderValue::kU16Norm4:
      case ShaderValue::kI16Vec4:
      case ShaderValue::kU16Vec4:
        if (expected_size % 2 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 2 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 2);
        expected_size += 8;
        break;
      case ShaderValue::kVec2:
      case ShaderValue::kIVec2:
      case ShaderValue::kUVec2:
        if (expected_size % 4 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 4 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 4);
        expected_size += 8;
        break;
      case ShaderValue::kVec3:
      case ShaderValue::kIVec3:
      case ShaderValue::kUVec3:
        if (expected_size % 4 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 4 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 4);
        expected_size += 12;
        break;
      case ShaderValue::kVec4:
      case ShaderValue::kIVec4:
      case ShaderValue::kUVec4:
        if (expected_size % 4 != 0) {
          LOG(ERROR) << "Vertex attribute " << i << " of type "
                     << type->GetTypeName()
                     << " is misaligned (expected alignment of 4 bytes) ";
          return nullptr;
        }
        expected_align = std::max<size_t>(expected_align, 4);
        expected_size += 16;
        break;
      default:
        LOG(FATAL) << "Unhandled shader value type for vertex";
    }
  }
  if (expected_size % expected_align != 0) {
    LOG(ERROR) << "Vertex attributes have an alignment requirement of "
               << expected_align << ", but type " << type->GetTypeName()
               << " has a packed size of " << expected_size;
    return nullptr;
  }
  if (expected_size != size) {
    LOG(ERROR) << "Vertex attributes have a size of " << expected_size
               << ", but type " << type->GetTypeName() << " has a size of "
               << size;
    return nullptr;
  }

  auto& vertex_type = vertex_types_[name];
  vertex_type = std::make_unique<VertexType>(RenderInternal{}, name, type, size,
                                             attributes);
  return vertex_type.get();
}

RenderSceneType* RenderSystem::RegisterSceneType(
    std::string_view name, absl::Span<const Binding> bindings) {
  std::vector<Binding> scene_bindings;
  if (!GetUniqueBindings(bindings, &scene_bindings)) {
    return nullptr;
  }
  if (auto it = scene_types_.find(name); it != scene_types_.end()) {
    if (!absl::c_equal(it->second->GetBindings(), scene_bindings)) {
      LOG(ERROR) << "Scene type " << name
                 << " is already registered with different bindings.";
      return nullptr;
    }
    return it->second.get();
  }

  auto scene_type = backend_->CreateSceneType({}, scene_bindings);
  if (scene_type == nullptr) {
    return nullptr;
  }
  scene_type->SetName({}, name);
  auto& registered_scene_type = scene_types_[name];
  registered_scene_type = std::move(scene_type);
  return registered_scene_type.get();
}

const RenderDataType* RenderSystem::GetConstantsType(
    std::string_view name) const {
  auto it = constants_types_.find(name);
  if (it == constants_types_.end()) {
    return nullptr;
  }
  return it->second.get();
}

const VertexType* RenderSystem::GetVertexType(std::string_view name) const {
  auto it = vertex_types_.find(name);
  if (it == vertex_types_.end()) {
    return nullptr;
  }
  return it->second.get();
}

RenderSceneType* RenderSystem::GetSceneType(std::string_view name) const {
  auto it = scene_types_.find(name);
  if (it == scene_types_.end()) {
    return nullptr;
  }
  return it->second.get();
}

FrameDimensions RenderSystem::GetFrameDimensions() {
  return backend_->GetFrameDimensions({});
}

bool RenderSystem::CanFilter(TextureFormat format) const {
  return GetTexelSize(format) != 0 && backend_->CanFilter({}, format);
}

bool RenderSystem::ValidateTextureFormat(std::string_view name,
                                         TextureFormat format,
                                         const SamplerOptions& options) const {
  if (GetTexelSize(format) == 0) {
    LOG(ERROR) << "Invalid " << name << " format " << static_cast<int>(format);
    return false;
  }
  if (options.filter && !backend_->CanFilter({}, format)) {
    LOG(ERROR) << "Cannot create " << name << " with filtering, as format "
               << static_cast<int>(format) << " cannot be filtered";
    return false;
  }
  return true;
}

Texture* RenderSystem::CreateBackendTexture(std::string_view name,
                                            ResourceEntry entry,
                                            DataVolatility volatility,
                                            int width, int height,
                                            const SamplerOptions& options,
                                            TextureFormat format) {
  if (!IsValidTextureSize(name, width, height) ||
      !ValidateTextureFormat(name, format, options)) {
    return nullptr;
  }
  Texture* texture = backend_->CreateTexture({}, std::move(entry), volatility,
                                             width, height, options, format);
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to create " << name << " of dimensions " << width
               << "x" << height;
  }
  return texture;
}

TextureArray* RenderSystem::CreateBackendTextureArray(
    std::string_view name, ResourceEntry entry, DataVolatility volatility,
    int count, int width, int height, const SamplerOptions& options,
    TextureFormat format) {
  if (!IsValidTextureArraySize(name, count, width, height) ||
      !ValidateTextureFormat(name, format, options)) {
    return nullptr;
  }
  TextureArray* texture_array = backend_->CreateTextureArray(
      {}, std::move(entry), volatility, count, width, height, options, format);
  if (texture_array == nullptr) {
    LOG(ERROR) << "Failed to create " << name << " of count " << count
               << " and dimensions " << width << "x" << height;
  }
  return texture_array;
}

void RenderSystem::SetClearColor(Pixel color) {
  backend_->SetClearColor({}, color);
}

std::unique_ptr<RenderScene> RenderSystem::CreateScene(
    RenderSceneType* scene_type, int scene_order) {
  if (scene_type == nullptr) {
    LOG(ERROR) << "Null scene type passed in to CreateScene";
    return nullptr;
  }
  auto scene = backend_->CreateScene({}, scene_type, scene_order);
  if (scene == nullptr) {
    LOG(ERROR) << "Failed to create scene";
    return nullptr;
  }
  scene_type->GetDefaultSceneBindingData()->CopyTo(
      scene->GetSceneBindingData());
  return scene;
}

Mesh* RenderSystem::DoCreateMesh(const VertexType* vertex_type,
                                 DataVolatility volatility, int max_vertices,
                                 int max_triangles) {
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Null vertex type passed in to CreateMesh";
    return nullptr;
  }

  // Space for at least one triangle is required!
  if (max_vertices < 3 || max_vertices > kMaxMeshVertices) {
    LOG(ERROR) << "Invalid max number of vertices, must be in the range [3, "
               << kMaxMeshVertices
               << "]. Value specified was: " << max_vertices;
    return nullptr;
  }
  if (max_triangles < 1) {
    LOG(ERROR) << "Invalid max number of triangles, must be greater than zero. "
                  "Value specified was: "
               << max_triangles;
    return nullptr;
  }

  auto vertex_buffer = backend_->CreateVertexBuffer(
      {}, volatility, vertex_type->GetSize(), max_vertices);
  if (vertex_buffer == nullptr) {
    LOG(ERROR) << "Failed to create vertex buffer with space for "
               << max_vertices << " vertices.";
    return nullptr;
  }
  auto index_buffer =
      backend_->CreateIndexBuffer({}, volatility, max_triangles * 3);
  if (index_buffer == nullptr) {
    LOG(ERROR) << "Failed to create index buffer with space for "
               << max_triangles << " triangles.";
    return nullptr;
  }

  return new Mesh({}, resource_manager_->NewResourceEntry<Mesh>(), backend_,
                  vertex_type, volatility, std::move(vertex_buffer),
                  std::move(index_buffer));
}

Mesh* RenderSystem::LoadMeshChunk(Context* context, const fbs::MeshChunk* chunk,
                                  ResourceEntry entry) {
  std::string_view vertex_type_name =
      (chunk->vertex_type_name() != nullptr ? chunk->vertex_type_name()->c_str()
                                            : "");
  const VertexType* vertex_type = GetVertexType(vertex_type_name);
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Cannot load mesh because vertex type \"" << vertex_type_name
               << "\" is not registered";
    return nullptr;
  }

  if (chunk->vertices() == nullptr || chunk->indices() == nullptr) {
    LOG(ERROR) << "Mesh is empty";
    return nullptr;
  }
  const int vertex_size = chunk->vertex_size();
  if (vertex_size <= 0 || vertex_size != vertex_type->GetSize()) {
    LOG(ERROR) << "Mesh has mismatched vertex size " << vertex_size
               << " compared to material vertex size of "
               << vertex_type->GetSize();
    return nullptr;
  }
  const int64_t vertex_bytes = chunk->vertices()->size();
  if (vertex_bytes % vertex_size != 0 ||
      vertex_bytes / vertex_size > kMaxMeshVertices) {
    LOG(ERROR) << "Mesh vertex data of " << vertex_bytes
               << " bytes is not a whole number of vertices up to "
               << kMaxMeshVertices;
    return nullptr;
  }
  const int vertex_count = static_cast<int>(vertex_bytes / vertex_size);
  const int index_count = static_cast<int>(chunk->indices()->size());
  if (index_count % 3 != 0) {
    LOG(ERROR) << "Mesh has " << index_count
               << " indices, which is not a whole number of triangles";
    return nullptr;
  }
  for (const uint16_t index : *chunk->indices()) {
    if (index >= vertex_count) {
      LOG(ERROR) << "Mesh index " << index << " is out of range for "
                 << vertex_count << " vertices";
      return nullptr;
    }
  }

  auto volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  auto vertex_buffer =
      backend_->CreateVertexBuffer({}, volatility, vertex_size, vertex_count);
  if (vertex_buffer == nullptr ||
      !vertex_buffer->Set(chunk->vertices()->data(), vertex_count)) {
    LOG(ERROR) << "Failed to initialize vertex buffer when loading mesh";
    return nullptr;
  }
  auto index_buffer = backend_->CreateIndexBuffer({}, volatility, index_count);
  if (index_buffer == nullptr ||
      !index_buffer->Set(chunk->indices()->data(), index_count)) {
    LOG(ERROR) << "Failed to initialize index buffer when loading mesh";
    return nullptr;
  }

  return new Mesh({}, std::move(entry), backend_, vertex_type, volatility,
                  std::move(vertex_buffer), std::move(index_buffer));
}

bool RenderSystem::SaveMesh(std::string_view name, Mesh* mesh,
                            DataVolatility volatility) {
  return resource_writer_->Write(
      name, mesh,
      ContextBuilder().SetValue<DataVolatility>(volatility).Build());
}

bool RenderSystem::SaveMeshChunk(Context* context, Mesh* mesh,
                                 FlatBufferBuilder* builder) {
  if (mesh->GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot save mesh with kStaticWrite volatility.";
    return false;
  }
  auto view = mesh->Edit();
  if (view == nullptr) {
    LOG(ERROR) << "Failed to read mesh in order to save it";
    return false;
  }

  const int32_t vertex_size =
      static_cast<int32_t>(mesh->GetVertexType()->GetSize());
  const auto fb_indices = builder->CreateVector(view->GetIndexData({}),
                                                view->GetTriangleCount() * 3);
  const auto fb_vertices = builder->CreateVector(
      static_cast<const uint8_t*>(view->GetVertexData({})),
      view->GetVertexCount() * vertex_size);
  const auto fb_vertex_type_name =
      builder->CreateSharedString(mesh->GetVertexType()->GetName());

  fbs::MeshChunkBuilder mesh_builder(*builder);
  mesh_builder.add_vertex_type_name(fb_vertex_type_name);
  mesh_builder.add_volatility(ToFbs(context->GetValue<DataVolatility>()));
  mesh_builder.add_vertex_size(vertex_size);
  mesh_builder.add_indices(fb_indices);
  mesh_builder.add_vertices(fb_vertices);
  const auto fb_mesh = mesh_builder.Finish();

  builder->Finish(fb_mesh);
  return true;
}

Material* RenderSystem::DoCreateMaterial(MaterialType* material_type) {
  if (material_type == nullptr) {
    LOG(ERROR) << "Null material type passed to CreateMaterial";
    return nullptr;
  }
  auto material_data =
      material_type->GetPipeline({})->CreateMaterialBindingData();
  if (material_data == nullptr) {
    LOG(ERROR) << "Failed to create material binding data for material";
    return nullptr;
  }

  return new Material({}, resource_manager_->NewResourceEntry<Material>(),
                      material_type, std::move(material_data));
}

Material* RenderSystem::LoadMaterialChunk(Context* context,
                                          const fbs::MaterialChunk* chunk,
                                          ResourceEntry entry) {
  auto* resources = context->GetPtr<FileResources>();
  auto* material_type =
      resources->GetResource<MaterialType>(chunk->material_type_id());
  if (material_type == nullptr) {
    LOG(ERROR) << "Material type (ID: " << chunk->material_type_id()
               << ") not found when loading material";
    return nullptr;
  }
  auto material_data =
      material_type->GetPipeline({})->CreateMaterialBindingData();
  if (material_data == nullptr) {
    LOG(ERROR) << "Failed to create material binding data when loading "
                  "material";
    return nullptr;
  }

  auto* material = new Material({}, std::move(entry), material_type,
                                std::move(material_data));
  const auto bindings = material_type->GetBindings();
  if (!ReadBindingData({}, resources, bindings, chunk->material_data(),
                       material->GetMaterialBindingData()) ||
      !ReadBindingData({}, resources, bindings, chunk->instance_data(),
                       material->GetDefaultInstanceBindingData())) {
    return nullptr;
  }
  return material;
}

bool RenderSystem::SaveMaterial(std::string_view name, Material* material) {
  return resource_writer_->Write(name, material);
}

bool RenderSystem::SaveMaterialChunk(Context* context, Material* material,
                                     FlatBufferBuilder* builder) {
  auto* material_type = material->GetType();
  const auto bindings = material_type->GetBindings();
  const auto fb_material_data =
      WriteBindingData({}, builder, BindingSet::kMaterial, bindings,
                       material->GetMaterialBindingData());
  const auto fb_instance_data =
      WriteBindingData({}, builder, BindingSet::kInstance, bindings,
                       material->GetDefaultInstanceBindingData());

  fbs::MaterialChunkBuilder material_builder(*builder);
  material_builder.add_material_type_id(material_type->GetResourceId());
  material_builder.add_material_data(fb_material_data);
  material_builder.add_instance_data(fb_instance_data);
  auto fb_material = material_builder.Finish();

  builder->Finish(fb_material);
  return true;
}

bool RenderSystem::ValidateMaterialTypeArguments(RenderSceneType* scene_type,
                                                 const VertexType* vertex_type,
                                                 Shader* vertex_shader,
                                                 Shader* fragment_shader) {
  if (vertex_shader->GetType() != ShaderType::kVertex) {
    LOG(ERROR) << "Vertex shader is not the correct shader type";
    return false;
  }
  if (fragment_shader->GetType() != ShaderType::kFragment) {
    LOG(ERROR) << "Fragment shader is not the correct shader type";
    return false;
  }

  // Validate the vertex type matches the vertex shader inputs.
  auto attributes = vertex_type->GetAttributes();
  for (const ShaderParam& input : vertex_shader->GetInputs()) {
    if (input.location < 0) {
      LOG(ERROR) << "Vertex shader has negative input location "
                 << input.location;
      return false;
    }
    if (input.location >= static_cast<int>(attributes.size())) {
      LOG(ERROR) << "Vertex shader requires input location " << input.location
                 << ", but vertex type only has " << attributes.size()
                 << " attributes.";
      return false;
    }
    bool match = false;
    switch (attributes[input.location]) {
      case ShaderValue::kFloat:
        match = (input.value == ShaderValue::kFloat);
        break;
      case ShaderValue::kVec2:
      case ShaderValue::kI8Norm2:
      case ShaderValue::kI16Norm2:
      case ShaderValue::kU8Norm2:
      case ShaderValue::kU16Norm2:
        match = (input.value == ShaderValue::kVec2);
        break;
      case ShaderValue::kVec3:
      case ShaderValue::kI8Norm3:
      case ShaderValue::kI16Norm3:
      case ShaderValue::kU8Norm3:
      case ShaderValue::kU16Norm3:
        match = (input.value == ShaderValue::kVec3);
        break;
      case ShaderValue::kVec4:
      case ShaderValue::kColor:
      case ShaderValue::kI8Norm4:
      case ShaderValue::kI16Norm4:
      case ShaderValue::kU8Norm4:
      case ShaderValue::kU16Norm4:
        match = (input.value == ShaderValue::kVec4);
        break;
      case ShaderValue::kInt8:
      case ShaderValue::kInt16:
      case ShaderValue::kInt:
        match = (input.value == ShaderValue::kInt);
        break;
      case ShaderValue::kI8Vec2:
      case ShaderValue::kI16Vec2:
      case ShaderValue::kIVec2:
        match = (input.value == ShaderValue::kIVec2);
        break;
      case ShaderValue::kI8Vec3:
      case ShaderValue::kI16Vec3:
      case ShaderValue::kIVec3:
        match = (input.value == ShaderValue::kIVec3);
        break;
      case ShaderValue::kI8Vec4:
      case ShaderValue::kI16Vec4:
      case ShaderValue::kIVec4:
        match = (input.value == ShaderValue::kIVec4);
        break;
      case ShaderValue::kUint8:
      case ShaderValue::kUint16:
      case ShaderValue::kUint:
        match = (input.value == ShaderValue::kUint);
        break;
      case ShaderValue::kU8Vec2:
      case ShaderValue::kU16Vec2:
      case ShaderValue::kUVec2:
        match = (input.value == ShaderValue::kUVec2);
        break;
      case ShaderValue::kU8Vec3:
      case ShaderValue::kU16Vec3:
      case ShaderValue::kUVec3:
        match = (input.value == ShaderValue::kUVec3);
        break;
      case ShaderValue::kU8Vec4:
      case ShaderValue::kU16Vec4:
      case ShaderValue::kUVec4:
        match = (input.value == ShaderValue::kUVec4);
        break;
    }
    if (!match) {
      LOG(ERROR) << "Shader type mismatch for vertex input and vertex "
                    "attribute location "
                 << input.location;
      return false;
    }
  }

  // Validate the inputs on the fragment shader have corresponding
  // outputs from the vertex shader.
  for (const ShaderParam& input : fragment_shader->GetInputs()) {
    bool found = false;
    for (const ShaderParam& output : vertex_shader->GetOutputs()) {
      if (output.location == input.location) {
        if (input.value != output.value) {
          LOG(ERROR) << "Shader type mismatch for fragment input and vertex "
                        "output location "
                     << input.location;
          return false;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "Fragment shader input location " << input.location
                 << " not produced by vertex shader";
      return false;
    }
  }

  return true;
}

MaterialType* RenderSystem::DoCreateMaterialType(RenderSceneType* scene_type,
                                                 const VertexType* vertex_type,
                                                 Shader* vertex_shader,
                                                 Shader* fragment_shader,
                                                 const MaterialConfig& config) {
  if (scene_type == nullptr) {
    LOG(ERROR) << "Null scene type passed in to CreateMaterialType";
    return nullptr;
  }
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Null vertex type passed in to CreateMaterialType";
    return nullptr;
  }
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Null vertex shader passed in to CreateMaterialType";
    return nullptr;
  }
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Null fragment shader passed in to CreateMaterialType";
    return nullptr;
  }
  if (!ValidateMaterialTypeArguments(scene_type, vertex_type, vertex_shader,
                                     fragment_shader)) {
    return nullptr;
  }
  std::vector<Binding> bindings;
  if (!GetMaterialTypeBindings(scene_type, vertex_shader, fragment_shader,
                               &bindings)) {
    return nullptr;
  }

  // Create a pipeline for the material.
  auto pipeline = backend_->CreatePipeline({}, scene_type, vertex_type,
                                           bindings, vertex_shader->GetCode(),
                                           fragment_shader->GetCode(), config);
  if (pipeline == nullptr) {
    LOG(ERROR) << "Failed to create pipeline for material type";
    return nullptr;
  }

  auto* material_type =
      new MaterialType({}, resource_manager_->NewResourceEntry<MaterialType>(),
                       scene_type, bindings, std::move(pipeline), vertex_type,
                       vertex_shader, fragment_shader, config);
  scene_type->GetDefaultMaterialBindingData()->CopyTo(
      material_type->GetDefaultMaterialBindingData());
  scene_type->GetDefaultInstanceBindingData()->CopyTo(
      material_type->GetDefaultInstanceBindingData());
  return material_type;
}

MaterialType* RenderSystem::LoadMaterialTypeChunk(
    Context* context, const fbs::MaterialTypeChunk* chunk,
    ResourceEntry entry) {
  auto* resources = context->GetPtr<FileResources>();

  MaterialConfig config = {};
  config.depth_mode = FromFbs(chunk->depth_mode());
  config.depth_test = FromFbs(chunk->depth_test());
  config.raster_mode = FromFbs(chunk->raster_mode());
  config.cull_mode = FromFbs(chunk->cull_mode());
  config.blend_mode = FromFbs(chunk->blend_mode());
  config.color_channels = ColorChannels(chunk->color_channels());
  config.depth_bias_constant = chunk->depth_bias_constant();
  config.depth_bias_slope = chunk->depth_bias_slope();

  std::string_view scene_type_name =
      (chunk->scene_type_name() != nullptr ? chunk->scene_type_name()->c_str()
                                           : "");
  RenderSceneType* scene_type = GetSceneType(scene_type_name);
  if (scene_type == nullptr) {
    LOG(ERROR) << "Cannot load material type because scene type \""
               << scene_type_name << "\" is not registered";
    return nullptr;
  }
  auto* vertex_shader =
      resources->GetResource<Shader>(chunk->vertex_shader_id());
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Cannot load material type because vertex shader (ID: "
               << chunk->vertex_shader_id() << ") is not loaded";
    return nullptr;
  }
  auto* fragment_shader =
      resources->GetResource<Shader>(chunk->fragment_shader_id());
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Cannot load material type because fragment shader (ID: "
               << chunk->fragment_shader_id() << ") is not loaded";
    return nullptr;
  }
  std::string_view vertex_type_name =
      (chunk->vertex_type_name() != nullptr ? chunk->vertex_type_name()->c_str()
                                            : "");
  const VertexType* vertex_type = GetVertexType(vertex_type_name);
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Cannot load material type because vertex type \""
               << vertex_type_name << "\" is not registered";
    return nullptr;
  }

  if (!ValidateMaterialTypeArguments(scene_type, vertex_type, vertex_shader,
                                     fragment_shader)) {
    return nullptr;
  }

  std::vector<Binding> bindings;
  if (!GetMaterialTypeBindings(scene_type, vertex_shader, fragment_shader,
                               &bindings)) {
    return nullptr;
  }

  // Create a pipeline for the material.
  auto pipeline = backend_->CreatePipeline({}, scene_type, vertex_type,
                                           bindings, vertex_shader->GetCode(),
                                           fragment_shader->GetCode(), config);
  if (pipeline == nullptr) {
    LOG(ERROR) << "Failed to create pipeline for material type";
    return nullptr;
  }

  auto material_type = new MaterialType(
      {}, std::move(entry), scene_type, bindings, std::move(pipeline),
      vertex_type, vertex_shader, fragment_shader, config);
  if (!ReadBindingData({}, resources, bindings, chunk->material_data(),
                       material_type->GetDefaultMaterialBindingData()) ||
      !ReadBindingData({}, resources, bindings, chunk->instance_data(),
                       material_type->GetDefaultInstanceBindingData())) {
    return nullptr;
  }
  return material_type;
}

bool RenderSystem::SaveMaterialType(std::string_view name,
                                    MaterialType* material_type) {
  return resource_writer_->Write(name, material_type);
}

bool RenderSystem::SaveMaterialTypeChunk(Context* context,
                                         MaterialType* material_type,
                                         FlatBufferBuilder* builder) {
  const auto bindings = material_type->GetBindings();
  const auto fb_material_data =
      WriteBindingData({}, builder, BindingSet::kMaterial, bindings,
                       material_type->GetDefaultMaterialBindingData());
  const auto fb_instance_data =
      WriteBindingData({}, builder, BindingSet::kInstance, bindings,
                       material_type->GetDefaultInstanceBindingData());
  const auto fb_scene_type_name =
      builder->CreateSharedString(material_type->GetSceneType()->GetName());
  const auto fb_vertex_type_name =
      builder->CreateSharedString(material_type->GetVertexType()->GetName());

  fbs::MaterialTypeChunkBuilder material_type_builder(*builder);
  material_type_builder.add_scene_type_name(fb_scene_type_name);
  material_type_builder.add_vertex_shader_id(
      material_type->GetVertexShader()->GetResourceId());
  material_type_builder.add_fragment_shader_id(
      material_type->GetFragmentShader()->GetResourceId());
  material_type_builder.add_vertex_type_name(fb_vertex_type_name);
  const MaterialConfig& config = material_type->GetConfig();
  material_type_builder.add_cull_mode(ToFbs(config.cull_mode));
  material_type_builder.add_depth_mode(ToFbs(config.depth_mode));
  material_type_builder.add_depth_test(ToFbs(config.depth_test));
  material_type_builder.add_raster_mode(ToFbs(config.raster_mode));
  material_type_builder.add_blend_mode(ToFbs(config.blend_mode));
  material_type_builder.add_color_channels(
      static_cast<uint8_t>(config.color_channels.GetMask()));
  material_type_builder.add_depth_bias_constant(config.depth_bias_constant);
  material_type_builder.add_depth_bias_slope(config.depth_bias_slope);
  material_type_builder.add_material_data(fb_material_data);
  material_type_builder.add_instance_data(fb_instance_data);
  const auto fb_material_type = material_type_builder.Finish();

  builder->Finish(fb_material_type);
  return true;
}

Shader* RenderSystem::DoCreateShader(ShaderType type,
                                     std::unique_ptr<ShaderCode> shader_code,
                                     absl::Span<const Binding> bindings,
                                     absl::Span<const ShaderParam> inputs,
                                     absl::Span<const ShaderParam> outputs) {
  if (shader_code == nullptr) {
    LOG(ERROR) << "Null shader code passed to CreateShader";
    return nullptr;
  }
  std::vector<Binding> shader_bindings;
  if (!GetShaderBindings(type, bindings, &shader_bindings)) {
    return nullptr;
  }

  return new Shader({}, resource_manager_->NewResourceEntry<Shader>(), type,
                    std::move(shader_code), shader_bindings, inputs, outputs);
}

Shader* RenderSystem::LoadShaderChunk(Context* context,
                                      const fbs::ShaderChunk* chunk,
                                      ResourceEntry entry) {
  if (chunk->code() == nullptr) {
    LOG(ERROR) << "Shader does not contain any code";
    return nullptr;
  }

  const ShaderType type = FromFbs(chunk->type());
  if (type != ShaderType::kVertex && type != ShaderType::kFragment) {
    LOG(ERROR) << "Unknown shader type " << static_cast<int>(chunk->type())
               << " for shader";
    return nullptr;
  }
  std::vector<Binding> bindings;
  std::vector<Binding> shader_bindings;
  if (!ReadBindings(this, chunk->bindings(), &bindings) ||
      !GetShaderBindings(type, bindings, &shader_bindings)) {
    return nullptr;
  }

  auto shader_code =
      CreateShaderCode(chunk->code()->data(), chunk->code()->size());
  if (shader_code == nullptr) {
    LOG(ERROR) << "Failed to create shader code for shader";
    return nullptr;
  }

  absl::Span<const ShaderParam> inputs;
  if (chunk->inputs() != nullptr && chunk->inputs()->size() > 0) {
    inputs = absl::MakeConstSpan(chunk->inputs()->GetAs<ShaderParam>(0),
                                 chunk->inputs()->size());
  }

  absl::Span<const ShaderParam> outputs;
  if (chunk->outputs() != nullptr && chunk->outputs()->size() > 0) {
    outputs = absl::MakeConstSpan(chunk->outputs()->GetAs<ShaderParam>(0),
                                  chunk->outputs()->size());
  }

  return new Shader({}, std::move(entry), type, std::move(shader_code),
                    shader_bindings, inputs, outputs);
}

bool RenderSystem::SaveShader(std::string_view name, Shader* shader) {
  return resource_writer_->Write(name, shader);
}

bool RenderSystem::SaveShaderChunk(Context* context, Shader* shader,
                                   FlatBufferBuilder* builder) {
  const auto& code = shader->GetCode()->GetData({});
  if (code.empty()) {
    LOG(ERROR) << "Cannot save shader because shader code is empty (likely the "
                  "render system is not in edit mode)";
    return false;
  }

  static_assert(sizeof(fbs::ShaderParam) == sizeof(ShaderParam));
  const auto fb_bindings = WriteBindings(builder, shader->GetBindings());
  const auto inputs = shader->GetInputs();
  const auto fb_inputs = builder->CreateVectorOfStructs<fbs::ShaderParam>(
      inputs.size(), [&inputs](size_t i, fbs::ShaderParam* out_param) {
        *out_param =
            fbs::ShaderParam(ToFbs(inputs[i].value), inputs[i].location);
      });
  const auto outputs = shader->GetOutputs();
  const auto fb_outputs = builder->CreateVectorOfStructs<fbs::ShaderParam>(
      outputs.size(), [&outputs](size_t i, fbs::ShaderParam* out_param) {
        *out_param =
            fbs::ShaderParam(ToFbs(outputs[i].value), outputs[i].location);
      });
  const auto fb_code = builder->CreateVector(code);

  fbs::ShaderChunkBuilder shader_builder(*builder);
  shader_builder.add_type(ToFbs(shader->GetType()));
  shader_builder.add_bindings(fb_bindings);
  shader_builder.add_inputs(fb_inputs);
  shader_builder.add_outputs(fb_outputs);
  shader_builder.add_code(fb_code);
  const auto fb_shader = shader_builder.Finish();

  builder->Finish(fb_shader);
  return true;
}

std::unique_ptr<ShaderCode> RenderSystem::CreateShaderCode(const void* code,
                                                           int64_t code_size) {
  auto shader_code = backend_->CreateShaderCode({}, code, code_size);
  if (shader_code == nullptr) {
    return nullptr;
  }
  if (edit_) {
    std::vector<uint8_t> buffer(code_size);
    std::memcpy(buffer.data(), code, code_size);
    shader_code->SetData({}, std::move(buffer));
  }
  return shader_code;
}

std::unique_ptr<ShaderCode> RenderSystem::LoadShaderCode(
    std::string_view filename) {
  std::vector<uint8_t> buffer;
  auto file_system = context_.GetPtr<FileSystem>();
  if (!file_system->ReadFile(filename, &buffer)) {
    LOG(ERROR) << "Failed to read file " << filename
               << " when loading shader code";
    return nullptr;
  }
  auto shader_code =
      backend_->CreateShaderCode({}, buffer.data(), buffer.size());
  if (shader_code == nullptr) {
    return nullptr;
  }
  if (edit_) {
    shader_code->SetData({}, std::move(buffer));
  }
  return shader_code;
}

Texture* RenderSystem::DoCreateTexture(DataVolatility volatility, int width,
                                       int height,
                                       const SamplerOptions& options,
                                       TextureFormat format) {
  return CreateBackendTexture("texture",
                              resource_manager_->NewResourceEntry<Texture>(),
                              volatility, width, height, options, format);
}

Texture* RenderSystem::LoadTexture(Context* context, std::string_view name) {
  auto* file_system = context_.GetPtr<FileSystem>();

  auto file = file_system->OpenFile(name, kReadFileFlags);
  if (file == nullptr) {
    LOG(ERROR) << "Could not open texture file: " << name;
    return nullptr;
  }

  ChunkType chunk_type;
  if (file->Read(&chunk_type) != 1) {
    LOG(ERROR) << "Invalid texture file: " << name;
    return nullptr;
  }

  if (chunk_type == kChunkTypeFile) {
    file.reset();
    return resource_reader_->Read<Texture>(name, context);
  }

  file->SeekBegin();
  return LoadTextureFile(context, file.get());
}

Texture* RenderSystem::LoadTextureChunk(Context* context,
                                        const fbs::TextureChunk* chunk,
                                        ResourceEntry entry) {
  const int width = chunk->width();
  const int height = chunk->height();
  const TextureFormat format = FromFbs(chunk->format());
  const int64_t size_in_bytes = GetTextureSizeInBytes(format, width, height);
  const void* texels = GetChunkTexels("texture", chunk->texels(),
                                      chunk->pixels(), size_in_bytes);
  if (texels == nullptr) {
    return nullptr;
  }

  gb::ValidatedContext validated_context = TextureLoadContract(context);
  DCHECK(validated_context.IsValid())
      << "TextureLoadContext does not have any requirements!";
  DataVolatility volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  Texture* texture = CreateBackendTexture(
      "texture", std::move(entry), volatility, width, height,
      validated_context.GetValueOrDefault<SamplerOptions>(
          ReadSamplerOptions(chunk->options())),
      format);
  if (texture == nullptr) {
    return nullptr;
  }
  if (!texture->Set(texels, size_in_bytes)) {
    LOG(ERROR) << "Failed to initialize texture with image data";
    return nullptr;
  }
  return texture;
}

Texture* RenderSystem::LoadTextureFile(TextureLoadContract contract,
                                       File* file) {
  auto image = LoadImage(file);
  if (image == nullptr) {
    LOG(ERROR) << "Failed to read texture file";
    return nullptr;
  }

  gb::ValidatedContext validated_context = std::move(contract);
  DCHECK(validated_context.IsValid())
      << "TextureLoadContext does not have any requirements!";
  Texture* texture = CreateBackendTexture(
      "texture file", resource_manager_->NewResourceEntry<Texture>(),
      (edit_ ? DataVolatility::kStaticReadWrite : DataVolatility::kStaticWrite),
      image->GetWidth(), image->GetHeight(),
      validated_context.GetValue<SamplerOptions>(), TextureFormat::kRgba8Srgb);
  if (texture == nullptr) {
    return nullptr;
  }
  if (!texture->Set(image->GetPixels())) {
    LOG(ERROR) << "Failed to initialize texture with image data";
    resource_manager_->MaybeDeleteResource(texture);
    return nullptr;
  }
  return texture;
}

bool RenderSystem::SaveTexture(std::string_view name, Texture* texture,
                               DataVolatility volatility) {
  return resource_writer_->Write(
      name, texture,
      ContextBuilder().SetValue<DataVolatility>(volatility).Build());
}

bool RenderSystem::SaveTextureChunk(Context* context, Texture* texture,
                                    FlatBufferBuilder* builder) {
  if (texture->GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot save texture with kStaticWrite volatility.";
    return false;
  }

  // The texels are read straight into the builder, which must be done before
  // anything else is built, as building moves its buffer.
  const int64_t size_in_bytes = GetTextureSizeInBytes(
      texture->GetFormat(), texture->GetWidth(), texture->GetHeight());
  uint8_t* texels = nullptr;
  const auto fb_texels =
      builder->CreateUninitializedVector(size_in_bytes, &texels);
  if (!texture->Get(texels, size_in_bytes)) {
    LOG(ERROR) << "Failed to read texture in order to save it";
    return false;
  }
  const auto fb_sampler_options =
      WriteSamplerOptions(builder, texture->GetSamplerOptions());

  fbs::TextureChunkBuilder texture_builder(*builder);
  texture_builder.add_volatility(ToFbs(context->GetValue<DataVolatility>()));
  texture_builder.add_width(texture->GetWidth());
  texture_builder.add_height(texture->GetHeight());
  texture_builder.add_options(fb_sampler_options);
  texture_builder.add_format(ToFbs(texture->GetFormat()));
  texture_builder.add_texels(fb_texels);
  const auto fb_texture = texture_builder.Finish();

  builder->Finish(fb_texture);
  return true;
}

TextureArray* RenderSystem::DoCreateTextureArray(DataVolatility volatility,
                                                 int count, int width,
                                                 int height,
                                                 const SamplerOptions& options,
                                                 TextureFormat format) {
  return CreateBackendTextureArray(
      "texture array", resource_manager_->NewResourceEntry<TextureArray>(),
      volatility, count, width, height, options, format);
}

TextureArray* RenderSystem::LoadTextureArrayChunk(
    Context* context, const fbs::TextureArrayChunk* chunk,
    ResourceEntry entry) {
  const int count = chunk->count();
  const int width = chunk->width();
  const int height = chunk->height();
  const TextureFormat format = FromFbs(chunk->format());
  const int64_t texture_size = GetTextureSizeInBytes(format, width, height);
  const uint8_t* texels = static_cast<const uint8_t*>(GetChunkTexels(
      "texture array", chunk->texels(), chunk->pixels(), texture_size * count));
  if (texels == nullptr) {
    return nullptr;
  }

  gb::ValidatedContext validated_context = TextureLoadContract(context);
  DCHECK(validated_context.IsValid())
      << "TextureLoadContext does not have any requirements!";
  DataVolatility volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  TextureArray* texture_array = CreateBackendTextureArray(
      "texture array", std::move(entry), volatility, count, width, height,
      validated_context.GetValueOrDefault<SamplerOptions>(
          ReadSamplerOptions(chunk->options())),
      format);
  if (texture_array == nullptr) {
    return nullptr;
  }
  for (int i = 0; i < count; ++i) {
    if (!texture_array->Set(i, texels + i * texture_size, texture_size)) {
      LOG(ERROR)
          << "Failed to initialize texture array with image data at index: "
          << i;
      return nullptr;
    }
  }
  return texture_array;
}

bool RenderSystem::SaveTextureArray(std::string_view name,
                                    TextureArray* texture_array,
                                    DataVolatility volatility) {
  return resource_writer_->Write(
      name, texture_array,
      ContextBuilder().SetValue<DataVolatility>(volatility).Build());
}

bool RenderSystem::SaveTextureArrayChunk(
    Context* context, TextureArray* texture_array,
    flatbuffers::FlatBufferBuilder* builder) {
  if (texture_array->GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot save texture array with kStaticWrite volatility.";
    return false;
  }

  // The texels are read straight into the builder, which must be done before
  // anything else is built, as building moves its buffer.
  const int64_t texture_size = GetTextureSizeInBytes(
      texture_array->GetFormat(), texture_array->GetWidth(),
      texture_array->GetHeight());
  uint8_t* texels = nullptr;
  const auto fb_texels = builder->CreateUninitializedVector(
      texture_size * texture_array->GetCount(), &texels);
  for (int i = 0; i < texture_array->GetCount(); ++i) {
    if (!texture_array->Get(i, texels + i * texture_size, texture_size)) {
      LOG(ERROR) << "Failed to read texture " << i
                 << " from texture array in order to save it";
      return false;
    }
  }
  const auto fb_sampler_options =
      WriteSamplerOptions(builder, texture_array->GetSamplerOptions());

  fbs::TextureArrayChunkBuilder texture_array_builder(*builder);
  texture_array_builder.add_volatility(
      ToFbs(context->GetValue<DataVolatility>()));
  texture_array_builder.add_count(texture_array->GetCount());
  texture_array_builder.add_width(texture_array->GetWidth());
  texture_array_builder.add_height(texture_array->GetHeight());
  texture_array_builder.add_options(fb_sampler_options);
  texture_array_builder.add_format(ToFbs(texture_array->GetFormat()));
  texture_array_builder.add_texels(fb_texels);
  const auto fb_texture_array = texture_array_builder.Finish();

  builder->Finish(fb_texture_array);
  return true;
}

VolumeTexture* RenderSystem::CreateBackendVolumeTexture(
    ResourceEntry entry, DataVolatility volatility, int width, int height,
    int depth, const SamplerOptions& options, TextureFormat format) {
  if (!ValidateVolumeTextureSize(width, height, depth) ||
      !ValidateTextureFormat("volume texture", format, options)) {
    return nullptr;
  }
  if (options.mipmap || options.tile_size != 0) {
    LOG(ERROR) << "Invalid volume texture sampler options, as volume textures "
                  "have no mipmaps (mipmap must be false, and tile_size zero)";
    return nullptr;
  }
  VolumeTexture* volume_texture = backend_->CreateVolumeTexture(
      {}, std::move(entry), volatility, width, height, depth, options, format);
  if (volume_texture == nullptr) {
    LOG(ERROR) << "Failed to create volume texture of dimensions " << width
               << "x" << height << "x" << depth;
  }
  return volume_texture;
}

VolumeTexture* RenderSystem::DoCreateVolumeTexture(
    DataVolatility volatility, int width, int height, int depth,
    const SamplerOptions& options, TextureFormat format) {
  return CreateBackendVolumeTexture(
      resource_manager_->NewResourceEntry<VolumeTexture>(), volatility, width,
      height, depth, options, format);
}

VolumeTexture* RenderSystem::LoadVolumeTextureChunk(
    Context* context, const fbs::VolumeTextureChunk* chunk,
    ResourceEntry entry) {
  const int width = chunk->width();
  const int height = chunk->height();
  const int depth = chunk->depth();
  const TextureFormat format = FromFbs(chunk->format());
  const int64_t size_in_bytes =
      GetTextureSizeInBytes(format, width, height, depth);
  const void* texels =
      GetChunkTexels("volume texture", chunk->texels(), nullptr, size_in_bytes);
  if (texels == nullptr) {
    return nullptr;
  }

  gb::ValidatedContext validated_context = TextureLoadContract(context);
  DCHECK(validated_context.IsValid())
      << "TextureLoadContext does not have any requirements!";
  SamplerOptions sampler_options =
      validated_context.GetValueOrDefault<SamplerOptions>(
          ReadSamplerOptions(chunk->options()));

  // Sampler options given in the load context are for every texture loaded
  // with it, so their mipmap options, which volume textures don't have, are
  // ignored.
  sampler_options.SetMipmap(false).SetTileSize(0);

  DataVolatility volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  VolumeTexture* volume_texture =
      CreateBackendVolumeTexture(std::move(entry), volatility, width, height,
                                 depth, sampler_options, format);
  if (volume_texture == nullptr) {
    return nullptr;
  }
  if (!volume_texture->Set(texels, size_in_bytes)) {
    LOG(ERROR) << "Failed to initialize volume texture with its texels";
    resource_manager_->MaybeDeleteResource(volume_texture);
    return nullptr;
  }
  return volume_texture;
}

bool RenderSystem::SaveVolumeTexture(std::string_view name,
                                     VolumeTexture* volume_texture,
                                     DataVolatility volatility) {
  return resource_writer_->Write(
      name, volume_texture,
      ContextBuilder().SetValue<DataVolatility>(volatility).Build());
}

bool RenderSystem::SaveVolumeTextureChunk(
    Context* context, VolumeTexture* volume_texture,
    flatbuffers::FlatBufferBuilder* builder) {
  if (volume_texture->GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot save volume texture with kStaticWrite volatility.";
    return false;
  }

  // The texels are read straight into the builder, which must be done before
  // anything else is built, as building moves its buffer.
  const int64_t size_in_bytes = GetTextureSizeInBytes(
      volume_texture->GetFormat(), volume_texture->GetWidth(),
      volume_texture->GetHeight(), volume_texture->GetDepth());
  uint8_t* texels = nullptr;
  const auto fb_texels =
      builder->CreateUninitializedVector(size_in_bytes, &texels);
  if (!volume_texture->Get(texels, size_in_bytes)) {
    LOG(ERROR) << "Failed to read volume texture in order to save it";
    return false;
  }
  const auto fb_sampler_options =
      WriteSamplerOptions(builder, volume_texture->GetSamplerOptions());

  fbs::VolumeTextureChunkBuilder volume_texture_builder(*builder);
  volume_texture_builder.add_volatility(
      ToFbs(context->GetValue<DataVolatility>()));
  volume_texture_builder.add_width(volume_texture->GetWidth());
  volume_texture_builder.add_height(volume_texture->GetHeight());
  volume_texture_builder.add_depth(volume_texture->GetDepth());
  volume_texture_builder.add_options(fb_sampler_options);
  volume_texture_builder.add_format(ToFbs(volume_texture->GetFormat()));
  volume_texture_builder.add_texels(fb_texels);
  const auto fb_volume_texture = volume_texture_builder.Finish();

  builder->Finish(fb_volume_texture);
  return true;
}

bool RenderSystem::BeginFrame() {
  if (is_rendering_) {
    LOG(ERROR) << "Already rendering a frame";
    return false;
  }

  if (!backend_->BeginFrame({})) {
    return false;
  }

  is_rendering_ = true;
  return true;
}

void RenderSystem::Draw(RenderScene* scene, Mesh* mesh, Material* material,
                        BindingData* instance_data) {
  RENDER_ASSERT(is_rendering_);
  RENDER_ASSERT(scene != nullptr && mesh != nullptr && material != nullptr &&
                instance_data != nullptr);
  MaterialType* const material_type = material->GetType();
  RENDER_ASSERT(mesh->GetVertexType() == material_type->GetVertexType());
  RenderPipeline* const pipeline = material_type->GetPipeline({});
  RENDER_ASSERT(instance_data->GetPipeline({}) == pipeline);
  RENDER_ASSERT(scene->GetType() == material_type->GetSceneType());
  backend_->Draw({}, scene, pipeline, material->GetMaterialBindingData(),
                 instance_data, mesh->GetVertexBuffer({}),
                 mesh->GetIndexBuffer({}));
}

void RenderSystem::Draw(RenderScene* scene, const DrawList& commands) {
  RENDER_ASSERT(is_rendering_);
  RENDER_ASSERT(scene != nullptr);
  const auto& command_list = commands.GetCommands({});
  if (command_list.empty()) {
    return;
  }
  backend_->Draw({}, scene, command_list);
}

void RenderSystem::EndFrame() {
  if (!is_rendering_) {
    LOG(ERROR) << "Not rendering when EndFrame called";
    return;
  }

  // Draw the frame.
  backend_->EndFrame({});

  is_rendering_ = false;
}

}  // namespace gb
