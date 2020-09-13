#include "world_resources.h"

#include "gb/imgui/imgui_instance.h"
#include "gb/render/render_system.h"
#include "gb/resource/resource_system.h"

// Game includes
#include "scene_types.h"

WorldResources::WorldResources(gb::ValidatedContext context)
    : context_(std::move(context)) {}

WorldResources::~WorldResources() {}

std::unique_ptr<WorldResources> WorldResources::Create(Contract contract) {
  if (!contract.IsValid()) {
    return nullptr;
  }
  auto world_resources =
      absl::WrapUnique(new WorldResources(std::move(contract)));
  if (!world_resources->InitGraphics() || !world_resources->InitGui()) {
    return nullptr;
  }
  return world_resources;
}

bool WorldResources::InitGraphics() {
  auto* resource_system = context_.GetPtr<gb::ResourceSystem>();
  auto* render_system = context_.GetPtr<gb::RenderSystem>();

  auto vertex_shader_code =
      render_system->LoadShaderCode("asset:/shaders/block.vert.spv");
  if (vertex_shader_code == nullptr) {
    LOG(ERROR) << "Failed to load vertex shader";
    return false;
  }

  auto fragment_shader_code =
      render_system->LoadShaderCode("asset:/shaders/block.frag.spv");
  if (fragment_shader_code == nullptr) {
    LOG(ERROR) << "Failed to load fragment shader";
    return false;
  }

  const auto* vertex_type = render_system->RegisterVertexType<Vertex>(
      "Vertex", {gb::ShaderValue::kVec3, gb::ShaderValue::kVec3,
                 gb::ShaderValue::kVec2, gb::ShaderValue::kColor});
  if (vertex_type == nullptr) {
    LOG(ERROR) << "Vertex type could not be registered (likely a size or "
                  "alignment issue)";
    return false;
  }

  const auto* scene_constants =
      render_system->RegisterConstantsType<SceneData>("SceneData");
  const auto* scene_light_constants =
      render_system->RegisterConstantsType<SceneLightData>("SceneLightData");
  const auto* instance_constants =
      render_system->RegisterConstantsType<InstanceData>("InstanceData");
  if (scene_constants == nullptr || scene_light_constants == nullptr ||
      instance_constants == nullptr) {
    LOG(ERROR) << "Could not register one or more render constants";
    return false;
  }

  scene_type_ = render_system->RegisterSceneType(
      "Scene",
      {
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kScene, 0)
              .SetConstants(scene_constants, gb::DataVolatility::kPerFrame),
          gb::Binding()
              .SetShaders(gb::ShaderType::kFragment)
              .SetLocation(gb::BindingSet::kScene, 1)
              .SetConstants(scene_light_constants,
                            gb::DataVolatility::kPerFrame),
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kInstance, 0)
              .SetConstants(instance_constants, gb::DataVolatility::kPerFrame),

      });
  if (scene_type_ == nullptr) {
    LOG(ERROR) << "Could not register scene type";
    return false;
  }

  gb::Shader* vertex_shader = render_system->CreateShader(
      &resources_, gb::ShaderType::kVertex, std::move(vertex_shader_code),
      {
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kScene, 0)
              .SetConstants(scene_constants),
          gb::Binding()
              .SetShaders(gb::ShaderType::kVertex)
              .SetLocation(gb::BindingSet::kInstance, 0)
              .SetConstants(instance_constants),
      },
      {
          {gb::ShaderValue::kVec3, 0},  // in_pos
          {gb::ShaderValue::kVec3, 1},  // in_normal
          {gb::ShaderValue::kVec2, 2},  // in_uv
          {gb::ShaderValue::kVec4, 3},  // in_color
      },
      {
          {gb::ShaderValue::kVec3, 0},  // out_pos
          {gb::ShaderValue::kVec3, 1},  // out_normal
          {gb::ShaderValue::kVec2, 2},  // out_uv
          {gb::ShaderValue::kVec4, 3},  // out_color
      });
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Could not create vertex shader";
    return false;
  }

  gb::Shader* fragment_shader = render_system->CreateShader(
      &resources_, gb::ShaderType::kFragment, std::move(fragment_shader_code),
      {
          gb::Binding()
              .SetShaders(gb::ShaderType::kFragment)
              .SetLocation(gb::BindingSet::kScene, 1)
              .SetConstants(scene_light_constants),
          gb::Binding()
              .SetShaders(gb::ShaderType::kFragment)
              .SetLocation(gb::BindingSet::kMaterial, 0)
              .SetTexture(),
      },
      {
          {gb::ShaderValue::kVec3, 0},  // in_pos
          {gb::ShaderValue::kVec3, 1},  // in_normal
          {gb::ShaderValue::kVec2, 2},  // in_uv
          {gb::ShaderValue::kVec4, 3},  // in_color
      },
      {
          {gb::ShaderValue::kVec4, 0},  // out_color
      });
  if (fragment_shader == nullptr) {
    LOG(ERROR) << "Could not create fragment shader";
    return false;
  }

  gb::MaterialType* material_type = render_system->CreateMaterialType(
      &resources_, scene_type_, vertex_type, vertex_shader, fragment_shader);
  if (material_type == nullptr) {
    LOG(ERROR) << "Could not create material type";
    return false;
  }

  chunk_material_ = render_system->CreateMaterial(&resources_, material_type);
  if (chunk_material_ == nullptr) {
    LOG(ERROR) << "Could not create material";
    return false;
  }

  block_texture_ = resource_system->Load<gb::Texture>(
      &resources_, "asset:/textures/block.png");
  if (block_texture_ == nullptr) {
    LOG(ERROR) << "Failed to block texture";
    return false;
  }
  chunk_material_->GetMaterialBindingData()->SetTexture(0, block_texture_);
  return true;
}

bool WorldResources::InitGui() {
  auto* gui_instance = context_.GetPtr<gb::ImGuiInstance>();
  chunk_gui_texture_ = gui_instance->AddTexture(block_texture_);
  if (chunk_gui_texture_ == nullptr) {
    return false;
  }
  return true;
}