// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_system.h"

#include "gb/base/context_builder.h"
#include "gb/render/render_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAreArray;

class RenderSystemTest : public RenderTest {
 protected:
  // 16x16 test image as follows:
  // - Upper Left 8x8: Pure red
  // - Upper right 8x8: Pure green
  // - Lower left 8x8: Pure blue
  // - Lower right 8x8: Black and white 4x4 checkerboard
  // - Middle 8x8 (4x4 border): Alpha at 128
  std::vector<Pixel> MakeImageData() {
    std::vector<Pixel> pixels(16 * 16, Pixel(0, 0, 0, 255));
    for (int x = 0; x < 8; ++x) {
      for (int y = 0; y < 8; ++y) {
        pixels[y * 16 + x].r = 255;
        pixels[y * 16 + x + 8].g = 255;
        pixels[(y + 8) * 16 + x].b = 255;
        pixels[(y + 4) * 16 + x + 4].a = 128;
      }
    }
    for (int x = 0; x < 4; ++x) {
      for (int y = 0; y < 4; ++y) {
        pixels[(y + 8) * 16 + x + 12] = Pixel(255, 255, 255);
        pixels[(y + 12) * 16 + x + 8] = Pixel(255, 255, 255);
      }
    }
    return pixels;
  }

  // 16x16 test image as PNG.
  const std::vector<uint8_t> kPngImage = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
      0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10,
      0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0xf3, 0xff, 0x61, 0x00, 0x00, 0x00,
      0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00,
      0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc,
      0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
      0x0e, 0xc2, 0x00, 0x00, 0x0e, 0xc2, 0x01, 0x15, 0x28, 0x4a, 0x80, 0x00,
      0x00, 0x00, 0x4c, 0x49, 0x44, 0x41, 0x54, 0x38, 0x4f, 0x63, 0xfc, 0xcf,
      0xc0, 0x00, 0x44, 0xb8, 0x01, 0x48, 0x01, 0x3e, 0xc0, 0x04, 0xa5, 0xc9,
      0x06, 0x03, 0x6f, 0x00, 0xb6, 0x30, 0x68, 0x84, 0xd2, 0x60, 0x80, 0x25,
      0x0c, 0xea, 0xa1, 0x34, 0x18, 0x0c, 0x83, 0x30, 0x60, 0x62, 0x04, 0x06,
      0x01, 0x3e, 0x0c, 0x24, 0x50, 0xf0, 0x7f, 0x34, 0x38, 0x0c, 0xc2, 0x00,
      0xe4, 0x2b, 0xbc, 0xa9, 0x1d, 0x5d, 0x96, 0x91, 0x11, 0x14, 0x18, 0x08,
      0x30, 0xe4, 0xc3, 0x80, 0x81, 0x01, 0x00, 0x1a, 0x1a, 0x1b, 0x15, 0x7e,
      0x54, 0xb9, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
      0x42, 0x60, 0x82};

  // A cube, CCW faces
  const std::vector<Vector3> kCubeVertices = {
      {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
      {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
  };
  const std::vector<Triangle> kCubeTriangles = {
      {5, 1, 2}, {2, 6, 5}, {0, 4, 7}, {7, 3, 0}, {3, 7, 6}, {6, 2, 3},
      {0, 1, 5}, {5, 3, 0}, {4, 5, 6}, {6, 7, 4}, {1, 0, 3}, {3, 2, 1},
  };
};

TEST_F(RenderSystemTest, RequiredCreationContext) {
  EXPECT_EQ(RenderSystem::Create(ContextBuilder()
                                     .SetOwned(ResourceSystem::Create())
                                     .SetOwned(std::make_unique<FileSystem>())
                                     .Build()),
            nullptr);
  EXPECT_EQ(
      RenderSystem::Create(ContextBuilder()
                               .SetOwned<RenderBackend>(
                                   std::make_unique<TestRenderBackend>(&state_))
                               .SetOwned(std::make_unique<FileSystem>())
                               .Build()),
      nullptr);
  EXPECT_EQ(
      RenderSystem::Create(ContextBuilder()
                               .SetOwned<RenderBackend>(
                                   std::make_unique<TestRenderBackend>(&state_))
                               .SetOwned(ResourceSystem::Create())
                               .Build()),
      nullptr);
  EXPECT_NE(
      RenderSystem::Create(ContextBuilder()
                               .SetOwned<RenderBackend>(
                                   std::make_unique<TestRenderBackend>(&state_))
                               .SetOwned(ResourceSystem::Create())
                               .SetOwned(std::make_unique<FileSystem>())
                               .Build()),
      nullptr);
}

TEST_F(RenderSystemTest, RegisterConstantsType) {
  CreateSystem();
  const auto* type = render_system_->RegisterConstantsType<Vector3>("vec3");
  EXPECT_NE(type, nullptr);
  EXPECT_EQ(type->GetName(), "vec3");
  EXPECT_EQ(type->GetType(), TypeKey::Get<Vector3>());
  EXPECT_EQ(type->GetSize(), sizeof(Vector3));
  EXPECT_EQ(render_system_->GetConstantsType("vec3"), type);

  EXPECT_EQ(render_system_->RegisterConstantsType<Vector3>("vec3"), nullptr);
  EXPECT_EQ(render_system_->RegisterConstantsType<Vector4>("vec3"), nullptr);
  EXPECT_NE(render_system_->RegisterConstantsType<Vector3>("vec3b"), nullptr);
  EXPECT_NE(render_system_->RegisterConstantsType<Vector4>("vec4"), nullptr);
}

TEST_F(RenderSystemTest, RegisterVertexType) {
  struct Vertex {
    Vector3 pos;
    Vector2 uv;
  };
  CreateSystem();
  const auto* type = render_system_->RegisterVertexType<Vertex>(
      "vertex", {ShaderValue::kVec3, ShaderValue::kVec2});
  EXPECT_NE(type, nullptr);
  EXPECT_EQ(type->GetName(), "vertex");
  EXPECT_EQ(type->GetType(), TypeKey::Get<Vertex>());
  EXPECT_EQ(type->GetSize(), sizeof(Vertex));
  EXPECT_THAT(type->GetAttributes(),
              ElementsAre(ShaderValue::kVec3, ShaderValue::kVec2));

  EXPECT_EQ(render_system_->RegisterVertexType<Vertex>(
                "vertex", {ShaderValue::kVec3, ShaderValue::kVec2}),
            nullptr);
  EXPECT_EQ(render_system_->RegisterVertexType<Vector4>("vertex",
                                                        {ShaderValue::kVec4}),
            nullptr);
  EXPECT_EQ(render_system_->RegisterVertexType<Vertex>("vertex2",
                                                       {ShaderValue::kVec3}),
            nullptr);
  EXPECT_NE(render_system_->RegisterVertexType<Vector3>("vertex3",
                                                        {ShaderValue::kVec3}),
            nullptr);
  EXPECT_NE(render_system_->RegisterVertexType<Vector4>(
                "vertex4", {ShaderValue::kFloat, ShaderValue::kFloat,
                            ShaderValue::kFloat, ShaderValue::kFloat}),
            nullptr);
}

TEST_F(RenderSystemTest, RegisterSceneType) {
  CreateSystem();
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kScene, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector3>("vec3")),
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
  };
  auto scene_type = render_system_->RegisterSceneType("scene", bindings);
  EXPECT_NE(scene_type, nullptr);
  EXPECT_EQ(scene_type->GetName(), "scene");
  EXPECT_THAT(scene_type->GetBindings(), ElementsAreArray(bindings));
  EXPECT_EQ(render_system_->GetSceneType("scene"), scene_type);

  EXPECT_EQ(render_system_->RegisterSceneType("scene", {}), nullptr);
  EXPECT_NE(render_system_->RegisterSceneType("scene2", {}), nullptr);
}

TEST_F(RenderSystemTest, FrameDimensions) {
  state_.frame_dimensions = {100, 200};
  CreateSystem();
  EXPECT_EQ(render_system_->GetFrameDimensions().width, 100);
  EXPECT_EQ(render_system_->GetFrameDimensions().height, 200);
  state_.frame_dimensions = {300, 400};
  EXPECT_EQ(render_system_->GetFrameDimensions().width, 300);
  EXPECT_EQ(render_system_->GetFrameDimensions().height, 400);
}

TEST_F(RenderSystemTest, LoadPngTexture) {
  CreateSystem();
  ASSERT_TRUE(file_system_->WriteFile("mem:/image.png", kPngImage));
  ResourcePtr<Texture> texture =
      resource_system_->Load<Texture>("mem:/image.png");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  auto expected_pixels = MakeImageData();
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, LoadPngTextureInEditMode) {
  CreateSystem(true);
  ASSERT_TRUE(file_system_->WriteFile("mem:/image.png", kPngImage));
  ResourcePtr<Texture> texture =
      resource_system_->Load<Texture>("mem:/image.png");
  ASSERT_NE(texture, nullptr);
  EXPECT_NE(texture->GetResourceId(), 0);
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.png");
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticReadWrite);
}

TEST_F(RenderSystemTest, SaveLoadTexture) {
  CreateSystem();
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  auto texture_id = texture->GetResourceId();
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(texture->Set(expected_pixels));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/image.gbtx", texture.Get()));
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  texture.Reset();

  texture = resource_system_->Load<Texture>("mem:/image.gbtx");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetResourceId(), texture_id);
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SaveLoadTextureWithVolatility) {
  CreateSystem();
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  auto texture_id = texture->GetResourceId();
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(texture->Set(expected_pixels));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/image.gbtx", texture.Get(),
                                          DataVolatility::kPerFrame));
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  texture.Reset();

  texture = resource_system_->Load<Texture>("mem:/image.gbtx");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetResourceId(), texture_id);
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kPerFrame);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SaveLoadTextureInEditMode) {
  CreateSystem(true);
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kPerFrame, 16, 16);
  auto texture_id = texture->GetResourceId();
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(texture->Set(expected_pixels));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/image.gbtx", texture.Get()));
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  texture.Reset();

  texture = resource_system_->Load<Texture>("mem:/image.gbtx");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetResourceId(), texture_id);
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticReadWrite);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SavShaderNotInEditMode) {
  CreateSystem();
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(shader, nullptr);
  ASSERT_FALSE(render_system_->SaveShader("mem:/shader.gbsh", shader.Get()));
}

TEST_F(RenderSystemTest, SaveLoadShaderInEditMode) {
  CreateSystem(true);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector2>("vec2")),
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kInstance, 1)
          .SetTexture(),
  };
  std::vector<ShaderParam> inputs = {{ShaderValue::kVec3, 0},
                                     {ShaderValue::kVec2, 1}};
  std::vector<ShaderParam> outputs = {{ShaderValue::kFloat, 0},
                                      {ShaderValue::kVec4, 1}};
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      bindings, inputs, outputs);
  ASSERT_NE(shader, nullptr);
  auto shader_id = shader->GetResourceId();
  ASSERT_TRUE(render_system_->SaveShader("mem:/shader.gbsh", shader.Get()));
  EXPECT_EQ(shader->GetResourceName(), "mem:/shader.gbsh");
  shader.Reset();

  shader = resource_system_->Load<Shader>("mem:/shader.gbsh");
  ASSERT_NE(shader, nullptr);
  EXPECT_EQ(shader->GetResourceId(), shader_id);
  EXPECT_EQ(shader->GetResourceName(), "mem:/shader.gbsh");
  EXPECT_EQ(shader->GetType(), ShaderType::kVertex);
  EXPECT_THAT(shader->GetBindings(), UnorderedElementsAreArray(bindings));
  EXPECT_THAT(shader->GetInputs(), UnorderedElementsAreArray(inputs));
  EXPECT_THAT(shader->GetOutputs(), UnorderedElementsAreArray(outputs));
}

TEST_F(RenderSystemTest, SaveMaterialTypeWithUnsavedShader) {
  CreateSystem(true);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector2>("vec2")),
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kInstance, 1)
          .SetTexture(),
  };
  auto* material_type = CreateMaterialType(bindings);
  ASSERT_FALSE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                                material_type));
}

TEST_F(RenderSystemTest, SaveLoadMaterialType) {
  CreateSystem(true);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector2>("vec2")),
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kInstance, 1)
          .SetTexture(),
  };
  auto* material_type_ptr =
      CreateMaterialType(bindings, MaterialConfig()
                                       .SetCullMode(CullMode::kFront)
                                       .SetDepthMode(DepthMode::kTest));
  auto material_type_id = material_type_ptr->GetResourceId();
  auto* vertex_shader = material_type_ptr->GetVertexShader();
  auto* fragment_shader = material_type_ptr->GetFragmentShader();
  auto* vertex_type = material_type_ptr->GetVertexType();

  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  texture->Set(MakeImageData());
  material_type_ptr->GetDefaultMaterialBindingData()->SetConstants<Vector2>(
      0, {1, 2});
  material_type_ptr->GetDefaultInstanceBindingData()->SetTexture(1,
                                                                 texture.Get());

  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh", vertex_shader));
  ASSERT_TRUE(
      render_system_->SaveShader("mem:/fragment.gbsh", fragment_shader));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/texture.gbtx", texture.Get()));

  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type_ptr));
  EXPECT_EQ(material_type_ptr->GetResourceName(), "mem:/material_type.gbmt");
  temp_resource_set_.Remove(material_type_ptr, false);

  auto material_type =
      resource_system_->Load<MaterialType>("mem:/material_type.gbmt");
  ASSERT_NE(material_type, nullptr);
  EXPECT_EQ(material_type->GetResourceId(), material_type_id);
  EXPECT_EQ(material_type->GetResourceName(), "mem:/material_type.gbmt");
  EXPECT_THAT(material_type->GetBindings(),
              UnorderedElementsAreArray(bindings));
  EXPECT_EQ(material_type->GetVertexShader(), vertex_shader);
  EXPECT_EQ(material_type->GetFragmentShader(), fragment_shader);
  EXPECT_EQ(material_type->GetVertexType(), vertex_type);
  EXPECT_EQ(material_type->GetConfig().cull_mode, CullMode::kFront);
  EXPECT_EQ(material_type->GetConfig().depth_mode, DepthMode::kTest);
  Vector2 constants = {0, 0};
  material_type->GetDefaultMaterialBindingData()->GetConstants<Vector2>(
      0, &constants);
  EXPECT_EQ(constants.x, 1);
  EXPECT_EQ(constants.y, 2);
  EXPECT_EQ(material_type->GetDefaultInstanceBindingData()->GetTexture(1),
            texture.Get());
}

TEST_F(RenderSystemTest, SaveLoadMaterialTypeAndDependencies) {
  CreateSystem(true);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector2>("vec2")),
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kInstance, 1)
          .SetTexture(),
  };
  auto* material_type_ptr = CreateMaterialType(bindings);
  auto material_type_id = material_type_ptr->GetResourceId();
  auto* vertex_shader = material_type_ptr->GetVertexShader();
  auto* fragment_shader = material_type_ptr->GetFragmentShader();
  auto* vertex_type = material_type_ptr->GetVertexType();

  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  texture->Set(MakeImageData());
  material_type_ptr->GetDefaultMaterialBindingData()->SetConstants<Vector2>(
      0, {1, 2});
  material_type_ptr->GetDefaultInstanceBindingData()->SetTexture(1,
                                                                 texture.Get());

  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh", vertex_shader));
  ASSERT_TRUE(
      render_system_->SaveShader("mem:/fragment.gbsh", fragment_shader));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/texture.gbtx", texture.Get()));

  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type_ptr));
  EXPECT_EQ(material_type_ptr->GetResourceName(), "mem:/material_type.gbmt");
  temp_resource_set_.RemoveAll();

  auto material_type = resource_system_->Load<MaterialType>(
      &temp_resource_set_, "mem:/material_type.gbmt");
  ASSERT_NE(material_type, nullptr);
  EXPECT_EQ(material_type->GetResourceId(), material_type_id);
  EXPECT_EQ(material_type->GetResourceName(), "mem:/material_type.gbmt");
  EXPECT_THAT(material_type->GetBindings(),
              UnorderedElementsAreArray(bindings));
  EXPECT_NE(material_type->GetVertexShader(), nullptr);
  EXPECT_NE(material_type->GetFragmentShader(), nullptr);
  EXPECT_EQ(material_type->GetVertexType(), vertex_type);
  Vector2 constants = {0, 0};
  material_type->GetDefaultMaterialBindingData()->GetConstants<Vector2>(
      0, &constants);
  EXPECT_EQ(constants.x, 1);
  EXPECT_EQ(constants.y, 2);
  EXPECT_NE(material_type->GetDefaultInstanceBindingData()->GetTexture(1),
            nullptr);
}

TEST_F(RenderSystemTest, SaveLoadMaterialAndDependencies) {
  CreateSystem(true);
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector2>("vec2")),
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kInstance, 1)
          .SetTexture(),
  };
  auto* material = CreateMaterial(bindings);
  auto material_id = material->GetResourceId();
  auto* material_type = material->GetType();
  auto* vertex_shader = material_type->GetVertexShader();
  auto* fragment_shader = material_type->GetFragmentShader();

  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  texture->Set(MakeImageData());
  material->GetMaterialBindingData()->SetConstants<Vector2>(0, {1, 2});
  material->GetDefaultInstanceBindingData()->SetTexture(1, texture.Get());

  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh", vertex_shader));
  ASSERT_TRUE(
      render_system_->SaveShader("mem:/fragment.gbsh", fragment_shader));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/texture.gbtx", texture.Get()));
  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type));
  ASSERT_TRUE(render_system_->SaveMaterial("mem:/material.gbma", material));
  EXPECT_EQ(material->GetResourceName(), "mem:/material.gbma");
  temp_resource_set_.RemoveAll();

  material = resource_system_->Load<Material>(&temp_resource_set_,
                                              "mem:/material.gbma");
  ASSERT_NE(material, nullptr);
  EXPECT_EQ(material->GetResourceId(), material_id);
  EXPECT_EQ(material->GetResourceName(), "mem:/material.gbma");
  EXPECT_NE(material->GetType(), nullptr);
  Vector2 constants = {0, 0};
  material->GetMaterialBindingData()->GetConstants<Vector2>(0, &constants);
  EXPECT_EQ(constants.x, 1);
  EXPECT_EQ(constants.y, 2);
  EXPECT_NE(material->GetDefaultInstanceBindingData()->GetTexture(1), nullptr);
}

TEST_F(RenderSystemTest, SaveLoadMeshAndDependencies) {
  CreateSystem(true);
  auto* material = CreateMaterial({});
  auto* material_type = material->GetType();
  auto* vertex_shader = material_type->GetVertexShader();
  auto* fragment_shader = material_type->GetFragmentShader();

  const int vertex_count = static_cast<int>(kCubeVertices.size());
  const int triangle_count = static_cast<int>(kCubeTriangles.size());
  auto mesh = render_system_->CreateMesh(
      material, DataVolatility::kStaticReadWrite, vertex_count, triangle_count);
  mesh->Set<Vector3>(kCubeVertices, kCubeTriangles);
  auto mesh_id = mesh->GetResourceId();

  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh", vertex_shader));
  ASSERT_TRUE(
      render_system_->SaveShader("mem:/fragment.gbsh", fragment_shader));
  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type));
  ASSERT_TRUE(render_system_->SaveMaterial("mem:/material.gbma", material));
  ASSERT_TRUE(render_system_->SaveMesh("mem:/mesh.gbme", mesh.Get()));
  EXPECT_EQ(mesh->GetResourceName(), "mem:/mesh.gbme");
  mesh.Reset();
  temp_resource_set_.RemoveAll();

  mesh = resource_system_->Load<Mesh>(&temp_resource_set_, "mem:/mesh.gbme");
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(mesh->GetResourceId(), mesh_id);
  EXPECT_EQ(mesh->GetResourceName(), "mem:/mesh.gbme");
  EXPECT_NE(mesh->GetMaterial(), nullptr);
  auto view = mesh->Edit();
  ASSERT_NE(view, nullptr);
  ASSERT_EQ(view->GetVertexCount(), vertex_count);
  for (int i = 0; i < vertex_count; ++i) {
    EXPECT_EQ(view->GetVertex<Vector3>(i), kCubeVertices[i]);
  }
  ASSERT_EQ(view->GetTriangleCount(), triangle_count);
  for (int i = 0; i < triangle_count; ++i) {
    EXPECT_EQ(view->GetTriangle(i), kCubeTriangles[i]);
  }
}

}  // namespace
}  // namespace gb
