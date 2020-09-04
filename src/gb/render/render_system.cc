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
#include "gb/render/render_backend.h"
#include "gb/render/render_resource_chunks.h"
#include "gb/resource/file/resource_file_reader.h"
#include "gb/resource/file/resource_file_writer.h"
#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_system.h"
#include "stb_image.h"

namespace gb {

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
  resource_writer_->RegisterResourceWriter<Texture>(
      kChunkTypeTexture, [this](Context* context, Texture* resource,
                                std::vector<ChunkWriter>* out_chunks) {
        return SaveTextureChunk(context, resource, out_chunks);
      });
  resource_writer_->RegisterResourceWriter<Shader>(
      kChunkTypeShader, [this](Context* context, Shader* resource,
                               std::vector<ChunkWriter>* out_chunks) {
        return SaveShaderChunk(context, resource, out_chunks);
      });
  resource_writer_->RegisterResourceWriter<MaterialType>(
      kChunkTypeMaterialType, [this](Context* context, MaterialType* resource,
                                     std::vector<ChunkWriter>* out_chunks) {
        return SaveMaterialTypeChunk(context, resource, out_chunks);
      });
  resource_writer_->RegisterResourceWriter<Material>(
      kChunkTypeMaterial, [this](Context* context, Material* resource,
                                 std::vector<ChunkWriter>* out_chunks) {
        return SaveMaterialChunk(context, resource, out_chunks);
      });
  resource_writer_->RegisterResourceWriter<Mesh>(
      kChunkTypeMesh, [this](Context* context, Mesh* resource,
                             std::vector<ChunkWriter>* out_chunks) {
        return SaveMeshChunk(context, resource, out_chunks);
      });

  resource_reader_ = ResourceFileReader::Create(context_);
  resource_reader_->RegisterGenericChunk<BindingChunk>(
      kChunkTypeBinding, 1,
      [this](Context* context, ChunkReader* chunk_reader) {
        return LoadBindingChunk(context, chunk_reader);
      });
  resource_reader_->RegisterGenericChunk<BindingDataChunk>(
      kChunkTypeMaterialBindingData, 1,
      [this](Context* context, ChunkReader* chunk_reader) {
        return LoadBindingDataChunk(context, chunk_reader);
      });
  resource_reader_->RegisterGenericChunk<BindingDataChunk>(
      kChunkTypeInstanceBindingData, 1,
      [this](Context* context, ChunkReader* chunk_reader) {
        return LoadBindingDataChunk(context, chunk_reader);
      });
  resource_reader_->RegisterResourceChunk<Texture, TextureChunk>(
      kChunkTypeTexture, 1,
      [this](Context* context, ChunkReader* chunk_reader, ResourceEntry entry) {
        return LoadTextureChunk(context, chunk_reader, std::move(entry));
      });
  resource_reader_->RegisterResourceChunk<Shader, ShaderChunk>(
      kChunkTypeShader, 1,
      [this](Context* context, ChunkReader* chunk_reader, ResourceEntry entry) {
        return LoadShaderChunk(context, chunk_reader, std::move(entry));
      });
  resource_reader_->RegisterResourceChunk<MaterialType, MaterialTypeChunk>(
      kChunkTypeMaterialType, 1,
      [this](Context* context, ChunkReader* chunk_reader, ResourceEntry entry) {
        return LoadMaterialTypeChunk(context, chunk_reader, std::move(entry));
      });
  resource_reader_->RegisterResourceChunk<Material, MaterialChunk>(
      kChunkTypeMaterial, 1,
      [this](Context* context, ChunkReader* chunk_reader, ResourceEntry entry) {
        return LoadMaterialChunk(context, chunk_reader, std::move(entry));
      });
  resource_reader_->RegisterResourceChunk<Mesh, MeshChunk>(
      kChunkTypeMesh, 1,
      [this](Context* context, ChunkReader* chunk_reader, ResourceEntry entry) {
        return LoadMeshChunk(context, chunk_reader, std::move(entry));
      });

  resource_manager_ = std::make_unique<ResourceManager>();
  resource_manager_->InitGenericLoader(
      [this](Context* context, TypeKey* type, std::string_view name) {
        return resource_reader_->Read(type, name, context);
      });
  resource_manager_->InitLoader<Texture>(
      [this](Context* context, std::string_view name) {
        return LoadTexture(name);
      });

  TypeKey::Get<Texture>()->SetTypeName("Texture");
  TypeKey::Get<Shader>()->SetTypeName("Shader");
  TypeKey::Get<MaterialType>()->SetTypeName("MaterialType");
  TypeKey::Get<Material>()->SetTypeName("Material");
  TypeKey::Get<Mesh>()->SetTypeName("Mesh");
  auto* resource_system = context_.GetPtr<ResourceSystem>();
  if (!resource_system->Register<Texture, Shader, MaterialType, Material, Mesh>(
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
  for (ShaderValue attribute : attributes) {
    switch (attribute) {
      case ShaderValue::kFloat:
      case ShaderValue::kColor:
        expected_size += 4;
        break;
      case ShaderValue::kVec2:
        expected_size += 8;
        break;
      case ShaderValue::kVec3:
        expected_size += 12;
        break;
      case ShaderValue::kVec4:
        expected_size += 16;
        break;
      default:
        LOG(FATAL) << "Unhandled shader value type for vertex";
    }
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

std::unique_ptr<RenderScene> RenderSystem::CreateScene(
    RenderSceneType* scene_type, int scene_order) {
  return backend_->CreateScene({}, scene_type, scene_order);
}

bool RenderSystem::LoadBindingChunk(Context* context,
                                    ChunkReader* chunk_reader) {
  std::vector<Binding> bindings(chunk_reader->GetCount());
  for (int i = 0; i < chunk_reader->GetCount(); ++i) {
    auto& binding = bindings[i];
    auto& chunk = chunk_reader->GetChunkData<BindingChunk>()[i];
    binding.shader_types = ShaderTypes(chunk.shaders);
    binding.set = static_cast<BindingSet>(chunk.set);
    binding.index = chunk.index;
    binding.binding_type = static_cast<BindingType>(chunk.type);
    binding.volatility = static_cast<DataVolatility>(chunk.volatility);
    chunk_reader->ConvertToPtr(&chunk.constants_name);
    if (chunk.constants_name.ptr != nullptr) {
      binding.constants_type = GetConstantsType(chunk.constants_name.ptr);
      if (binding.constants_type == nullptr) {
        LOG(ERROR) << "Invalid constants type " << chunk.constants_name.ptr
                   << " for binding " << i;
        return false;
      }
    }
    if (!binding.IsValid()) {
      LOG(ERROR) << "Invalid binding " << i;
      return false;
    }
  }
  context->SetNew<std::vector<Binding>>(std::move(bindings));
  return true;
}

bool RenderSystem::SaveBindingChunk(absl::Span<const Binding> bindings,
                                    std::vector<ChunkWriter>* out_chunks) {
  auto& chunk_writer = out_chunks->emplace_back(ChunkWriter::New<BindingChunk>(
      kChunkTypeBinding, 1, static_cast<int32_t>(bindings.size())));
  for (int i = 0; i < chunk_writer.GetCount(); ++i) {
    const auto& binding = bindings[i];
    auto& chunk = chunk_writer.GetChunkData<BindingChunk>()[i];
    chunk.shaders = static_cast<uint8_t>(binding.shader_types.GetMask());
    chunk.set = static_cast<uint8_t>(binding.set);
    chunk.index = static_cast<uint16_t>(binding.index);
    chunk.type = static_cast<uint16_t>(binding.binding_type);
    chunk.volatility = static_cast<uint16_t>(binding.volatility);
    if (binding.constants_type != nullptr) {
      chunk.constants_name =
          chunk_writer.AddString(binding.constants_type->GetName());
    }
  }
  return true;
}

bool RenderSystem::LoadBindingDataChunk(Context* context,
                                        ChunkReader* chunk_reader) {
  BindingSet set;
  if (chunk_reader->GetType() == kChunkTypeMaterialBindingData) {
    set = BindingSet::kMaterial;
  } else if (chunk_reader->GetType() == kChunkTypeInstanceBindingData) {
    set = BindingSet::kInstance;
  } else {
    LOG(ERROR) << "Unhandled binding data chunk type: "
               << chunk_reader->GetType().ToString();
    return false;
  }

  auto all_bindings = context->GetValue<std::vector<Binding>>();
  std::vector<Binding> bindings;
  bindings.reserve(all_bindings.size());
  for (const auto& binding : all_bindings) {
    if (binding.set == set) {
      bindings.emplace_back(binding);
    }
  }

  const auto* resources = context->GetPtr<FileResources>();
  auto binding_data =
      std::make_unique<LocalBindingData>(RenderInternal{}, set, bindings);
  for (int i = 0; i < chunk_reader->GetCount(); ++i) {
    BindingDataChunk& chunk = chunk_reader->GetChunkData<BindingDataChunk>()[i];
    BindingType type = static_cast<BindingType>(chunk.type);
    if (type == BindingType::kTexture) {
      if (chunk.texture_id != 0) {
        Texture* texture = resources->GetResource<Texture>(chunk.texture_id);
        if (texture == nullptr) {
          LOG(ERROR) << "Failed to find texture (ID: " << chunk.texture_id
                     << ") for binding data";
          return false;
        }
        binding_data->SetTexture(chunk.index, texture);
      }
    } else if (type == BindingType::kConstants) {
      chunk_reader->ConvertToPtr(&chunk.constants_data);
      if (chunk.constants_data.ptr == nullptr) {
        LOG(ERROR) << "Missing constants data for binding data";
        return false;
      }
      TypeKey* constants_type = nullptr;
      for (const auto& binding : bindings) {
        if (binding.index == chunk.index) {
          constants_type = binding.constants_type->GetType();
          break;
        }
      }
      if (constants_type == nullptr) {
        LOG(ERROR) << "Unknown constants type at index " << chunk.index
                   << " for binding data";
        return false;
      }
      binding_data->SetInternal({}, chunk.index, constants_type,
                                chunk.constants_data.ptr);
    }
  }

  context->SetOwned<LocalBindingData>(chunk_reader->GetType().ToString(),
                                      std::move(binding_data));
  return true;
}

bool RenderSystem::SaveBindingDataChunk(BindingSet set,
                                        absl::Span<const Binding> all_bindings,
                                        BindingData* binding_data,
                                        std::vector<ChunkWriter>* out_chunks) {
  ChunkType chunk_type;
  switch (set) {
    case BindingSet::kMaterial:
      chunk_type = kChunkTypeMaterialBindingData;
      break;
    case BindingSet::kInstance:
      chunk_type = kChunkTypeInstanceBindingData;
      break;
    default:
      LOG(ERROR) << "Invalid binding data set to save: "
                 << static_cast<int>(set);
      return false;
  }

  std::vector<Binding> bindings;
  bindings.reserve(all_bindings.size());
  for (const auto& binding : all_bindings) {
    if (binding.set == set) {
      bindings.emplace_back(binding);
    }
  }
  if (bindings.empty()) {
    return true;
  }
  auto& chunk_writer =
      out_chunks->emplace_back(ChunkWriter::New<BindingDataChunk>(
          chunk_type, 1, static_cast<int32_t>(bindings.size())));
  for (int i = 0; i < chunk_writer.GetCount(); ++i) {
    const auto& binding = bindings[i];
    auto& chunk = chunk_writer.GetChunkData<BindingDataChunk>()[i];
    chunk.type = static_cast<int32_t>(binding.binding_type);
    chunk.index = static_cast<int32_t>(binding.index);
    switch (binding.binding_type) {
      case BindingType::kTexture: {
        auto* texture = binding_data->GetTexture(binding.index);
        if (texture != nullptr) {
          chunk.texture_id = texture->GetResourceId();
        }
      } break;
      case BindingType::kConstants: {
        std::vector<uint8_t> buffer(binding.constants_type->GetSize());
        binding_data->GetInternal({}, binding.index,
                                  binding.constants_type->GetType(),
                                  buffer.data());
        chunk.constants_data = chunk_writer.AddData<const void>(
            buffer.data(), static_cast<int32_t>(buffer.size()));
      } break;
      default:
        LOG(ERROR) << "Unhandled binding type " << chunk.type
                   << " writing binding data";
        return false;
    }
  }
  return true;
}

Mesh* RenderSystem::DoCreateMesh(Material* material, DataVolatility volatility,
                                 int max_vertices, int max_triangles) {
  if (material == nullptr) {
    LOG(ERROR) << "Null material passed to CreateMesh";
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
    LOG(ERROR) << "Invalid max number of triangls, must be greater than zero. "
                  "Value specified was: "
               << max_triangles;
    return nullptr;
  }

  auto vertex_buffer = backend_->CreateVertexBuffer(
      {}, volatility, material->GetType()->GetVertexType()->GetSize(),
      max_vertices);
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
                  material, volatility, std::move(vertex_buffer),
                  std::move(index_buffer));
}

Mesh* RenderSystem::LoadMeshChunk(Context* context, ChunkReader* chunk_reader,
                                  ResourceEntry entry) {
  auto* chunk = chunk_reader->GetChunkData<MeshChunk>();
  if (chunk == nullptr) {
    LOG(ERROR) << "Invalid mesh chunk";
    return nullptr;
  }
  chunk_reader->ConvertToPtr(&chunk->indices);
  chunk_reader->ConvertToPtr(&chunk->vertices);

  auto* resources = context->GetPtr<FileResources>();
  auto* material = resources->GetResource<Material>(chunk->material_id);
  if (material == nullptr) {
    LOG(ERROR) << "Material (ID: " << chunk->material_id
               << ") not found when loading mesh";
    return nullptr;
  }

  auto volatility = static_cast<DataVolatility>(chunk->volatility);
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  auto vertex_buffer = backend_->CreateVertexBuffer(
      {}, volatility, chunk->vertex_size, chunk->vertex_count);
  if (vertex_buffer == nullptr ||
      !vertex_buffer->Set(chunk->vertices.ptr, chunk->vertex_count)) {
    LOG(ERROR) << "Failed to initialize vertex buffer when loading mesh";
    return nullptr;
  }
  auto index_buffer =
      backend_->CreateIndexBuffer({}, volatility, chunk->index_count);
  if (index_buffer == nullptr ||
      !index_buffer->Set(chunk->indices.ptr, chunk->index_count)) {
    LOG(ERROR) << "Failed to initialize index buffer when loading mesh";
    return nullptr;
  }

  return new Mesh({}, std::move(entry), backend_, material, volatility,
                  std::move(vertex_buffer), std::move(index_buffer));
}

bool RenderSystem::SaveMesh(std::string_view name, Mesh* mesh,
                            DataVolatility volatility) {
  return resource_writer_->Write(
      name, mesh,
      ContextBuilder().SetValue<DataVolatility>(volatility).Build());
}

bool RenderSystem::SaveMeshChunk(Context* context, Mesh* mesh,
                                 std::vector<ChunkWriter>* out_chunks) {
  if (mesh->GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot save mesh with kStaticWrite volatility.";
    return false;
  }
  auto view = mesh->Edit();
  if (view == nullptr) {
    LOG(ERROR) << "Failed to read mesh in order to save it";
    return false;
  }

  auto& chunk_writer =
      out_chunks->emplace_back(ChunkWriter::New<MeshChunk>(kChunkTypeMesh, 1));
  auto* chunk = chunk_writer.GetChunkData<MeshChunk>();
  chunk->id = mesh->GetResourceId();
  chunk->material_id = mesh->GetMaterial()->GetResourceId();
  chunk->volatility = static_cast<int32_t>(context->GetValue<DataVolatility>());
  chunk->index_count = view->GetTriangleCount() * 3;
  chunk->vertex_count = view->GetVertexCount();
  chunk->vertex_size = static_cast<int32_t>(
      mesh->GetMaterial()->GetType()->GetVertexType()->GetSize());
  chunk->indices = chunk_writer.AddData<const uint16_t>(view->GetIndexData({}),
                                                        chunk->index_count);
  chunk->vertices = chunk_writer.AddData<const void>(
      view->GetVertexData({}), chunk->vertex_count * chunk->vertex_size);
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
                                          ChunkReader* chunk_reader,
                                          ResourceEntry entry) {
  auto* chunk = chunk_reader->GetChunkData<MaterialChunk>();
  if (chunk == nullptr) {
    LOG(ERROR) << "Invalid material chunk";
    return nullptr;
  }

  // Get the material type
  auto* resources = context->GetPtr<FileResources>();
  auto* material_type =
      resources->GetResource<MaterialType>(chunk->material_type_id);
  if (material_type == nullptr) {
    LOG(ERROR) << "Material type (ID: " << chunk->material_type_id
               << ") not found when loading material";
    return nullptr;
  }

  // Validate all material bindings are compatible with the the material type.
  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  for (const auto& binding : material_type->GetBindings()) {
    mapped_bindings[{binding.set, binding.index}] = binding;
  }
  auto bindings = context->GetValue<std::vector<Binding>>();
  for (const Binding& binding : bindings) {
    if (!mapped_bindings.contains({binding.set, binding.index})) {
      LOG(ERROR) << "Material binding not found in loaded material type";
      return nullptr;
    }
  }

  auto* material = new Material({}, std::move(entry), material_type);
  const auto* material_binding_data = context->GetPtr<LocalBindingData>(
      kChunkTypeMaterialBindingData.ToString());
  if (material_binding_data != nullptr) {
    material_binding_data->CopyTo(material->GetMaterialBindingData());
  }
  const auto* instance_binding_data = context->GetPtr<LocalBindingData>(
      kChunkTypeInstanceBindingData.ToString());
  if (instance_binding_data != nullptr) {
    instance_binding_data->CopyTo(material->GetDefaultInstanceBindingData());
  }
  return material;
}

bool RenderSystem::SaveMaterial(std::string_view name, Material* material) {
  return resource_writer_->Write(name, material);
}

bool RenderSystem::SaveMaterialChunk(Context* context, Material* material,
                                     std::vector<ChunkWriter>* out_chunks) {
  auto* material_type = material->GetType();
  if (!SaveBindingChunk(material_type->GetBindings(), out_chunks) ||
      !SaveBindingDataChunk(BindingSet::kMaterial, material_type->GetBindings(),
                            material->GetMaterialBindingData(), out_chunks) ||
      !SaveBindingDataChunk(BindingSet::kInstance, material_type->GetBindings(),
                            material->GetDefaultInstanceBindingData(),
                            out_chunks)) {
    return false;
  }

  auto& chunk_writer = out_chunks->emplace_back(
      ChunkWriter::New<MaterialChunk>(kChunkTypeMaterial, 1));
  auto* chunk = chunk_writer.GetChunkData<MaterialChunk>();
  chunk->id = material->GetResourceId();
  chunk->material_type_id = material_type->GetResourceId();
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
    bool match = (input.value == attributes[input.location]);
    if (!match) {
      switch (attributes[input.location]) {
        case ShaderValue::kColor:
          match = (input.value == ShaderValue::kVec4);
          break;
        default:
          break;
      }
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
      return false;
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
      return false;
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

MaterialType* RenderSystem::LoadMaterialTypeChunk(Context* context,
                                                  ChunkReader* chunk_reader,
                                                  ResourceEntry entry) {
  auto* resources = context->GetPtr<FileResources>();
  auto* chunk = chunk_reader->GetChunkData<MaterialTypeChunk>();
  if (chunk == nullptr) {
    LOG(ERROR) << "Invalid material type chunk";
    return nullptr;
  }
  chunk_reader->ConvertToPtr(&chunk->scene_type_name);
  chunk_reader->ConvertToPtr(&chunk->vertex_type_name);

  MaterialConfig config = {};
  config.cull_mode = static_cast<CullMode>(chunk->cull_mode);
  config.depth_mode = static_cast<DepthMode>(chunk->depth_mode);

  RenderSceneType* scene_type = GetSceneType(chunk->scene_type_name.ptr);
  if (scene_type == nullptr) {
    LOG(ERROR) << "Cannot load material type because scene type \""
               << chunk->scene_type_name.ptr << "\" is not registered";
    return nullptr;
  }
  auto* vertex_shader = resources->GetResource<Shader>(chunk->vertex_shader_id);
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Cannot load material type becayse vertex shader (ID: "
               << chunk->vertex_shader_id << ") is not loaded";
    return false;
  }
  auto* fragment_shader =
      resources->GetResource<Shader>(chunk->fragment_shader_id);
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Cannot load material type becayse fragment shader (ID: "
               << chunk->fragment_shader_id << ") is not loaded";
    return false;
  }
  const VertexType* vertex_type = GetVertexType(chunk->vertex_type_name.ptr);
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Cannot load material type because vertex type \""
               << chunk->vertex_type_name.ptr << "\" is not registered";
    return nullptr;
  }

  if (!ValidateMaterialTypeArguments(scene_type, vertex_type, vertex_shader,
                                     fragment_shader)) {
    return nullptr;
  }

  // Validate all shader bindings are compatible with the the scene and
  // each other.
  absl::flat_hash_map<std::tuple<BindingSet, int>, Binding> mapped_bindings;
  auto bindings = context->GetValue<std::vector<Binding>>();
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
  const auto* material_binding_data = context->GetPtr<LocalBindingData>(
      kChunkTypeMaterialBindingData.ToString());
  if (material_binding_data != nullptr) {
    material_binding_data->CopyTo(
        material_type->GetDefaultMaterialBindingData());
  }
  const auto* instance_binding_data = context->GetPtr<LocalBindingData>(
      kChunkTypeInstanceBindingData.ToString());
  if (instance_binding_data != nullptr) {
    instance_binding_data->CopyTo(
        material_type->GetDefaultInstanceBindingData());
  }
  return material_type;
}

bool RenderSystem::SaveMaterialType(std::string_view name,
                                    MaterialType* material_type) {
  return resource_writer_->Write(name, material_type);
}

bool RenderSystem::SaveMaterialTypeChunk(Context* context,
                                         MaterialType* material_type,
                                         std::vector<ChunkWriter>* out_chunks) {
  if (!SaveBindingChunk(material_type->GetBindings(), out_chunks) ||
      !SaveBindingDataChunk(BindingSet::kMaterial, material_type->GetBindings(),
                            material_type->GetDefaultMaterialBindingData(),
                            out_chunks) ||
      !SaveBindingDataChunk(BindingSet::kInstance, material_type->GetBindings(),
                            material_type->GetDefaultInstanceBindingData(),
                            out_chunks)) {
    return false;
  }

  auto& chunk_writer = out_chunks->emplace_back(
      ChunkWriter::New<MaterialTypeChunk>(kChunkTypeMaterialType, 1));
  auto* chunk = chunk_writer.GetChunkData<MaterialTypeChunk>();
  chunk->id = material_type->GetResourceId();
  chunk->scene_type_name =
      chunk_writer.AddString(material_type->GetSceneType()->GetName());
  chunk->vertex_shader_id = material_type->GetVertexShader()->GetResourceId();
  chunk->fragment_shader_id =
      material_type->GetFragmentShader()->GetResourceId();
  chunk->vertex_type_name =
      chunk_writer.AddString(material_type->GetVertexType()->GetName());
  const MaterialConfig& config = material_type->GetConfig();
  chunk->cull_mode = static_cast<uint8_t>(config.cull_mode);
  chunk->depth_mode = static_cast<uint8_t>(config.depth_mode);
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
                                      ChunkReader* chunk_reader,
                                      ResourceEntry entry) {
  std::vector<Binding> all_bindings;
  auto* bindings = context->GetPtr<std::vector<Binding>>();
  if (bindings != nullptr) {
    all_bindings = std::move(*bindings);
  }

  auto* chunk = chunk_reader->GetChunkData<ShaderChunk>();
  if (chunk == nullptr) {
    LOG(ERROR) << "Invalid shader chunk";
    return nullptr;
  }
  chunk_reader->ConvertToPtr<const ShaderParamEntry>(&chunk->inputs);
  chunk_reader->ConvertToPtr<const ShaderParamEntry>(&chunk->outputs);
  chunk_reader->ConvertToPtr<const void>(&chunk->code);

  std::vector<ShaderParam> inputs(chunk->input_count);
  for (int i = 0; i < chunk->input_count; ++i) {
    inputs[i].value = static_cast<ShaderValue>(chunk->inputs.ptr[i].value);
    inputs[i].location = chunk->inputs.ptr[i].location;
  }
  std::vector<ShaderParam> outputs(chunk->output_count);
  for (int i = 0; i < chunk->output_count; ++i) {
    outputs[i].value = static_cast<ShaderValue>(chunk->outputs.ptr[i].value);
    outputs[i].location = chunk->outputs.ptr[i].location;
  }

  auto shader_code =
      backend_->CreateShaderCode({}, chunk->code.ptr, chunk->code_size);
  if (shader_code == nullptr) {
    LOG(ERROR) << "Failed to create shader code for shader";
    return nullptr;
  }

  return new Shader({}, std::move(entry), static_cast<ShaderType>(chunk->type),
                    std::move(shader_code), all_bindings, inputs, outputs);
}

bool RenderSystem::SaveShader(std::string_view name, Shader* shader) {
  return resource_writer_->Write(name, shader);
}

bool RenderSystem::SaveShaderChunk(Context* context, Shader* shader,
                                   std::vector<ChunkWriter>* out_chunks) {
  const auto& code = shader->GetCode()->GetData({});
  if (code.empty()) {
    LOG(ERROR) << "Cannot save shader because shader code is empty (likely the "
                  "render system is not in edit mode)";
    return false;
  }

  if (!SaveBindingChunk(shader->GetBindings(), out_chunks)) {
    return false;
  }

  auto& chunk_writer = out_chunks->emplace_back(
      ChunkWriter::New<ShaderChunk>(kChunkTypeShader, 1));
  auto* chunk = chunk_writer.GetChunkData<ShaderChunk>();

  chunk->id = shader->GetResourceId();
  chunk->type = static_cast<int32_t>(shader->GetType());

  auto inputs = shader->GetInputs();
  std::vector<ShaderParamEntry> input_entries;
  input_entries.reserve(inputs.size());
  for (const auto& input : inputs) {
    input_entries.push_back(
        {static_cast<int32_t>(input.value), input.location});
  }
  chunk->input_count = static_cast<int32_t>(inputs.size());
  chunk->inputs = chunk_writer.AddData<const ShaderParamEntry>(input_entries);

  auto outputs = shader->GetOutputs();
  std::vector<ShaderParamEntry> output_entries;
  output_entries.reserve(outputs.size());
  for (const auto& output : outputs) {
    output_entries.push_back(
        {static_cast<int32_t>(output.value), output.location});
  }
  chunk->output_count = static_cast<int32_t>(outputs.size());
  chunk->outputs = chunk_writer.AddData<const ShaderParamEntry>(output_entries);

  chunk->code_size = static_cast<int32_t>(code.size());
  chunk->code = chunk_writer.AddData<const void>(code.data(), chunk->code_size);
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
                                       int height) {
  if (width <= 0 || width > kMaxTextureWidth || height <= 0 ||
      height > kMaxTextureHeight) {
    LOG(ERROR) << "Invalid texture dimensions in CreateTexture: " << width
               << " by " << height;
    return nullptr;
  }
  return backend_->CreateTexture({},
                                 resource_manager_->NewResourceEntry<Texture>(),
                                 volatility, width, height);
}

Texture* RenderSystem::LoadTexture(std::string_view name) {
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
  file->SeekBegin();

  if (chunk_type == kChunkTypeFile) {
    file.reset();
    return resource_reader_->Read<Texture>(name);
  }
  return LoadStbTexture(file.get());
}

Texture* RenderSystem::LoadTextureChunk(Context* context,
                                        ChunkReader* chunk_reader,
                                        ResourceEntry entry) {
  auto* chunk = chunk_reader->GetChunkData<TextureChunk>();
  if (chunk == nullptr) {
    LOG(ERROR) << "Invalid texture chunk";
    return nullptr;
  }
  if (chunk->width <= 0 || chunk->height <= 0 ||
      chunk->width >= kMaxTextureWidth || chunk->height >= kMaxTextureHeight) {
    LOG(ERROR) << "Invalid texture dimensions in texture file: " << chunk->width
               << " by " << chunk->height;
    return nullptr;
  }
  chunk_reader->ConvertToPtr(&chunk->pixels);

  DataVolatility volatility = static_cast<DataVolatility>(chunk->volatility);
  if (edit_ && volatility == DataVolatility::kStaticWrite) {
    volatility = DataVolatility::kStaticReadWrite;
  }
  Texture* texture = backend_->CreateTexture({}, std::move(entry), volatility,
                                             chunk->width, chunk->height);
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to create texture of dimensions " << chunk->width
               << "x" << chunk->height;
    return nullptr;
  }
  if (!texture->Set(chunk->pixels.ptr,
                    chunk->width * chunk->height * sizeof(Pixel))) {
    LOG(ERROR) << "Failed to initialize texture with image data";
    return nullptr;
  }
  return texture;
}

Texture* RenderSystem::LoadStbTexture(File* file) {
  struct IoState {
    int64_t size;
    gb::File* file;
  };
  static stbi_io_callbacks io_callbacks = {
      // Read callback.
      [](void* user, char* data, int size) -> int {
        auto* state = static_cast<IoState*>(user);
        return static_cast<int>(state->file->Read(data, size));
      },
      // Skip callback.
      [](void* user, int n) -> void {
        auto* state = static_cast<IoState*>(user);
        state->file->SeekBy(n);
      },
      // End-of-file callback.
      [](void* user) -> int {
        auto* state = static_cast<IoState*>(user);
        const int64_t position = state->file->GetPosition();
        return position < 0 || position == state->size;
      },
  };

  IoState state;
  file->SeekEnd();
  state.size = file->GetPosition();
  state.file = file;
  file->SeekBegin();

  int channels = 0;
  int width = 0;
  int height = 0;
  void* pixels = stbi_load_from_callbacks(&io_callbacks, &state, &width,
                                          &height, &channels, STBI_rgb_alpha);
  if (pixels == nullptr) {
    LOG(ERROR) << "Failed to read texture file with error: "
               << stbi_failure_reason();
    return false;
  }
  ScopedCall free_pixels([pixels]() { stbi_image_free(pixels); });

  Texture* texture = backend_->CreateTexture(
      {}, resource_manager_->NewResourceEntry<Texture>(),
      (edit_ ? DataVolatility::kStaticReadWrite : DataVolatility::kStaticWrite),
      width, height);
  if (texture == nullptr) {
    LOG(ERROR) << "Failed to create texture of dimensions " << width << "x"
               << height;
    return nullptr;
  }
  if (!texture->Set(pixels, width * height * sizeof(Pixel))) {
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
                                    std::vector<ChunkWriter>* out_chunks) {
  if (texture->GetVolatility() == DataVolatility::kStaticWrite) {
    LOG(ERROR) << "Cannot save texture with kStaticWrite volatility.";
    return false;
  }
  auto view = texture->Edit();
  if (view == nullptr) {
    LOG(ERROR) << "Failed to read texture in order to save it";
    return false;
  }
  out_chunks->emplace_back(
      ChunkWriter::New<TextureChunk>(kChunkTypeTexture, 1));
  ChunkWriter& chunk_writer = out_chunks->back();
  auto* chunk = chunk_writer.GetChunkData<TextureChunk>();
  chunk->id = texture->GetResourceId();
  chunk->volatility = static_cast<int32_t>(context->GetValue<DataVolatility>());
  chunk->width = static_cast<uint16_t>(texture->GetWidth());
  chunk->height = static_cast<uint16_t>(texture->GetHeight());
  chunk->pixels = chunk_writer.AddData<const Pixel>(
      view->GetPixels(), chunk->width * chunk->height);
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

void RenderSystem::Draw(RenderScene* scene, Mesh* mesh,
                        BindingData* instance_data) {
  RENDER_ASSERT(is_rendering_);
  RENDER_ASSERT(scene != nullptr && mesh != nullptr &&
                instance_data != nullptr);
  Material* const material = mesh->GetMaterial();
  MaterialType* const material_type = material->GetType();
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
