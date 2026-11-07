// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_system.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "flatbuffers/flatbuffers.h"
#include "gb/base/context_builder.h"
#include "gb/image/image.h"
#include "gb/image/pixel.h"
#include "gb/pilot/fake_pilot.h"
#include "gb/pilot/pilot.h"
#include "gb/render/render_resource_generated.h"
#include "gb/render/render_test.h"
#include "gb/render/test_render_buffer.h"
#include "gb/render/test_shader_code.h"
#include "gb/render/test_texture.h"
#include "gb/render/test_texture_array.h"
#include "gb/render/test_volume_texture.h"
#include "gb/resource/resource_chunks.h"
#include "gb/resource/resource_file_writer.h"
#include "gb/test/log_error_guard.h"
#include "gb/test/log_recorder.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAreArray;

auto LogTextHas(std::string_view text) {
  return Field(&LogRecorder::Message::text, HasSubstr(std::string(text)));
}

class RenderSystemTest : public RenderTest {
 protected:
  // Runs a pilot frame in which the render system draws a frame, as a program
  // would.
  void RenderPilotFrame(Pilot* pilot) {
    pilot->BeginFrame();
    if (render_system_->BeginFrame()) {
      render_system_->EndFrame();
    } else {
      ADD_FAILURE() << "The render system's BeginFrame() failed";
    }
    pilot->EndFrame();
  }

  // Writes "resource" to "name" with "writer" writing its chunk in place of
  // the render system, to make files the render system wouldn't write.
  template <typename Type>
  bool WriteResource(
      std::string_view name, Type* resource, const ChunkType& chunk_type,
      ResourceFileWriter::ResourceFlatBufferWriter<Type> writer) {
    auto resource_writer =
        ResourceFileWriter::Create(ContextBuilder()
                                       .SetPtr(resource_system_.get())
                                       .SetPtr(file_system_.get())
                                       .Build());
    if (resource_writer == nullptr ||
        !resource_writer->RegisterResourceFlatBufferWriter<Type>(
            chunk_type, 1, std::move(writer))) {
      return false;
    }
    return resource_writer->Write(name, resource);
  }

  // Writes a mesh chunk with the specified vertex data and indices to "name".
  bool WriteMeshChunk(std::string_view name, const VertexType* vertex_type,
                      absl::Span<const uint8_t> vertices,
                      absl::Span<const uint16_t> indices) {
    auto mesh = render_system_->CreateMesh(
        vertex_type, DataVolatility::kStaticReadWrite, 3, 1);
    if (mesh == nullptr) {
      return false;
    }
    return WriteResource<Mesh>(
        name, mesh.Get(), kChunkTypeMesh,
        [vertex_type, vertices, indices](
            Context*, Mesh*, flatbuffers::FlatBufferBuilder* builder) {
          const auto fb_vertex_type_name =
              builder->CreateString(vertex_type->GetName());
          const auto fb_vertices =
              builder->CreateVector(vertices.data(), vertices.size());
          const auto fb_indices =
              builder->CreateVector(indices.data(), indices.size());
          fbs::MeshChunkBuilder chunk_builder(*builder);
          chunk_builder.add_vertex_type_name(fb_vertex_type_name);
          chunk_builder.add_volatility(fbs::DataVolatility_StaticWrite);
          chunk_builder.add_vertex_size(vertex_type->GetSize());
          chunk_builder.add_vertices(fb_vertices);
          chunk_builder.add_indices(fb_indices);
          builder->Finish(chunk_builder.Finish());
          return true;
        });
  }

  // Writes a storage buffer chunk to "name" of "count" values of the type named
  // "type_name", with "value_bytes" bytes of zeros for each value. A storage
  // buffer of "type" stands in for the resource being written.
  bool WriteStorageBufferChunk(std::string_view name,
                               const RenderDataType* type,
                               std::string_view type_name, int count,
                               int value_bytes) {
    auto storage_buffer = render_system_->CreateStorageBuffer(
        type, DataVolatility::kStaticReadWrite, 1);
    if (storage_buffer == nullptr) {
      return false;
    }
    return WriteResource<StorageBuffer>(
        name, storage_buffer.Get(), kChunkTypeStorageBuffer,
        [type_name, count, value_bytes](
            Context*, StorageBuffer*, flatbuffers::FlatBufferBuilder* builder) {
          const auto fb_values =
              builder->CreateVector(std::vector<uint8_t>(count * value_bytes));
          const auto fb_type_name = builder->CreateString(type_name);
          fbs::StorageBufferChunkBuilder chunk_builder(*builder);
          chunk_builder.add_volatility(fbs::DataVolatility_StaticReadWrite);
          chunk_builder.add_type_name(fb_type_name);
          chunk_builder.add_count(count);
          chunk_builder.add_values(fb_values);
          builder->Finish(chunk_builder.Finish());
          return true;
        });
  }

  // Writes a vertex shader chunk to "name" with a single material binding at
  // "index". The binding is a texture if "constants_name" is empty, and
  // constants of that type otherwise.
  bool WriteShaderChunk(std::string_view name, int index,
                        std::string_view constants_name) {
    auto shader = render_system_->CreateShader(
        ShaderType::kVertex,
        render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                         kVertexShaderCode.size()),
        {}, {}, {});
    if (shader == nullptr) {
      return false;
    }
    return WriteResource<Shader>(
        name, shader.Get(), kChunkTypeShader,
        [index, constants_name](Context*, Shader*,
                                flatbuffers::FlatBufferBuilder* builder) {
          flatbuffers::Offset<flatbuffers::String> fb_constants_name;
          if (!constants_name.empty()) {
            fb_constants_name = builder->CreateString(constants_name);
          }
          const auto fb_binding = fbs::CreateBinding(
              *builder,
              static_cast<uint8_t>(ShaderTypes(ShaderType::kVertex).GetMask()),
              fbs::BindingSet_Material, index,
              (constants_name.empty() ? fbs::BindingType_Texture
                                      : fbs::BindingType_Constants),
              fbs::DataVolatility_StaticReadWrite, fb_constants_name);
          const auto fb_bindings = builder->CreateVector(&fb_binding, 1);
          const auto fb_code = builder->CreateVector(
              reinterpret_cast<const uint8_t*>(kVertexShaderCode.data()),
              kVertexShaderCode.size());
          fbs::ShaderChunkBuilder chunk_builder(*builder);
          chunk_builder.add_type(fbs::ShaderType_Vertex);
          chunk_builder.add_bindings(fb_bindings);
          chunk_builder.add_code(fb_code);
          builder->Finish(chunk_builder.Finish());
          return true;
        });
  }

  // Writes "material_type" to "name" with "constants" as the data for its
  // material constants at index 0. Its shaders must already be saved.
  bool WriteMaterialTypeChunk(std::string_view name,
                              MaterialType* material_type,
                              absl::Span<const uint8_t> constants) {
    return WriteResource<MaterialType>(
        name, material_type, kChunkTypeMaterialType,
        [constants](Context*, MaterialType* material_type,
                    flatbuffers::FlatBufferBuilder* builder) {
          const auto fb_binding_layout_name = builder->CreateString(
              material_type->GetBindingLayout()->GetName());
          const auto fb_vertex_type_name =
              builder->CreateString(material_type->GetVertexType()->GetName());
          const auto fb_constants =
              builder->CreateVector(constants.data(), constants.size());
          const auto fb_entry = fbs::CreateBindingDataEntry(
              *builder, 0, fbs::BindingType_Constants, 0, 0, fb_constants);
          const auto fb_material_data = builder->CreateVector(&fb_entry, 1);
          fbs::MaterialTypeChunkBuilder chunk_builder(*builder);
          chunk_builder.add_vertex_shader_id(
              material_type->GetVertexShader()->GetResourceId());
          chunk_builder.add_fragment_shader_id(
              material_type->GetFragmentShader()->GetResourceId());
          chunk_builder.add_binding_layout_name(fb_binding_layout_name);
          chunk_builder.add_vertex_type_name(fb_vertex_type_name);
          chunk_builder.add_material_data(fb_material_data);
          builder->Finish(chunk_builder.Finish());
          return true;
        });
  }

  // Returns an uncompressed 32 bit TGA image of the specified size, with all
  // pixels zero.
  std::vector<uint8_t> MakeTgaImage(int width, int height) {
    std::vector<uint8_t> image(18 + width * height * 4, 0);
    image[2] = 2;  // Uncompressed true color.
    image[12] = static_cast<uint8_t>(width & 0xff);
    image[13] = static_cast<uint8_t>(width >> 8);
    image[14] = static_cast<uint8_t>(height & 0xff);
    image[15] = static_cast<uint8_t>(height >> 8);
    image[16] = 32;    // Bits per pixel.
    image[17] = 0x28;  // 8 alpha bits, with the origin at the top left.
    return image;
  }

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

  // Alternate image data with arbitrarily changing gradiants.
  std::vector<Pixel> MakeAlternateImageData() {
    std::vector<Pixel> pixels(16 * 16, Pixel(0, 0, 0, 255));
    for (int i = 0; i < 256; ++i) {
      pixels[i].r = 0;
      pixels[i].g = i % 16;
      pixels[i].b = 256 - i;
      pixels[i].a = (i / 16) * 16;
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

  EXPECT_EQ(render_system_->RegisterConstantsType<Vector3>("vec3"), type);
  EXPECT_EQ(render_system_->RegisterConstantsType<Vector4>("vec3"), nullptr);
  EXPECT_EQ(render_system_->GetConstantsType("vec3"), type);
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
            type);
  EXPECT_EQ(render_system_->RegisterVertexType<Vector4>("vertex",
                                                        {ShaderValue::kVec4}),
            nullptr);
  EXPECT_EQ(render_system_->RegisterVertexType<Vertex>(
                "vertex", {ShaderValue::kVec2, ShaderValue::kVec3}),
            nullptr);
  EXPECT_EQ(render_system_->GetVertexType("vertex"), type);
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

TEST_F(RenderSystemTest, RegisterBindingLayout) {
  CreateSystem();
  std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kVertex)
          .SetLocation(BindingSet::kView, 0)
          .SetConstants(render_system_->RegisterConstantsType<Vector3>("vec3")),
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kMaterial, 1)
          .SetTexture(),
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kMaterial, 2)
          .SetTextureArray(),
  };
  auto binding_layout =
      render_system_->RegisterBindingLayout("layout", bindings);
  ASSERT_NE(binding_layout, nullptr);
  EXPECT_EQ(binding_layout->GetName(), "layout");
  EXPECT_THAT(binding_layout->GetBindings(), ElementsAreArray(bindings));
  EXPECT_EQ(render_system_->GetBindingLayout("layout"), binding_layout);

  EXPECT_EQ(render_system_->RegisterBindingLayout("layout", bindings),
            binding_layout);
  EXPECT_EQ(render_system_->RegisterBindingLayout("layout", {}), nullptr);
  EXPECT_EQ(render_system_->RegisterBindingLayout("layout", {bindings[0]}),
            nullptr);
  EXPECT_EQ(render_system_->GetBindingLayout("layout"), binding_layout);
  EXPECT_NE(render_system_->RegisterBindingLayout("layout2", {}), nullptr);
}

TEST_F(RenderSystemTest, CreateShaderCodeFromBytes) {
  CreateSystem();
  const uint8_t bytes[] = {0x03, 0x02, 0x23, 0x07, 0x00, 0x01};
  auto code = render_system_->CreateShaderCode(absl::MakeConstSpan(bytes));
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(static_cast<TestShaderCode*>(code.get())->GetCode(),
            std::string(std::begin(bytes), std::end(bytes)));

  state_.fail_create_shader_code = true;
  EXPECT_EQ(render_system_->CreateShaderCode(absl::MakeConstSpan(bytes)),
            nullptr);
}

TEST_F(RenderSystemTest, GivesTheBackendDefaultBindings) {
  CreateSystem();
  const DefaultBindings& defaults = state_.default_bindings;

  auto* texture = static_cast<TestTexture*>(defaults.texture);
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetWidth(), 1);
  EXPECT_EQ(texture->GetHeight(), 1);
  EXPECT_THAT(texture->GetPixels(), ElementsAre(PixelColor::kWhite));

  auto* texture_array = static_cast<TestTextureArray*>(defaults.texture_array);
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetCount(), 1);
  EXPECT_EQ(texture_array->GetWidth(), 1);
  EXPECT_EQ(texture_array->GetHeight(), 1);
  EXPECT_THAT(texture_array->GetPixels(0), ElementsAre(PixelColor::kWhite));

  auto* volume_texture =
      static_cast<TestVolumeTexture*>(defaults.volume_texture);
  ASSERT_NE(volume_texture, nullptr);
  EXPECT_EQ(volume_texture->GetWidth(), 1);
  EXPECT_EQ(volume_texture->GetHeight(), 1);
  EXPECT_EQ(volume_texture->GetDepth(), 1);
  EXPECT_THAT(volume_texture->GetTexels(), ElementsAre(255, 255, 255, 255));

  auto* storage_buffer =
      static_cast<TestRenderBuffer*>(defaults.storage_buffer);
  ASSERT_NE(storage_buffer, nullptr);
  const int size_in_bytes =
      storage_buffer->GetSize() * storage_buffer->GetValueSize();
  EXPECT_EQ(size_in_bytes, 64 * 1024);
  EXPECT_THAT(absl::MakeConstSpan(
                  static_cast<const uint8_t*>(storage_buffer->GetData()),
                  size_in_bytes),
              Each(0));

  render_system_.reset();
  EXPECT_EQ(state_.default_bindings.texture, nullptr);
  EXPECT_EQ(state_.default_bindings.texture_array, nullptr);
  EXPECT_EQ(state_.default_bindings.volume_texture, nullptr);
  EXPECT_EQ(state_.default_bindings.storage_buffer, nullptr);
}

TEST_F(RenderSystemTest, CreateFailsWithoutDefaultBindings) {
  LogErrorGuard log_error_guard;
  state_.fail_create_texture = true;
  EXPECT_EQ(
      RenderSystem::Create(ContextBuilder()
                               .SetOwned<RenderBackend>(
                                   std::make_unique<TestRenderBackend>(&state_))
                               .SetOwned(ResourceSystem::Create())
                               .SetOwned(std::make_unique<FileSystem>())
                               .Build()),
      nullptr);
  EXPECT_THAT(log_error_guard.TakeMessages(),
              Contains(LogTextHas("default bindings")));
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

TEST_F(RenderSystemTest, EndFrameWithoutAPilotDoesntReadTheImage) {
  CreateSystem();
  ASSERT_TRUE(render_system_->BeginFrame());
  render_system_->EndFrame();
  EXPECT_FALSE(state_.rendering);
  EXPECT_EQ(state_.read_image_count, 0);
}

TEST_F(RenderSystemTest, PilotIsGivenOnlyTheFramesItSaves) {
  state_.frame_dimensions = {3, 2};
  FakePilot pilot;
  CreateSystem(false, &pilot);
  render_system_->SetClearColor(Pixel(10, 20, 30));

  RenderPilotFrame(&pilot);
  EXPECT_EQ(state_.read_image_count, 0);
  EXPECT_THAT(pilot.TakeSavedFrames(), IsEmpty());

  pilot.SetSavingFrames(true);
  RenderPilotFrame(&pilot);
  EXPECT_FALSE(state_.rendering);
  EXPECT_EQ(state_.invalid_call_count, 0);
  std::vector<std::unique_ptr<Image>> saved = pilot.TakeSavedFrames();
  ASSERT_THAT(saved, SizeIs(1));
  EXPECT_EQ(saved[0]->GetWidth(), 3);
  EXPECT_EQ(saved[0]->GetHeight(), 2);
  EXPECT_THAT(saved[0]->GetPixels(), Each(Pixel(10, 20, 30)));

  pilot.SetSavingFrames(false);
  RenderPilotFrame(&pilot);
  EXPECT_EQ(state_.read_image_count, 1);
  EXPECT_THAT(pilot.TakeSavedFrames(), IsEmpty());
}

TEST_F(RenderSystemTest, PilotIsGivenTheFirstFrameDrawnInItsFrame) {
  state_.frame_dimensions = {3, 2};
  FakePilot pilot;
  pilot.SetSavingFrames(true);
  CreateSystem(false, &pilot);
  pilot.BeginFrame();
  render_system_->SetClearColor(Pixel(10, 20, 30));
  ASSERT_TRUE(render_system_->BeginFrame());
  render_system_->EndFrame();
  render_system_->SetClearColor(Pixel(40, 50, 60));
  ASSERT_TRUE(render_system_->BeginFrame());
  render_system_->EndFrame();
  pilot.EndFrame();
  EXPECT_EQ(state_.read_image_count, 1);
  std::vector<std::unique_ptr<Image>> saved = pilot.TakeSavedFrames();
  ASSERT_THAT(saved, SizeIs(1));
  EXPECT_THAT(saved[0]->GetPixels(), Each(Pixel(10, 20, 30)));
}

TEST_F(RenderSystemTest, PilotFrameThatCantBeReadIsntSaved) {
  LogErrorGuard log_error_guard;
  state_.fail_read_image = true;
  FakePilot pilot;
  pilot.SetSavingFrames(true);
  CreateSystem(false, &pilot);
  RenderPilotFrame(&pilot);
  EXPECT_FALSE(state_.rendering);
  EXPECT_THAT(pilot.TakeSavedFrames(), IsEmpty());
  EXPECT_THAT(log_error_guard.TakeMessages(),
              ElementsAre(LogTextHas("set to fail")));
}

TEST_F(RenderSystemTest, LoadPngTexture) {
  CreateSystem();
  SamplerOptions expected_options;
  ASSERT_TRUE(file_system_->WriteFile("mem:/image.png", kPngImage));
  ResourcePtr<Texture> texture =
      resource_system_->Load<Texture>("mem:/image.png");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  EXPECT_EQ(texture->GetSamplerOptions(), expected_options);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  auto expected_pixels = MakeImageData();
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, LoadPngTextureWithSamplerOptions) {
  CreateSystem();
  SamplerOptions expected_options =
      SamplerOptions()
          .SetFilter(false)
          .SetMipmap(false)
          .SetAddressMode(SamplerAddressMode::kClampBorder, Pixel(32, 64, 128))
          .SetTileSize(48);
  ASSERT_TRUE(file_system_->WriteFile("mem:/image.png", kPngImage));
  ResourcePtr<Texture> texture = resource_system_->Load<Texture>(
      "mem:/image.png",
      ContextBuilder().SetValue<SamplerOptions>(expected_options).Build());
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  EXPECT_EQ(texture->GetSamplerOptions(), expected_options);
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

TEST_F(RenderSystemTest, LoadTextureFileAtMaxSize) {
  CreateSystem();
  ASSERT_TRUE(file_system_->WriteFile("mem:/wide.tga",
                                      MakeTgaImage(kMaxTextureWidth, 1)));
  ASSERT_TRUE(file_system_->WriteFile("mem:/too_wide.tga",
                                      MakeTgaImage(kMaxTextureWidth + 1, 1)));

  ResourcePtr<Texture> texture =
      resource_system_->Load<Texture>("mem:/wide.tga");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetWidth(), kMaxTextureWidth);
  EXPECT_EQ(resource_system_->Load<Texture>("mem:/too_wide.tga"), nullptr);
}

TEST_F(RenderSystemTest, SaveLoadTexture) {
  CreateSystem();
  const SamplerOptions expected_options;
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  auto texture_id = texture->GetResourceId();
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
  EXPECT_EQ(texture->GetSamplerOptions(), expected_options);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SaveLoadTextureWithSamplerOptions) {
  CreateSystem();
  SamplerOptions expected_options =
      SamplerOptions()
          .SetFilter(false)
          .SetMipmap(false)
          .SetAddressMode(SamplerAddressMode::kClampBorder, Pixel(32, 64, 128))
          .SetTileSize(48);
  auto expected_pixels = MakeImageData();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               16, 16, expected_options);
  ASSERT_NE(texture, nullptr);
  auto texture_id = texture->GetResourceId();
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
  EXPECT_EQ(texture->GetSamplerOptions(), expected_options);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SaveLoadTextureOverridingSamplerOptions) {
  CreateSystem();
  SamplerOptions expected_options =
      SamplerOptions()
          .SetFilter(false)
          .SetMipmap(false)
          .SetAddressMode(SamplerAddressMode::kClampBorder, Pixel(32, 64, 128))
          .SetTileSize(48);
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  auto texture_id = texture->GetResourceId();
  ASSERT_TRUE(texture->Set(expected_pixels));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/image.gbtx", texture.Get()));
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  texture.Reset();

  texture = resource_system_->Load<Texture>(
      "mem:/image.gbtx",
      ContextBuilder().SetValue<SamplerOptions>(expected_options).Build());
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetResourceId(), texture_id);
  EXPECT_EQ(texture->GetResourceName(), "mem:/image.gbtx");
  EXPECT_EQ(texture->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture->GetWidth(), 16);
  EXPECT_EQ(texture->GetHeight(), 16);
  EXPECT_EQ(texture->GetSamplerOptions(), expected_options);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SaveLoadTextureWithVolatility) {
  CreateSystem();
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  auto texture_id = texture->GetResourceId();
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
  ASSERT_NE(texture, nullptr);
  auto texture_id = texture->GetResourceId();
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

TEST_F(RenderSystemTest, SaveLoadTextureAtMaxSize) {
  CreateSystem();
  auto texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite,
                                               kMaxTextureWidth, 1);
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(render_system_->SaveTexture("mem:/wide.gbtx", texture.Get()));
  texture = render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 1,
                                          kMaxTextureHeight);
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(render_system_->SaveTexture("mem:/tall.gbtx", texture.Get()));
  texture.Reset();

  texture = resource_system_->Load<Texture>("mem:/wide.gbtx");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetWidth(), kMaxTextureWidth);
  texture = resource_system_->Load<Texture>("mem:/tall.gbtx");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetHeight(), kMaxTextureHeight);
}

TEST_F(RenderSystemTest, LoadTextureAfterFailedLoad) {
  CreateSystem();
  auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(texture->Set(expected_pixels));
  ASSERT_TRUE(render_system_->SaveTexture("mem:/image.gbtx", texture.Get()));
  texture.Reset();

  state_.texture_config.fail_set = true;
  EXPECT_EQ(resource_system_->Load<Texture>("mem:/image.gbtx"), nullptr);
  state_.texture_config.fail_set = false;
  texture = resource_system_->Load<Texture>("mem:/image.gbtx");
  ASSERT_NE(texture, nullptr);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, SaveLoadTextureArray) {
  CreateSystem();
  const SamplerOptions expected_options;
  std::vector<Pixel> expected_pixels[] = {
      MakeImageData(),
      MakeAlternateImageData(),
  };
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 2, 16, 16);
  ASSERT_NE(texture_array, nullptr);
  auto texture_array_id = texture_array->GetResourceId();
  ASSERT_TRUE(texture_array->Set(0, expected_pixels[0]));
  ASSERT_TRUE(texture_array->Set(1, expected_pixels[1]));
  ASSERT_TRUE(render_system_->SaveTextureArray("mem:/texture_array.gbta",
                                               texture_array.Get()));
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  texture_array.Reset();

  texture_array =
      resource_system_->Load<TextureArray>("mem:/texture_array.gbta");
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetResourceId(), texture_array_id);
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  EXPECT_EQ(texture_array->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture_array->GetCount(), 2);
  EXPECT_EQ(texture_array->GetWidth(), 16);
  EXPECT_EQ(texture_array->GetHeight(), 16);
  EXPECT_EQ(texture_array->GetSamplerOptions(), expected_options);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());
  EXPECT_THAT(test_texture_array->GetPixels(0),
              ElementsAreArray(expected_pixels[0]));
  EXPECT_THAT(test_texture_array->GetPixels(1),
              ElementsAreArray(expected_pixels[1]));
}

TEST_F(RenderSystemTest, SaveLoadTextureArrayWithSamplerOptions) {
  CreateSystem();
  SamplerOptions expected_options =
      SamplerOptions()
          .SetFilter(false)
          .SetMipmap(false)
          .SetAddressMode(SamplerAddressMode::kClampBorder, Pixel(32, 64, 128))
          .SetTileSize(48);
  std::vector<Pixel> expected_pixels[] = {
      MakeImageData(),
      MakeAlternateImageData(),
  };
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 2, 16, 16, expected_options);
  ASSERT_NE(texture_array, nullptr);
  auto texture_array_id = texture_array->GetResourceId();
  ASSERT_TRUE(texture_array->Set(0, expected_pixels[0]));
  ASSERT_TRUE(texture_array->Set(1, expected_pixels[1]));
  ASSERT_TRUE(render_system_->SaveTextureArray("mem:/texture_array.gbta",
                                               texture_array.Get()));
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  texture_array.Reset();

  texture_array =
      resource_system_->Load<TextureArray>("mem:/texture_array.gbta");
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetResourceId(), texture_array_id);
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  EXPECT_EQ(texture_array->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture_array->GetCount(), 2);
  EXPECT_EQ(texture_array->GetWidth(), 16);
  EXPECT_EQ(texture_array->GetHeight(), 16);
  EXPECT_EQ(texture_array->GetSamplerOptions(), expected_options);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());
  EXPECT_THAT(test_texture_array->GetPixels(0),
              ElementsAreArray(expected_pixels[0]));
  EXPECT_THAT(test_texture_array->GetPixels(1),
              ElementsAreArray(expected_pixels[1]));
}

TEST_F(RenderSystemTest, SaveLoadTextureArrayOverridingSamplerOptions) {
  CreateSystem();
  SamplerOptions expected_options =
      SamplerOptions()
          .SetFilter(false)
          .SetMipmap(false)
          .SetAddressMode(SamplerAddressMode::kClampBorder, Pixel(32, 64, 128))
          .SetTileSize(48);
  std::vector<Pixel> expected_pixels[] = {
      MakeImageData(),
      MakeAlternateImageData(),
  };
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 2, 16, 16);
  ASSERT_NE(texture_array, nullptr);
  auto texture_array_id = texture_array->GetResourceId();
  ASSERT_TRUE(texture_array->Set(0, expected_pixels[0]));
  ASSERT_TRUE(texture_array->Set(1, expected_pixels[1]));
  ASSERT_TRUE(render_system_->SaveTextureArray("mem:/texture_array.gbta",
                                               texture_array.Get()));
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  texture_array.Reset();

  texture_array = resource_system_->Load<TextureArray>(
      "mem:/texture_array.gbta",
      ContextBuilder().SetValue<SamplerOptions>(expected_options).Build());
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetResourceId(), texture_array_id);
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  EXPECT_EQ(texture_array->GetVolatility(), DataVolatility::kStaticWrite);
  EXPECT_EQ(texture_array->GetCount(), 2);
  EXPECT_EQ(texture_array->GetWidth(), 16);
  EXPECT_EQ(texture_array->GetHeight(), 16);
  EXPECT_EQ(texture_array->GetSamplerOptions(), expected_options);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());
  EXPECT_THAT(test_texture_array->GetPixels(0),
              ElementsAreArray(expected_pixels[0]));
  EXPECT_THAT(test_texture_array->GetPixels(1),
              ElementsAreArray(expected_pixels[1]));
}

TEST_F(RenderSystemTest, SaveLoadTextureArrayWithVolatility) {
  CreateSystem();
  std::vector<Pixel> expected_pixels[] = {
      MakeImageData(),
      MakeAlternateImageData(),
  };
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 2, 16, 16);
  ASSERT_NE(texture_array, nullptr);
  auto texture_array_id = texture_array->GetResourceId();
  ASSERT_TRUE(texture_array->Set(0, expected_pixels[0]));
  ASSERT_TRUE(texture_array->Set(1, expected_pixels[1]));
  ASSERT_TRUE(render_system_->SaveTextureArray("mem:/texture_array.gbta",
                                               texture_array.Get(),
                                               DataVolatility::kPerFrame));
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  texture_array.Reset();

  texture_array =
      resource_system_->Load<TextureArray>("mem:/texture_array.gbta");
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetResourceId(), texture_array_id);
  EXPECT_EQ(texture_array->GetResourceName(), "mem:/texture_array.gbta");
  EXPECT_EQ(texture_array->GetVolatility(), DataVolatility::kPerFrame);
  EXPECT_EQ(texture_array->GetCount(), 2);
  EXPECT_EQ(texture_array->GetWidth(), 16);
  EXPECT_EQ(texture_array->GetHeight(), 16);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());
  EXPECT_THAT(test_texture_array->GetPixels(0),
              ElementsAreArray(expected_pixels[0]));
  EXPECT_THAT(test_texture_array->GetPixels(1),
              ElementsAreArray(expected_pixels[1]));
}

TEST_F(RenderSystemTest, SaveLoadTextureArrayAtMaxSize) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 1, kMaxTextureWidth, 1);
  ASSERT_NE(texture_array, nullptr);
  ASSERT_TRUE(
      render_system_->SaveTextureArray("mem:/wide.gbta", texture_array.Get()));
  texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 1, 1, kMaxTextureHeight);
  ASSERT_NE(texture_array, nullptr);
  ASSERT_TRUE(
      render_system_->SaveTextureArray("mem:/tall.gbta", texture_array.Get()));
  texture_array.Reset();

  texture_array = resource_system_->Load<TextureArray>("mem:/wide.gbta");
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetWidth(), kMaxTextureWidth);
  texture_array = resource_system_->Load<TextureArray>("mem:/tall.gbta");
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetHeight(), kMaxTextureHeight);
}

TEST_F(RenderSystemTest, LoadTextureArrayAfterFailedLoad) {
  CreateSystem();
  auto expected_pixels = MakeImageData();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 1, 16, 16);
  ASSERT_NE(texture_array, nullptr);
  ASSERT_TRUE(texture_array->Set(0, expected_pixels));
  ASSERT_TRUE(render_system_->SaveTextureArray("mem:/texture_array.gbta",
                                               texture_array.Get()));
  texture_array.Reset();

  state_.texture_array_config.fail_set = true;
  EXPECT_EQ(resource_system_->Load<TextureArray>("mem:/texture_array.gbta"),
            nullptr);
  state_.texture_array_config.fail_set = false;
  texture_array =
      resource_system_->Load<TextureArray>("mem:/texture_array.gbta");
  ASSERT_NE(texture_array, nullptr);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());
  EXPECT_THAT(test_texture_array->GetPixels(0),
              ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, LoadTextureArrayWithInvalidCount) {
  CreateSystem();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 1, 16, 16);
  ASSERT_NE(texture_array, nullptr);
  ASSERT_TRUE(WriteResource<TextureArray>(
      "mem:/texture_array.gbta", texture_array.Get(), kChunkTypeTextureArray,
      [](Context*, TextureArray*, flatbuffers::FlatBufferBuilder* builder) {
        const auto fb_pixels = builder->CreateVector(std::vector<uint32_t>());
        fbs::TextureArrayChunkBuilder chunk_builder(*builder);
        chunk_builder.add_count(0);
        chunk_builder.add_width(16);
        chunk_builder.add_height(16);
        chunk_builder.add_pixels(fb_pixels);
        builder->Finish(chunk_builder.Finish());
        return true;
      }));
  texture_array.Reset();

  EXPECT_EQ(resource_system_->Load<TextureArray>("mem:/texture_array.gbta"),
            nullptr);
}

TEST_F(RenderSystemTest, SaveLoadTextureInEachFormat) {
  CreateSystem();
  for (TextureFormat format :
       {TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
        TextureFormat::kR8, TextureFormat::kRgba4}) {
    std::vector<uint8_t> expected_texels(16 * 8 * GetTexelSize(format));
    for (int i = 0; i < static_cast<int>(expected_texels.size()); ++i) {
      expected_texels[i] = static_cast<uint8_t>(i * 7);
    }
    auto texture = render_system_->CreateTexture(
        DataVolatility::kStaticReadWrite, 16, 8, SamplerOptions(), format);
    ASSERT_NE(texture, nullptr);
    ASSERT_TRUE(texture->Set(expected_texels.data(), expected_texels.size()));
    const std::string name =
        absl::StrCat("mem:/texture_", static_cast<int>(format), ".gbtx");
    ASSERT_TRUE(render_system_->SaveTexture(name, texture.Get()));
    texture.Reset();

    texture = resource_system_->Load<Texture>(name);
    ASSERT_NE(texture, nullptr) << name;
    EXPECT_EQ(texture->GetFormat(), format);
    EXPECT_EQ(texture->GetWidth(), 16);
    EXPECT_EQ(texture->GetHeight(), 8);
    auto* test_texture = static_cast<TestTexture*>(texture.Get());
    EXPECT_THAT(test_texture->GetTexels(), ElementsAreArray(expected_texels))
        << name;
  }
}

TEST_F(RenderSystemTest, SaveLoadTextureArrayInEachFormat) {
  CreateSystem();
  for (TextureFormat format :
       {TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
        TextureFormat::kR8, TextureFormat::kRgba4}) {
    const int texture_size = 16 * 8 * GetTexelSize(format);
    std::vector<uint8_t> expected_texels(texture_size * 2);
    for (int i = 0; i < static_cast<int>(expected_texels.size()); ++i) {
      expected_texels[i] = static_cast<uint8_t>(i * 7);
    }
    auto texture_array = render_system_->CreateTextureArray(
        DataVolatility::kStaticReadWrite, 2, 16, 8, SamplerOptions(), format);
    ASSERT_NE(texture_array, nullptr);
    ASSERT_TRUE(texture_array->Set(0, expected_texels.data(), texture_size));
    ASSERT_TRUE(texture_array->Set(1, expected_texels.data() + texture_size,
                                   texture_size));
    const std::string name =
        absl::StrCat("mem:/texture_array_", static_cast<int>(format), ".gbta");
    ASSERT_TRUE(render_system_->SaveTextureArray(name, texture_array.Get()));
    texture_array.Reset();

    texture_array = resource_system_->Load<TextureArray>(name);
    ASSERT_NE(texture_array, nullptr) << name;
    EXPECT_EQ(texture_array->GetFormat(), format);
    auto* test_texture_array =
        static_cast<TestTextureArray*>(texture_array.Get());
    EXPECT_THAT(test_texture_array->GetTexels(0),
                ElementsAreArray(expected_texels.data(), texture_size))
        << name;
    EXPECT_THAT(
        test_texture_array->GetTexels(1),
        ElementsAreArray(expected_texels.data() + texture_size, texture_size))
        << name;
  }
}

TEST_F(RenderSystemTest, LoadTextureSavedAsPixels) {
  CreateSystem();
  const auto expected_pixels = MakeImageData();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(WriteResource<Texture>(
      "mem:/image.gbtx", texture.Get(), kChunkTypeTexture,
      [&expected_pixels](Context*, Texture*,
                         flatbuffers::FlatBufferBuilder* builder) {
        std::vector<uint32_t> packed_pixels;
        for (const Pixel& pixel : expected_pixels) {
          packed_pixels.push_back(pixel.Packed());
        }
        const auto fb_pixels = builder->CreateVector(packed_pixels);
        fbs::TextureChunkBuilder chunk_builder(*builder);
        chunk_builder.add_width(16);
        chunk_builder.add_height(16);
        chunk_builder.add_pixels(fb_pixels);
        builder->Finish(chunk_builder.Finish());
        return true;
      }));
  texture.Reset();

  texture = resource_system_->Load<Texture>("mem:/image.gbtx");
  ASSERT_NE(texture, nullptr);
  EXPECT_EQ(texture->GetFormat(), TextureFormat::kRgba8Srgb);
  auto* test_texture = static_cast<TestTexture*>(texture.Get());
  EXPECT_THAT(test_texture->GetPixels(), ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, LoadTextureArraySavedAsPixels) {
  CreateSystem();
  const auto expected_pixels = MakeImageData();
  auto texture_array = render_system_->CreateTextureArray(
      DataVolatility::kStaticReadWrite, 1, 16, 16);
  ASSERT_NE(texture_array, nullptr);
  ASSERT_TRUE(WriteResource<TextureArray>(
      "mem:/texture_array.gbta", texture_array.Get(), kChunkTypeTextureArray,
      [&expected_pixels](Context*, TextureArray*,
                         flatbuffers::FlatBufferBuilder* builder) {
        std::vector<uint32_t> packed_pixels;
        for (const Pixel& pixel : expected_pixels) {
          packed_pixels.push_back(pixel.Packed());
        }
        const auto fb_pixels = builder->CreateVector(packed_pixels);
        fbs::TextureArrayChunkBuilder chunk_builder(*builder);
        chunk_builder.add_count(1);
        chunk_builder.add_width(16);
        chunk_builder.add_height(16);
        chunk_builder.add_pixels(fb_pixels);
        builder->Finish(chunk_builder.Finish());
        return true;
      }));
  texture_array.Reset();

  texture_array =
      resource_system_->Load<TextureArray>("mem:/texture_array.gbta");
  ASSERT_NE(texture_array, nullptr);
  EXPECT_EQ(texture_array->GetFormat(), TextureFormat::kRgba8Srgb);
  auto* test_texture_array =
      static_cast<TestTextureArray*>(texture_array.Get());
  EXPECT_THAT(test_texture_array->GetPixels(0),
              ElementsAreArray(expected_pixels));
}

TEST_F(RenderSystemTest, LoadTextureWithTooFewTexels) {
  CreateSystem();
  auto texture =
      render_system_->CreateTexture(DataVolatility::kStaticReadWrite, 16, 16);
  ASSERT_NE(texture, nullptr);
  ASSERT_TRUE(WriteResource<Texture>(
      "mem:/image.gbtx", texture.Get(), kChunkTypeTexture,
      [](Context*, Texture*, flatbuffers::FlatBufferBuilder* builder) {
        const auto fb_texels =
            builder->CreateVector(std::vector<uint8_t>(16 * 16 * 2 - 1));
        fbs::TextureChunkBuilder chunk_builder(*builder);
        chunk_builder.add_width(16);
        chunk_builder.add_height(16);
        chunk_builder.add_format(fbs::TextureFormat_Rg8);
        chunk_builder.add_texels(fb_texels);
        builder->Finish(chunk_builder.Finish());
        return true;
      }));
  texture.Reset();

  EXPECT_EQ(resource_system_->Load<Texture>("mem:/image.gbtx"), nullptr);
}

TEST_F(RenderSystemTest, SaveLoadVolumeTextureInEachFormat) {
  CreateSystem();
  const SamplerOptions options =
      SamplerOptions().SetMipmap(false).SetFilter(false).SetAddressMode(
          SamplerAddressMode::kClampBorder, Pixel(255, 255, 255));
  for (TextureFormat format :
       {TextureFormat::kRgba8Srgb, TextureFormat::kRgba8, TextureFormat::kRg8,
        TextureFormat::kR8, TextureFormat::kRgba4}) {
    std::vector<uint8_t> expected_texels(
        GetTextureSizeInBytes(format, 8, 4, 2));
    for (int i = 0; i < static_cast<int>(expected_texels.size()); ++i) {
      expected_texels[i] = static_cast<uint8_t>(i * 7);
    }
    auto volume_texture = render_system_->CreateVolumeTexture(
        DataVolatility::kStaticReadWrite, 8, 4, 2, options, format);
    ASSERT_NE(volume_texture, nullptr);
    const ResourceId volume_texture_id = volume_texture->GetResourceId();
    ASSERT_TRUE(
        volume_texture->Set(expected_texels.data(), expected_texels.size()));
    const std::string name =
        absl::StrCat("mem:/volume_", static_cast<int>(format), ".gbvt");
    ASSERT_TRUE(render_system_->SaveVolumeTexture(name, volume_texture.Get(),
                                                  DataVolatility::kPerFrame));
    EXPECT_EQ(volume_texture->GetResourceName(), name);
    volume_texture.Reset();

    volume_texture = resource_system_->Load<VolumeTexture>(name);
    ASSERT_NE(volume_texture, nullptr) << name;
    EXPECT_EQ(volume_texture->GetResourceId(), volume_texture_id);
    EXPECT_EQ(volume_texture->GetVolatility(), DataVolatility::kPerFrame);
    EXPECT_EQ(volume_texture->GetWidth(), 8);
    EXPECT_EQ(volume_texture->GetHeight(), 4);
    EXPECT_EQ(volume_texture->GetDepth(), 2);
    EXPECT_EQ(volume_texture->GetFormat(), format);
    EXPECT_EQ(volume_texture->GetSamplerOptions(), options);
    EXPECT_THAT(
        static_cast<TestVolumeTexture*>(volume_texture.Get())->GetTexels(),
        ElementsAreArray(expected_texels))
        << name;
  }
}

TEST_F(RenderSystemTest, LoadVolumeTextureOverridingSamplerOptions) {
  CreateSystem();
  auto volume_texture = render_system_->CreateVolumeTexture(
      DataVolatility::kStaticReadWrite, 8, 4, 2,
      SamplerOptions().SetMipmap(false));
  ASSERT_NE(volume_texture, nullptr);
  ASSERT_TRUE(volume_texture->Clear());
  ASSERT_TRUE(render_system_->SaveVolumeTexture("mem:/volume.gbvt",
                                                volume_texture.Get()));
  volume_texture.Reset();

  // The override's mipmap options are for 2D textures, and are ignored.
  volume_texture = resource_system_->Load<VolumeTexture>(
      "mem:/volume.gbvt",
      ContextBuilder()
          .SetValue<SamplerOptions>(
              SamplerOptions()
                  .SetFilter(false)
                  .SetAddressMode(SamplerAddressMode::kClampEdge)
                  .SetTileSize(16))
          .Build());
  ASSERT_NE(volume_texture, nullptr);
  EXPECT_EQ(volume_texture->GetSamplerOptions(),
            SamplerOptions().SetFilter(false).SetMipmap(false).SetAddressMode(
                SamplerAddressMode::kClampEdge));
}

TEST_F(RenderSystemTest, LoadVolumeTextureAfterFailedLoad) {
  CreateSystem();
  auto volume_texture = render_system_->CreateVolumeTexture(
      DataVolatility::kStaticReadWrite, 8, 4, 2,
      SamplerOptions().SetMipmap(false));
  ASSERT_NE(volume_texture, nullptr);
  ASSERT_TRUE(volume_texture->Clear());
  ASSERT_TRUE(render_system_->SaveVolumeTexture("mem:/volume.gbvt",
                                                volume_texture.Get()));
  volume_texture.Reset();

  state_.volume_texture_config.fail_set = true;
  EXPECT_EQ(resource_system_->Load<VolumeTexture>("mem:/volume.gbvt"), nullptr);
  state_.volume_texture_config.fail_set = false;
  EXPECT_NE(resource_system_->Load<VolumeTexture>("mem:/volume.gbvt"), nullptr);
}

TEST_F(RenderSystemTest, SaveVolumeTextureWithStaticWriteFails) {
  CreateSystem();
  auto volume_texture = render_system_->CreateVolumeTexture(
      DataVolatility::kStaticWrite, 8, 4, 2, SamplerOptions().SetMipmap(false));
  ASSERT_NE(volume_texture, nullptr);
  EXPECT_FALSE(render_system_->SaveVolumeTexture("mem:/volume.gbvt",
                                                 volume_texture.Get()));
}

TEST_F(RenderSystemTest, SaveShaderNotInEditMode) {
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
                                     {ShaderValue::kVec2, 1},
                                     {ShaderValue::kU16Vec4, 2}};
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

  // A loaded shader keeps its code in edit mode, so it can be saved again.
  EXPECT_TRUE(render_system_->SaveShader("mem:/shader.gbsh", shader.Get()));
}

TEST_F(RenderSystemTest, LoadShaderWithInvalidType) {
  CreateSystem();
  auto shader = render_system_->CreateShader(
      ShaderType::kVertex,
      render_system_->CreateShaderCode(kVertexShaderCode.data(),
                                       kVertexShaderCode.size()),
      {}, {}, {});
  ASSERT_NE(shader, nullptr);
  ASSERT_TRUE(WriteResource<Shader>(
      "mem:/shader.gbsh", shader.Get(), kChunkTypeShader,
      [](Context*, Shader*, flatbuffers::FlatBufferBuilder* builder) {
        const auto fb_code = builder->CreateVector(
            reinterpret_cast<const uint8_t*>(kVertexShaderCode.data()),
            kVertexShaderCode.size());
        fbs::ShaderChunkBuilder chunk_builder(*builder);
        chunk_builder.add_type(static_cast<fbs::ShaderType>(2));
        chunk_builder.add_code(fb_code);
        builder->Finish(chunk_builder.Finish());
        return true;
      }));
  shader.Reset();

  EXPECT_EQ(resource_system_->Load<Shader>("mem:/shader.gbsh"), nullptr);
}

TEST_F(RenderSystemTest, LoadShaderWithInvalidBindings) {
  CreateSystem();
  ASSERT_NE(render_system_->RegisterConstantsType<Vector2>("vec2"), nullptr);
  ASSERT_TRUE(WriteShaderChunk("mem:/valid.gbsh", 0, "vec2"));
  ASSERT_TRUE(WriteShaderChunk("mem:/unknown_constants.gbsh", 0, "unknown"));
  ASSERT_TRUE(
      WriteShaderChunk("mem:/invalid_index.gbsh", kMaxBindingIndex + 1, ""));

  EXPECT_NE(resource_system_->Load<Shader>("mem:/valid.gbsh"), nullptr);
  EXPECT_EQ(resource_system_->Load<Shader>("mem:/unknown_constants.gbsh"),
            nullptr);
  EXPECT_EQ(resource_system_->Load<Shader>("mem:/invalid_index.gbsh"), nullptr);
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
  const MaterialConfig config =
      MaterialConfig()
          .SetCullMode(CullMode::kFront)
          .SetDepthMode(DepthMode::kTest, DepthTest::kGreater)
          .SetRasterMode(RasterMode::kLine)
          .SetBlendMode(BlendMode::kAdditive)
          .SetColorChannels({ColorChannel::kRed, ColorChannel::kAlpha})
          .SetDepthBias(1.5f, 2.5f);
  auto* material_type_ptr = CreateMaterialType(bindings, config);
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
  EXPECT_EQ(material_type->GetConfig(), config);
  Vector2 constants = {0, 0};
  material_type->GetDefaultMaterialBindingData()->GetConstants<Vector2>(
      0, &constants);
  EXPECT_EQ(constants.x, 1);
  EXPECT_EQ(constants.y, 2);
  EXPECT_EQ(material_type->GetDefaultInstanceBindingData()->GetTexture(1),
            texture.Get());
}

TEST_F(RenderSystemTest, SaveLoadMaterialTypeWithSameIndexInOtherSet) {
  CreateSystem(true);
  auto* constants_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(constants_type, nullptr);
  auto* material_type_ptr = CreateMaterialType({
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kView, 0)
          .SetTexture(),
      Binding()
          .SetShaders(kAllShaderTypes)
          .SetLocation(BindingSet::kMaterial, 0)
          .SetConstants(constants_type),
  });
  ASSERT_NE(material_type_ptr, nullptr);
  material_type_ptr->GetDefaultMaterialBindingData()->SetConstants<Vector2>(
      0, {1, 2});
  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh",
                                         material_type_ptr->GetVertexShader()));
  ASSERT_TRUE(render_system_->SaveShader(
      "mem:/fragment.gbsh", material_type_ptr->GetFragmentShader()));
  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type_ptr));
  temp_resource_set_.RemoveAll();

  auto material_type = resource_system_->Load<MaterialType>(
      &temp_resource_set_, "mem:/material_type.gbmt");
  ASSERT_NE(material_type, nullptr);
  Vector2 constants = {0, 0};
  material_type->GetDefaultMaterialBindingData()->GetConstants<Vector2>(
      0, &constants);
  EXPECT_EQ(constants, Vector2({1, 2}));
}

TEST_F(RenderSystemTest, LoadMaterialTypeWithWrongConstantsSize) {
  CreateSystem(true);
  auto* constants_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(constants_type, nullptr);
  auto* material_type =
      CreateMaterialType({Binding()
                              .SetShaders(kAllShaderTypes)
                              .SetLocation(BindingSet::kMaterial, 0)
                              .SetConstants(constants_type)});
  ASSERT_NE(material_type, nullptr);
  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh",
                                         material_type->GetVertexShader()));
  ASSERT_TRUE(render_system_->SaveShader("mem:/fragment.gbsh",
                                         material_type->GetFragmentShader()));
  const uint8_t constants[sizeof(Vector2) / 2] = {};
  ASSERT_TRUE(WriteMaterialTypeChunk("mem:/material_type.gbmt", material_type,
                                     constants));
  temp_resource_set_.RemoveAll();

  EXPECT_EQ(resource_system_->Load<MaterialType>(&temp_resource_set_,
                                                 "mem:/material_type.gbmt"),
            nullptr);
}

TEST_F(RenderSystemTest, LoadMaterialTypeWithoutBlendOrDepthBias) {
  CreateSystem(true);
  auto* constants_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(constants_type, nullptr);
  auto* material_type =
      CreateMaterialType({Binding()
                              .SetShaders(kAllShaderTypes)
                              .SetLocation(BindingSet::kMaterial, 0)
                              .SetConstants(constants_type)});
  ASSERT_NE(material_type, nullptr);
  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh",
                                         material_type->GetVertexShader()));
  ASSERT_TRUE(render_system_->SaveShader("mem:/fragment.gbsh",
                                         material_type->GetFragmentShader()));

  // The chunk has none of the fields added for blending, color channels, or
  // depth bias, like material types saved before they were added.
  const uint8_t constants[sizeof(Vector2)] = {};
  ASSERT_TRUE(WriteMaterialTypeChunk("mem:/material_type.gbmt", material_type,
                                     constants));
  temp_resource_set_.RemoveAll();

  auto* loaded = resource_system_->Load<MaterialType>(
      &temp_resource_set_, "mem:/material_type.gbmt");
  ASSERT_NE(loaded, nullptr);
  const MaterialConfig& config = loaded->GetConfig();
  EXPECT_EQ(config.blend_mode, BlendMode::kAlpha);
  EXPECT_EQ(config.color_channels, kAllColorChannels);
  EXPECT_EQ(config.depth_bias_constant, 0.0f);
  EXPECT_EQ(config.depth_bias_slope, 0.0f);
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

TEST_F(RenderSystemTest, SaveLoadMaterialWithVolumeTexture) {
  CreateSystem(true);
  const std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kInstance, 1)
          .SetVolumeTexture(),
  };
  auto* material = CreateMaterial(bindings);
  auto* material_type = material->GetType();
  auto volume_texture = render_system_->CreateVolumeTexture(
      DataVolatility::kStaticReadWrite, 4, 4, 4,
      SamplerOptions().SetMipmap(false));
  ASSERT_NE(volume_texture, nullptr);
  ASSERT_TRUE(volume_texture->Clear());
  material->GetDefaultInstanceBindingData()->SetVolumeTexture(
      1, volume_texture.Get());

  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh",
                                         material_type->GetVertexShader()));
  ASSERT_TRUE(render_system_->SaveShader("mem:/fragment.gbsh",
                                         material_type->GetFragmentShader()));
  ASSERT_TRUE(render_system_->SaveVolumeTexture("mem:/volume.gbvt",
                                                volume_texture.Get()));
  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type));
  ASSERT_TRUE(render_system_->SaveMaterial("mem:/material.gbma", material));
  volume_texture.Reset();
  temp_resource_set_.RemoveAll();

  material = resource_system_->Load<Material>(&temp_resource_set_,
                                              "mem:/material.gbma");
  ASSERT_NE(material, nullptr);
  EXPECT_THAT(material->GetType()->GetBindings(),
              UnorderedElementsAreArray(bindings));
  const VolumeTexture* loaded_volume_texture =
      material->GetDefaultInstanceBindingData()->GetVolumeTexture(1);
  ASSERT_NE(loaded_volume_texture, nullptr);
  EXPECT_EQ(loaded_volume_texture->GetResourceName(), "mem:/volume.gbvt");
}

TEST_F(RenderSystemTest, SaveLoadStorageBuffer) {
  CreateSystem();
  auto* vector2_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(vector2_type, nullptr);
  auto storage_buffer = render_system_->CreateStorageBuffer(
      vector2_type, DataVolatility::kStaticReadWrite, 3);
  ASSERT_NE(storage_buffer, nullptr);
  const ResourceId storage_buffer_id = storage_buffer->GetResourceId();
  const std::vector<Vector2> values = {{1, 2}, {3, 4}, {5, 6}};
  ASSERT_TRUE(storage_buffer->Set<Vector2>(0, values));
  ASSERT_TRUE(render_system_->SaveStorageBuffer(
      "mem:/buffer.gbsb", storage_buffer.Get(), DataVolatility::kPerFrame));
  EXPECT_EQ(storage_buffer->GetResourceName(), "mem:/buffer.gbsb");
  storage_buffer.Reset();

  storage_buffer = resource_system_->Load<StorageBuffer>("mem:/buffer.gbsb");
  ASSERT_NE(storage_buffer, nullptr);
  EXPECT_EQ(storage_buffer->GetResourceId(), storage_buffer_id);
  EXPECT_EQ(storage_buffer->GetVolatility(), DataVolatility::kPerFrame);
  EXPECT_EQ(storage_buffer->GetType(), vector2_type);
  EXPECT_EQ(storage_buffer->GetCount(), 3);
  std::vector<Vector2> loaded_values(3);
  ASSERT_TRUE(storage_buffer->Get<Vector2>(0, absl::MakeSpan(loaded_values)));
  EXPECT_THAT(loaded_values, ElementsAreArray(values));
}

TEST_F(RenderSystemTest, LoadStorageBufferAfterFailedLoad) {
  CreateSystem();
  auto* vector2_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(vector2_type, nullptr);
  auto storage_buffer = render_system_->CreateStorageBuffer(
      vector2_type, DataVolatility::kStaticReadWrite, 3);
  ASSERT_NE(storage_buffer, nullptr);
  ASSERT_TRUE(render_system_->SaveStorageBuffer("mem:/buffer.gbsb",
                                                storage_buffer.Get()));
  storage_buffer.Reset();

  state_.storage_buffer_config.fail_set = true;
  EXPECT_EQ(resource_system_->Load<StorageBuffer>("mem:/buffer.gbsb"), nullptr);
  state_.storage_buffer_config.fail_set = false;
  EXPECT_NE(resource_system_->Load<StorageBuffer>("mem:/buffer.gbsb"), nullptr);
}

TEST_F(RenderSystemTest, SaveStorageBufferWithStaticWriteFails) {
  CreateSystem();
  auto* vector2_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(vector2_type, nullptr);
  auto storage_buffer = render_system_->CreateStorageBuffer(
      vector2_type, DataVolatility::kStaticWrite, 3);
  ASSERT_NE(storage_buffer, nullptr);
  EXPECT_FALSE(render_system_->SaveStorageBuffer("mem:/buffer.gbsb",
                                                 storage_buffer.Get()));
}

TEST_F(RenderSystemTest, LoadStorageBufferValidatesType) {
  CreateSystem();
  auto* vector2_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(vector2_type, nullptr);
  const int vector2_bytes = static_cast<int>(sizeof(Vector2));
  ASSERT_TRUE(WriteStorageBufferChunk("mem:/valid.gbsb", vector2_type, "vec2",
                                      3, vector2_bytes));
  ASSERT_TRUE(WriteStorageBufferChunk("mem:/unknown.gbsb", vector2_type,
                                      "unknown", 3, vector2_bytes));
  ASSERT_TRUE(WriteStorageBufferChunk("mem:/wrong_size.gbsb", vector2_type,
                                      "vec2", 3,
                                      static_cast<int>(sizeof(Vector3))));
  ASSERT_TRUE(WriteStorageBufferChunk("mem:/empty.gbsb", vector2_type, "vec2",
                                      0, vector2_bytes));

  EXPECT_NE(resource_system_->Load<StorageBuffer>("mem:/valid.gbsb"), nullptr);
  EXPECT_EQ(resource_system_->Load<StorageBuffer>("mem:/unknown.gbsb"),
            nullptr);
  EXPECT_EQ(resource_system_->Load<StorageBuffer>("mem:/wrong_size.gbsb"),
            nullptr);
  EXPECT_EQ(resource_system_->Load<StorageBuffer>("mem:/empty.gbsb"), nullptr);
}

TEST_F(RenderSystemTest, SaveLoadMaterialWithStorageBuffer) {
  CreateSystem(true);
  auto* vector2_type = render_system_->RegisterConstantsType<Vector2>("vec2");
  ASSERT_NE(vector2_type, nullptr);
  const std::vector<Binding> bindings = {
      Binding()
          .SetShaders(ShaderType::kFragment)
          .SetLocation(BindingSet::kInstance, 1)
          .SetStorageBuffer(vector2_type),
  };
  auto* material = CreateMaterial(bindings);
  auto* material_type = material->GetType();
  auto storage_buffer = render_system_->CreateStorageBuffer(
      vector2_type, DataVolatility::kStaticReadWrite, 4);
  ASSERT_NE(storage_buffer, nullptr);
  material->GetDefaultInstanceBindingData()->SetStorageBuffer(
      1, storage_buffer.Get());

  ASSERT_TRUE(render_system_->SaveShader("mem:/vertex.gbsh",
                                         material_type->GetVertexShader()));
  ASSERT_TRUE(render_system_->SaveShader("mem:/fragment.gbsh",
                                         material_type->GetFragmentShader()));
  ASSERT_TRUE(render_system_->SaveStorageBuffer("mem:/buffer.gbsb",
                                                storage_buffer.Get()));
  ASSERT_TRUE(render_system_->SaveMaterialType("mem:/material_type.gbmt",
                                               material_type));
  ASSERT_TRUE(render_system_->SaveMaterial("mem:/material.gbma", material));
  storage_buffer.Reset();
  temp_resource_set_.RemoveAll();

  material = resource_system_->Load<Material>(&temp_resource_set_,
                                              "mem:/material.gbma");
  ASSERT_NE(material, nullptr);
  EXPECT_THAT(material->GetType()->GetBindings(),
              UnorderedElementsAreArray(bindings));
  const StorageBuffer* loaded_storage_buffer =
      material->GetDefaultInstanceBindingData()->GetStorageBuffer(1);
  ASSERT_NE(loaded_storage_buffer, nullptr);
  EXPECT_EQ(loaded_storage_buffer->GetResourceName(), "mem:/buffer.gbsb");
  EXPECT_EQ(loaded_storage_buffer->GetType(), vector2_type);
}

TEST_F(RenderSystemTest, SaveLoadMeshAndDependencies) {
  CreateSystem(true);
  auto* material_type = CreateMaterialType({});

  const int vertex_count = static_cast<int>(kCubeVertices.size());
  const int triangle_count = static_cast<int>(kCubeTriangles.size());
  auto mesh = render_system_->CreateMesh(material_type->GetVertexType(),
                                         DataVolatility::kStaticReadWrite,
                                         vertex_count, triangle_count);
  mesh->Set<Vector3>(kCubeVertices, kCubeTriangles);
  auto mesh_id = mesh->GetResourceId();

  ASSERT_TRUE(render_system_->SaveMesh("mem:/mesh.gbme", mesh.Get()));
  EXPECT_EQ(mesh->GetResourceName(), "mem:/mesh.gbme");
  mesh.Reset();
  temp_resource_set_.RemoveAll();

  mesh = resource_system_->Load<Mesh>(&temp_resource_set_, "mem:/mesh.gbme");
  ASSERT_NE(mesh, nullptr);
  EXPECT_EQ(mesh->GetResourceId(), mesh_id);
  EXPECT_EQ(mesh->GetResourceName(), "mem:/mesh.gbme");
  EXPECT_NE(mesh->GetVertexType(), nullptr);
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

TEST_F(RenderSystemTest, LoadMeshWithInvalidData) {
  CreateSystem();
  auto* vertex_type = render_system_->RegisterVertexType<Vector3>(
      "vertex", {ShaderValue::kVec3});
  ASSERT_NE(vertex_type, nullptr);
  const std::vector<uint8_t> vertices(3 * sizeof(Vector3));
  const std::vector<uint8_t> too_many_vertices((kMaxMeshVertices + 1) *
                                               sizeof(Vector3));
  ASSERT_TRUE(
      WriteMeshChunk("mem:/valid.gbme", vertex_type, vertices, {0, 1, 2}));
  ASSERT_TRUE(WriteMeshChunk("mem:/partial_vertex.gbme", vertex_type,
                             absl::MakeConstSpan(vertices).subspan(1),
                             {0, 1, 1}));
  ASSERT_TRUE(WriteMeshChunk("mem:/too_many_vertices.gbme", vertex_type,
                             too_many_vertices, {0, 1, 2}));
  ASSERT_TRUE(WriteMeshChunk("mem:/partial_triangle.gbme", vertex_type,
                             vertices, {0, 1, 2, 0}));
  ASSERT_TRUE(WriteMeshChunk("mem:/invalid_index.gbme", vertex_type, vertices,
                             {0, 1, 3}));

  EXPECT_NE(resource_system_->Load<Mesh>("mem:/valid.gbme"), nullptr);
  EXPECT_EQ(resource_system_->Load<Mesh>("mem:/partial_vertex.gbme"), nullptr);
  EXPECT_EQ(resource_system_->Load<Mesh>("mem:/too_many_vertices.gbme"),
            nullptr);
  EXPECT_EQ(resource_system_->Load<Mesh>("mem:/partial_triangle.gbme"),
            nullptr);
  EXPECT_EQ(resource_system_->Load<Mesh>("mem:/invalid_index.gbme"), nullptr);
}

}  // namespace
}  // namespace gb
