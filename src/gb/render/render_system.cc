// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_system.h"

#include <limits>

#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "gb/render/render_backend.h"
#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_system.h"

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
}

bool RenderSystem::Init() {
  resource_manager_ = std::make_unique<ResourceManager>();
  resource_manager_->InitLoader<Texture>(
      [this](std::string_view name) { return LoadTexture(name); });
  resource_manager_->InitLoader<ShaderCode>(
      [this](std::string_view name) { return LoadShaderCode(name); });
  TypeKey::Get<Texture>()->SetTypeName("Texture");
  TypeKey::Get<ShaderCode>()->SetTypeName("ShaderCode");
  TypeKey::Get<Shader>()->SetTypeName("Shader");
  TypeKey::Get<MaterialType>()->SetTypeName("MaterialType");
  TypeKey::Get<Material>()->SetTypeName("Material");
  TypeKey::Get<Mesh>()->SetTypeName("Mesh");
  auto* resource_system = context_.GetPtr<ResourceSystem>();
  if (!resource_system->Register<Texture, ShaderCode, Shader, MaterialType,
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
      std::make_unique<RenderDataType>(RenderInternal{}, type, size));
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
      absl::StrCat(name),
      std::make_unique<VertexType>(RenderInternal{}, type, size, attributes));
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

const RenderSceneType* RenderSystem::GetSceneType(std::string_view name) const {
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

Material* RenderSystem::DoCreateMaterial(MaterialType* material_type) {
  if (material_type == nullptr) {
    LOG(ERROR) << "Null material type passed to CreateMaterial";
    return nullptr;
  }

  return new Material({}, resource_manager_->NewResourceEntry<Material>(),
                      material_type);
}

MaterialType* RenderSystem::DoCreateMaterialType(RenderSceneType* scene_type,
                                                 const VertexType* vertex_type,
                                                 Shader* vertex_shader,
                                                 Shader* fragment_shader) {
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
  if (vertex_shader->GetType() != ShaderType::kVertex) {
    LOG(ERROR) << "Vertex shader is not the correct shader type";
    return nullptr;
  }
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Null fragment shader passed in to CreateMaterialType";
    return nullptr;
  }
  if (fragment_shader->GetType() != ShaderType::kFragment) {
    LOG(ERROR) << "Fragment shader is not the correct shader type";
    return nullptr;
  }

  // Validate the vertex type matches the vertex shader inputs.
  auto attributes = vertex_type->GetAttributes();
  for (const ShaderParam& input : vertex_shader->GetInputs()) {
    if (input.location >= static_cast<int>(attributes.size())) {
      LOG(ERROR) << "Vertex shader requires input location " << input.location
                 << ", but vertex type only has " << attributes.size()
                 << " attributes.";
      return nullptr;
    }
    if (input.value != attributes[input.location]) {
      LOG(ERROR) << "Shader type mismatch for vertex input and vertex "
                    "attribute location "
                 << input.location;
      return nullptr;
    }
  }

  // Validate the inputs on the fragment shader have corresponding outputs from
  // the vertex shader.
  for (const ShaderParam& input : fragment_shader->GetInputs()) {
    bool found = false;
    for (const ShaderParam& output : vertex_shader->GetOutputs()) {
      if (output.location == input.location) {
        if (input.value != output.value) {
          LOG(ERROR) << "Shader type mismatch for fragment input and vertex "
                        "output location "
                     << input.location;
          return nullptr;
        }
        found = true;
        break;
      }
    }
    if (!found) {
      LOG(ERROR) << "Fragment shader input location " << input.location
                 << " not produced by vertex shader";
      return nullptr;
    }
  }

  // Validate all shader bindings are compatible with the the scene and each
  // other.
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
      LOG(ERROR)
          << "Vertex shader contains incompatible binding with scene: set="
          << static_cast<int>(binding.set) << ", index=" << binding.index;
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
      LOG(ERROR)
          << "Fragment shader contains incompatible binding with scene: set="
          << static_cast<int>(binding.set) << ", index=" << binding.index;
      return nullptr;
    }
    all_bindings.push_back(binding);
  }

  // Create a pipeline for the material.
  auto pipeline = backend_->CreatePipeline(
      {}, scene_type, vertex_type, all_bindings, vertex_shader->GetCode(),
      fragment_shader->GetCode());
  if (pipeline == nullptr) {
    LOG(ERROR) << "Failed to create pipeline for material type";
    return nullptr;
  }

  return new MaterialType(
      {}, resource_manager_->NewResourceEntry<MaterialType>(), all_bindings,
      std::move(pipeline), vertex_type, vertex_shader, fragment_shader);
}

Shader* RenderSystem::DoCreateShader(ShaderType type, ShaderCode* code,
                                     absl::Span<const Binding> bindings,
                                     absl::Span<const ShaderParam> inputs,
                                     absl::Span<const ShaderParam> outputs) {
  if (code == nullptr) {
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
                    code, all_bindings, inputs, outputs);
}

Texture* RenderSystem::DoCreateTexture(DataVolatility volatility, int width,
                                       int height) {
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

  // TODO: Load the texture from the file
  Texture* texture = backend_->CreateTexture(
      {}, resource_manager_->NewResourceEntry<Texture>(),
      DataVolatility::kStaticWrite, 0, 0);
  if (texture == nullptr) {
    LOG(ERROR) << "Error reading texture file: " << name;
  }

  return texture;
}

ShaderCode* RenderSystem::DoCreateShaderCode(const void* code,
                                             int64_t code_size) {
  return backend_->CreateShaderCode(
      {}, resource_manager_->NewResourceEntry<ShaderCode>(), code, code_size);
}

ShaderCode* RenderSystem::LoadShaderCode(std::string_view name) {
  auto* file_system = context_.GetPtr<FileSystem>();
  std::vector<uint8_t> file;
  if (!file_system->ReadFile(name, &file)) {
    LOG(ERROR) << "Could not read shader code file: " << name;
    return nullptr;
  }

  ShaderCode* code = backend_->CreateShaderCode(
      {}, resource_manager_->NewResourceEntry<ShaderCode>(), file.data(),
      static_cast<int>(file.size()));
  if (code == nullptr) {
    LOG(ERROR) << "Error reading shader code file: " << name;
  }

  return code;
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
  if (!is_rendering_ || scene == nullptr || mesh == nullptr ||
      instance_data == nullptr) {
    return;
  }
  Material* material = mesh->GetMaterial();
  RenderPipeline* pipeline = material->GetType()->GetPipeline({});
  if (instance_data->GetPipeline({}) != pipeline) {
    return;
  }
  backend_->Draw({}, scene, pipeline, material->GetMaterialBindingData(),
                 instance_data, mesh->GetVertexBuffer({}),
                 mesh->GetIndexBuffer({}));
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
