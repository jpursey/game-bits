// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_system.h"

#include <limits>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "gb/base/context_builder.h"
#include "gb/base/scoped_call.h"
#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "gb/image/image_file.h"
#include "gb/render/render_backend.h"
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

std::vector<Binding> ReadBindings(
    RenderSystem* render_system,
    const fb::Vector<fb::Offset<fbs::Binding>>* fb_bindings) {
  if (fb_bindings == nullptr) {
    return {};
  }
  const int count = static_cast<int>(fb_bindings->size());
  std::vector<Binding> bindings(count);
  for (int i = 0; i < count; ++i) {
    auto* fb_binding = fb_bindings->Get(i);
    if (fb_binding == nullptr) {
      LOG(ERROR) << "Null binding at index " << i;
      continue;
    }
    bindings[i].SetShaders(ShaderTypes(fb_binding->shaders()));
    bindings[i].SetLocation(FromFbs(fb_binding->set()), fb_binding->index());
    switch (fb_binding->type()) {
      case fbs::BindingType_Texture:
        bindings[i].SetTexture();
        break;
      case fbs::BindingType_TextureArray:
        bindings[i].SetTextureArray();
        break;
      case fbs::BindingType_Constants:
        if (fb_binding->constants_name() == nullptr) {
          LOG(ERROR) << "Unspecified constants name for binding";
        } else {
          bindings[i].SetConstants(render_system->GetConstantsType(
                                       fb_binding->constants_name()->c_str()),
                                   FromFbs(fb_binding->volatility()));
        }
        break;
      default:
        LOG(ERROR) << "Unhandled binding type: "
                   << EnumNameBindingType(fb_binding->type());
    }
  }
  return bindings;
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
    binding_entry_builder.add_constants_data(fb_constants_data);
    fb_binding_data.emplace_back(binding_entry_builder.Finish());
  }
  if (fb_binding_data.empty()) {
    return {};
  }
  return builder->CreateVector(fb_binding_data);
}

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
      continue;
    }
    const auto index = fb_binding_data_entry->index();
    switch (fb_binding_data_entry->type()) {
      case fbs::BindingType_Texture: {
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
      case fbs::BindingType_TextureArray: {
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
      case fbs::BindingType_Constants: {
        const auto* fb_constants_data = fb_binding_data_entry->constants_data();
        if (fb_constants_data == nullptr) {
          // No constants data is valid. This results in default values.
          break;
        }
        TypeKey* constants_type = nullptr;
        for (const auto& binding : bindings) {
          if (binding.index == index) {
            if (binding.constants_type->GetSize() !=
                fb_constants_data->size()) {
              LOG(ERROR) << "Constants size " << fb_constants_data->size()
                         << "does not match expected size "
                         << binding.constants_type->GetSize() << " at index "
                         << index << " for binding data";
            }
            constants_type = binding.constants_type->GetType();
            break;
          }
        }
        if (constants_type == nullptr) {
          LOG(ERROR) << "Unknown constants type at index " << index
                     << " for binding data";
          return false;
        }
        binding_data->SetInternal(access_token, index, constants_type,
                                  fb_constants_data->data());
      } break;
      default:
        LOG(ERROR) << "Unhandled binding type "
                   << EnumNameBindingType(fb_binding_data_entry->type())
                   << " at binding data index " << i;
        return false;
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
  TypeKey::Get<Shader>()->SetTypeName("Shader");
  TypeKey::Get<MaterialType>()->SetTypeName("MaterialType");
  TypeKey::Get<Material>()->SetTypeName("Material");
  TypeKey::Get<Mesh>()->SetTypeName("Mesh");
  auto* resource_system = context_.GetPtr<ResourceSystem>();
  if (!resource_system->Register<Texture, TextureArray, Shader, MaterialType,
                                 Material, Mesh>(resource_manager_.get())) {
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
  auto [it, added] = constants_types_.try_emplace(
      absl::StrCat(name),
      std::make_unique<RenderDataType>(RenderInternal{}, name, type, size));
  if (!added) {
    LOG(ERROR) << "Constants type " << name << " is already registered.";
    return nullptr;
  }
  return it->second.get();
}

const VertexType* RenderSystem::DoRegisterVertexType(
    std::string_view name, TypeKey* type, size_t size,
    absl::Span<const ShaderValue> attributes) {
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

  auto [it, added] = vertex_types_.try_emplace(
      absl::StrCat(name), std::make_unique<VertexType>(RenderInternal{}, name,
                                                       type, size, attributes));
  if (!added) {
    LOG(ERROR) << "Constants type " << name << " is already registered.";
    return nullptr;
  }
  return it->second.get();
}

RenderSceneType* RenderSystem::RegisterSceneType(
    std::string_view name, absl::Span<const Binding> bindings) {
  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  std::vector<Binding> all_bindings;

  // Verify the bindings.
  for (const Binding& binding : bindings) {
    if (!binding.IsValid()) {
      LOG(ERROR) << "Invalid binding: set=" << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }

    auto [it, added] =
        mapped_bindings.try_emplace({binding.set, binding.index}, binding);
    if (!added) {
      if (binding == it->second) {
        continue;
      }
      LOG(ERROR) << "Duplicate incompatible binding: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }

    all_bindings.push_back(binding);
  }

  auto scene_type = backend_->CreateSceneType({}, all_bindings);
  if (scene_type == nullptr) {
    return nullptr;
  }
  scene_type->SetName({}, name);
  auto [it, added] =
      scene_types_.try_emplace(absl::StrCat(name), std::move(scene_type));
  if (!added) {
    LOG(ERROR) << "Scene type " << name << " is already registered.";
    return nullptr;
  }
  return it->second.get();
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

void RenderSystem::SetClearColor(Pixel color) {
  backend_->SetClearColor({}, color);
}

std::unique_ptr<RenderScene> RenderSystem::CreateScene(
    RenderSceneType* scene_type, int scene_order) {
  return backend_->CreateScene({}, scene_type, scene_order);
}

Mesh* RenderSystem::DoCreateMesh(const VertexType* vertex_type,
                                 DataVolatility volatility, int max_vertices,
                                 int max_triangles) {
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Null vertex type passed in to CreateMesh";
    return nullptr;
  }

  // Space for at least one triangle is required!
  if (max_vertices < 3 || max_vertices > std::numeric_limits<uint16_t>::max()) {
    LOG(ERROR) << "Invalid max number of vertices, must be in the range [3, "
                  "65535]. Value specified was: "
               << max_vertices;
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
  auto* resources = context->GetPtr<FileResources>();

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
  if (chunk->vertex_size() == 0 ||
      chunk->vertex_size() != vertex_type->GetSize()) {
    LOG(ERROR) << "Mesh has mismatched vertex size " << chunk->vertex_size()
               << " compared to material vertex size of "
               << vertex_type->GetSize();
    return nullptr;
  }
  const int32_t vertex_count =
      static_cast<int32_t>(chunk->vertices()->size()) / chunk->vertex_size();

  auto volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  auto vertex_buffer = backend_->CreateVertexBuffer(
      {}, volatility, chunk->vertex_size(), vertex_count);
  if (vertex_buffer == nullptr ||
      !vertex_buffer->Set(chunk->vertices()->data(), vertex_count)) {
    LOG(ERROR) << "Failed to initialize vertex buffer when loading mesh";
    return nullptr;
  }
  const int32_t index_count = static_cast<int32_t>(chunk->indices()->size());
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

  return new Material({}, resource_manager_->NewResourceEntry<Material>(),
                      material_type);
}

Material* RenderSystem::LoadMaterialChunk(Context* context,
                                          const fbs::MaterialChunk* chunk,
                                          ResourceEntry entry) {
  // Get the material type
  auto* resources = context->GetPtr<FileResources>();
  auto* material_type =
      resources->GetResource<MaterialType>(chunk->material_type_id());
  if (material_type == nullptr) {
    LOG(ERROR) << "Material type (ID: " << chunk->material_type_id()
               << ") not found when loading material";
    return nullptr;
  }

  // Validate all material bindings are compatible with the the material type.
  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  for (const auto& binding : material_type->GetBindings()) {
    mapped_bindings[{binding.set, binding.index}] = binding;
  }
  auto bindings = ReadBindings(this, chunk->bindings());
  for (const Binding& binding : bindings) {
    if (!mapped_bindings.contains({binding.set, binding.index})) {
      LOG(ERROR) << "Material binding not found in loaded material type";
      return nullptr;
    }
  }

  auto* material = new Material({}, std::move(entry), material_type);

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
  const auto fb_bindings = WriteBindings(builder, bindings);
  const auto fb_material_data =
      WriteBindingData({}, builder, BindingSet::kMaterial, bindings,
                       material->GetMaterialBindingData());
  const auto fb_instance_data =
      WriteBindingData({}, builder, BindingSet::kInstance, bindings,
                       material->GetDefaultInstanceBindingData());

  fbs::MaterialChunkBuilder material_builder(*builder);
  material_builder.add_material_type_id(material_type->GetResourceId());
  material_builder.add_bindings(fb_bindings);
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

  // Validate all shader bindings are compatible with the the scene and
  // each other.
  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  std::vector<Binding> all_bindings;
  for (const Binding& binding : scene_type->GetBindings()) {
    mapped_bindings.try_emplace({binding.set, binding.index}, binding);
    all_bindings.push_back(binding);
  }
  for (const Binding& binding : vertex_shader->GetBindings()) {
    auto [it, added] =
        mapped_bindings.try_emplace({binding.set, binding.index}, binding);
    if (!added) {
      if (it->second.Combine(binding)) {
        continue;
      }
      LOG(ERROR) << "Vertex shader contains incompatible binding with "
                    "scene: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }
    all_bindings.push_back(binding);
  }
  for (const Binding& binding : fragment_shader->GetBindings()) {
    auto [it, added] =
        mapped_bindings.try_emplace({binding.set, binding.index}, binding);
    if (!added) {
      if (it->second.Combine(binding)) {
        continue;
      }
      LOG(ERROR) << "Fragment shader contains incompatible binding with "
                    "scene: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }
    all_bindings.push_back(binding);
  }

  // Create a pipeline for the material.
  auto pipeline = backend_->CreatePipeline(
      {}, scene_type, vertex_type, all_bindings, vertex_shader->GetCode(),
      fragment_shader->GetCode(), config);
  if (pipeline == nullptr) {
    LOG(ERROR) << "Failed to create pipeline for material type";
    return nullptr;
  }

  return new MaterialType({},
                          resource_manager_->NewResourceEntry<MaterialType>(),
                          scene_type, all_bindings, std::move(pipeline),
                          vertex_type, vertex_shader, fragment_shader, config);
}

MaterialType* RenderSystem::LoadMaterialTypeChunk(
    Context* context, const fbs::MaterialTypeChunk* chunk,
    ResourceEntry entry) {
  auto* resources = context->GetPtr<FileResources>();

  MaterialConfig config = {};
  config.cull_mode = FromFbs(chunk->cull_mode());
  config.depth_mode = FromFbs(chunk->depth_mode());

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
    LOG(ERROR) << "Cannot load material type becayse vertex shader (ID: "
               << chunk->vertex_shader_id() << ") is not loaded";
    return nullptr;
  }
  auto* fragment_shader =
      resources->GetResource<Shader>(chunk->fragment_shader_id());
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Cannot load material type becayse fragment shader (ID: "
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

  // Validate all shader bindings are compatible with the the scene and
  // each other.
  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  auto bindings = ReadBindings(this, chunk->bindings());
  for (const auto& binding : bindings) {
    mapped_bindings[{binding.set, binding.index}] = binding;
  }
  for (const Binding& binding : scene_type->GetBindings()) {
    if (!mapped_bindings.contains({binding.set, binding.index})) {
      LOG(ERROR) << "Scene binding not found in loaded material type";
      return nullptr;
    }
  }
  for (const Binding& binding : vertex_shader->GetBindings()) {
    if (!mapped_bindings.contains({binding.set, binding.index})) {
      LOG(ERROR) << "Vertex shader binding not found in loaded material type";
      return nullptr;
    }
  }
  for (const Binding& binding : fragment_shader->GetBindings()) {
    if (!mapped_bindings.contains({binding.set, binding.index})) {
      LOG(ERROR) << "Fragment shader binding not found in loaded material type";
      return nullptr;
    }
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
  const auto fb_bindings = WriteBindings(builder, bindings);
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
  material_type_builder.add_bindings(fb_bindings);
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
    LOG(ERROR) << "Null shader code bassed to CreateShader";
    return nullptr;
  }

  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  std::vector<Binding> all_bindings;
  for (const Binding& binding : bindings) {
    if (!binding.IsValid()) {
      LOG(ERROR) << "Invalid binding: set=" << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }
    if (!binding.shader_types.IsSet(type)) {
      LOG(ERROR) << "Invalid shader type for binding: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }

    auto [it, added] =
        mapped_bindings.try_emplace({binding.set, binding.index}, binding);
    if (!added) {
      if (binding == it->second) {
        continue;
      }
      LOG(ERROR) << "Duplicate incompatible binding: set="
                 << static_cast<int>(binding.set)
                 << ", index=" << binding.index;
      return nullptr;
    }

    all_bindings.push_back(binding);
  }

  return new Shader({}, resource_manager_->NewResourceEntry<Shader>(), type,
                    std::move(shader_code), all_bindings, inputs, outputs);
}

Shader* RenderSystem::LoadShaderChunk(Context* context,
                                      const fbs::ShaderChunk* chunk,
                                      ResourceEntry entry) {
  if (chunk->code() == nullptr) {
    LOG(ERROR) << "Shader does not contain any code";
    return nullptr;
  }

  auto shader_code = backend_->CreateShaderCode({}, chunk->code()->data(),
                                                chunk->code()->size());
  if (shader_code == nullptr) {
    LOG(ERROR) << "Failed to create shader code for shader";
    return nullptr;
  }

  std::vector<Binding> all_bindings = ReadBindings(this, chunk->bindings());

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

  return new Shader({}, std::move(entry), FromFbs(chunk->type()),
                    std::move(shader_code), all_bindings, inputs, outputs);
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
                                       const SamplerOptions& options) {
  if (width <= 0 || width > kMaxTextureWidth || height <= 0 ||
      height > kMaxTextureHeight) {
    LOG(ERROR) << "Invalid texture dimensions in CreateTexture: " << width
               << " by " << height;
    return nullptr;
  }
  return backend_->CreateTexture({},
                                 resource_manager_->NewResourceEntry<Texture>(),
                                 volatility, width, height, options);
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
  const auto width = chunk->width();
  const auto height = chunk->height();
  if (width <= 0 || height <= 0 || width >= kMaxTextureWidth ||
      height >= kMaxTextureHeight) {
    LOG(ERROR) << "Invalid texture dimensions in texture file: " << width
               << " by " << height;
    return nullptr;
  }

  gb::ValidatedContext validated_context = TextureLoadContract(context);
  DCHECK(validated_context.IsValid())
      << "TextureLoadContext does not have any requirements!";
  SamplerOptions sampler_options;
  auto* fb_sampler_options = chunk->options();
  if (fb_sampler_options != nullptr) {
    sampler_options.SetFilter(fb_sampler_options->filter());
    sampler_options.SetMipmap(fb_sampler_options->mipmap());
    sampler_options.SetTileSize(fb_sampler_options->tile_size());
    sampler_options.SetAddressMode(FromFbs(fb_sampler_options->address_mode()),
                                   Pixel(fb_sampler_options->border()));
  }

  DataVolatility volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  Texture* texture = backend_->CreateTexture(
      {}, std::move(entry), volatility, width, height,
      validated_context.GetValueOrDefault<SamplerOptions>(sampler_options));
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to create texture of dimensions " << width << "x"
               << height;
    return nullptr;
  }
  const auto* pixels = chunk->pixels();
  if (pixels == nullptr || static_cast<int>(pixels->size()) < width * height) {
    LOG(ERROR) << "Failed to create texture as it contains "
               << (pixels != nullptr ? pixels->size() : 0) << " pixels, but "
               << width * height << " are required.";
    return nullptr;
  }
  if (!texture->Set(pixels->data(), width * height * sizeof(Pixel))) {
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

  Texture* texture = backend_->CreateTexture(
      {}, resource_manager_->NewResourceEntry<Texture>(),
      (edit_ ? DataVolatility::kStaticReadWrite : DataVolatility::kStaticWrite),
      image->GetWidth(), image->GetHeight(),
      validated_context.GetValue<SamplerOptions>());
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to create texture of dimensions " << image->GetWidth()
               << "x" << image->GetHeight();
    return nullptr;
  }
  if (!texture->Set(image->GetPixels())) {
    LOG(ERROR) << "Failed to initialize texture with image data";
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
  auto view = texture->Edit();
  if (view == nullptr) {
    LOG(ERROR) << "Failed to read texture in order to save it";
    return false;
  }
  auto& sampler_options = texture->GetSamplerOptions();

  fbs::SamplerOptionsBuilder sampler_options_builder(*builder);
  sampler_options_builder.add_filter(sampler_options.filter);
  sampler_options_builder.add_mipmap(sampler_options.mipmap);
  sampler_options_builder.add_border(sampler_options.border.Packed());
  sampler_options_builder.add_tile_size(sampler_options.tile_size);
  sampler_options_builder.add_address_mode(ToFbs(sampler_options.address_mode));
  const auto fb_sampler_options = sampler_options_builder.Finish();

  const auto packed_pixels = view->GetPackedPixels();
  const auto fb_pixels = builder->CreateVector<uint32_t>(packed_pixels.data(),
                                                         packed_pixels.size());

  fbs::TextureChunkBuilder texture_builder(*builder);
  texture_builder.add_volatility(ToFbs(context->GetValue<DataVolatility>()));
  texture_builder.add_width(texture->GetWidth());
  texture_builder.add_height(texture->GetHeight());
  texture_builder.add_options(fb_sampler_options);
  texture_builder.add_pixels(fb_pixels);
  const auto fb_texture = texture_builder.Finish();

  builder->Finish(fb_texture);
  return true;
}

TextureArray* RenderSystem::DoCreateTextureArray(
    DataVolatility volatility, int count, int width, int height,
    const SamplerOptions& options) {
  if (count <= 0 || count > kMaxTextureArrayCount) {
    LOG(ERROR) << "Invalid texture array count in CreateTextureArray: "
               << count;
    return nullptr;
  }
  if (width <= 0 || width > kMaxTextureWidth || height <= 0 ||
      height > kMaxTextureHeight) {
    LOG(ERROR) << "Invalid texture dimensions in CreateTextureArray: " << width
               << " by " << height;
    return nullptr;
  }
  const uint64_t total_pixel_count =
      static_cast<uint64_t>(count) * static_cast<uint64_t>(width * height);
  if (total_pixel_count > static_cast<uint64_t>(kMaxTextureArrayPixels)) {
    LOG(ERROR) << "Texture array is too large with " << total_pixel_count
               << "pixels. Maximum is " << kMaxTextureArrayPixels << ".";
    return nullptr;
  }
  return backend_->CreateTextureArray(
      {}, resource_manager_->NewResourceEntry<TextureArray>(), volatility,
      count, width, height, options);
}

TextureArray* RenderSystem::LoadTextureArrayChunk(
    Context* context, const fbs::TextureArrayChunk* chunk,
    ResourceEntry entry) {
  const auto count = chunk->count();
  if (count <= 0 || count > kMaxTextureArrayCount) {
    LOG(ERROR) << "Invalid texture count in texture array: " << count;
  }
  const auto width = chunk->width();
  const auto height = chunk->height();
  if (width <= 0 || height <= 0 || width >= kMaxTextureWidth ||
      height >= kMaxTextureHeight) {
    LOG(ERROR) << "Invalid texture dimensions in texture array: " << width
               << " by " << height;
    return nullptr;
  }

  const uint64_t total_pixel_count =
      static_cast<uint64_t>(count) * static_cast<uint64_t>(width * height);
  if (total_pixel_count > static_cast<uint64_t>(kMaxTextureArrayPixels)) {
    LOG(ERROR) << "Texture array is too large with " << total_pixel_count
               << "pixels. Maximum is " << kMaxTextureArrayPixels << ".";
  }

  gb::ValidatedContext validated_context = TextureLoadContract(context);
  DCHECK(validated_context.IsValid())
      << "TextureLoadContext does not have any requirements!";
  SamplerOptions sampler_options;
  auto* fb_sampler_options = chunk->options();
  if (fb_sampler_options != nullptr) {
    sampler_options.SetFilter(fb_sampler_options->filter());
    sampler_options.SetMipmap(fb_sampler_options->mipmap());
    sampler_options.SetTileSize(fb_sampler_options->tile_size());
    sampler_options.SetAddressMode(FromFbs(fb_sampler_options->address_mode()),
                                   Pixel(fb_sampler_options->border()));
  }

  DataVolatility volatility = FromFbs(chunk->volatility());
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  TextureArray* texture_array = backend_->CreateTextureArray(
      {}, std::move(entry), volatility, count, width, height,
      validated_context.GetValueOrDefault<SamplerOptions>(sampler_options));
  if (texture_array == nullptr) {
    LOG(ERROR) << "Failed to create texture array of count " << count
               << " and dimensions " << width << "x" << height;
    return nullptr;
  }
  const auto* pixels = chunk->pixels();
  const int image_pixel_count = width * height;
  if (pixels == nullptr ||
      static_cast<int>(pixels->size()) < count * image_pixel_count) {
    LOG(ERROR) << "Failed to create texture array as it contains "
               << (pixels != nullptr ? pixels->size() : 0) << " pixels, but "
               << count * image_pixel_count << " are required.";
    return nullptr;
  }
  const uint32_t* pixel_data = pixels->data();
  for (int i = 0; i < count; ++i) {
    if (!texture_array->Set(i, pixel_data, image_pixel_count * sizeof(Pixel))) {
      LOG(ERROR)
          << "Failed to initialize texture array with image data at index: "
          << i;
      return nullptr;
    }
    pixel_data += image_pixel_count;
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
  const int texture_pixel_count =
      texture_array->GetWidth() * texture_array->GetHeight();
  const int total_pixel_count = texture_array->GetCount() * texture_pixel_count;
  std::vector<uint32_t> pixels(total_pixel_count);
  for (int i = 0; i < texture_array->GetCount(); ++i) {
    if (!texture_array->Get(i, pixels.data() + (i * texture_pixel_count),
                            texture_pixel_count * sizeof(uint32_t))) {
      LOG(ERROR) << "Failed to read texture " << i
                 << " from texture array in order to save it";
      return false;
    }
  }
  auto& sampler_options = texture_array->GetSamplerOptions();

  fbs::SamplerOptionsBuilder sampler_options_builder(*builder);
  sampler_options_builder.add_filter(sampler_options.filter);
  sampler_options_builder.add_mipmap(sampler_options.mipmap);
  sampler_options_builder.add_border(sampler_options.border.Packed());
  sampler_options_builder.add_tile_size(sampler_options.tile_size);
  sampler_options_builder.add_address_mode(ToFbs(sampler_options.address_mode));
  const auto fb_sampler_options = sampler_options_builder.Finish();

  const auto fb_pixels =
      builder->CreateVector<uint32_t>(pixels.data(), total_pixel_count);

  fbs::TextureArrayChunkBuilder texture_array_builder(*builder);
  texture_array_builder.add_volatility(
      ToFbs(context->GetValue<DataVolatility>()));
  texture_array_builder.add_count(texture_array->GetCount());
  texture_array_builder.add_width(texture_array->GetWidth());
  texture_array_builder.add_height(texture_array->GetHeight());
  texture_array_builder.add_options(fb_sampler_options);
  texture_array_builder.add_pixels(fb_pixels);
  const auto fb_texture_array = texture_array_builder.Finish();

  builder->Finish(fb_texture_array);
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
