// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/imgui/imgui_instance.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "gb/base/context_builder.h"
#include "gb/file/file_system.h"
#include "gb/render/binding_data.h"
#include "gb/render/draw_list.h"
#include "gb/render/material.h"
#include "gb/render/render_system.h"
#include "gb/render/render_test.h"
#include "gb/render/test_render_backend.h"
#include "gb/render/texture.h"
#include "gb/render/texture_array.h"
#include "gb/resource/resource_set.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "imgui.h"

namespace gb {
namespace {

using ::testing::HasSubstr;
using ::testing::IsSupersetOf;

constexpr int kWidth = 800;
constexpr int kHeight = 600;

class ImGuiInstanceTest : public RenderTest {
 protected:
  void SetUp() override {
    CreateSystem();
    state_.frame_dimensions = {kWidth, kHeight};
  }

  // Creates an instance that doesn't save its settings.
  std::unique_ptr<ImGuiInstance> CreateInstance() {
    auto instance = ImGuiInstance::Create(ContextBuilder()
                                              .SetPtr(file_system_.get())
                                              .SetPtr(render_system_.get())
                                              .Build());
    if (instance != nullptr) {
      ImGui::GetIO().IniFilename = nullptr;
    }
    return instance;
  }

  // Starts a frame on the active instance.
  void NewFrame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(kWidth, kHeight);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
  }

  // Adds rectangles (4 vertices and 6 indices each) to the draw list.
  static void AddRects(ImDrawList* draw_list, int count) {
    for (int i = 0; i < count; ++i) {
      draw_list->AddRectFilled(ImVec2(10, 10), ImVec2(20, 20), IM_COL32_WHITE);
    }
  }

  // Ends the frame and draws it, keeping only this frame's draw commands.
  void DrawFrame(ImGuiInstance* instance) {
    state_.draw_commands.clear();
    ASSERT_TRUE(render_system_->BeginFrame());
    instance->Draw();
    render_system_->EndFrame();
  }

  // Returns the number of indices drawn by the last DrawFrame().
  int GetDrawnIndexCount() const {
    int count = 0;
    for (const DrawCommand& command : state_.draw_commands) {
      if (command.type == DrawCommand::Type::kDraw) {
        count += static_cast<int>(command.draw.index_count);
      }
    }
    return count;
  }
};

TEST_F(ImGuiInstanceTest, CreateIsActive) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  EXPECT_TRUE(instance->IsActive());
  EXPECT_EQ(ImGuiInstance::GetActive(), instance.get());
}

TEST_F(ImGuiInstanceTest, NoActiveInstanceAfterDestroy) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  instance.reset();
  EXPECT_EQ(ImGuiInstance::GetActive(), nullptr);
}

TEST_F(ImGuiInstanceTest, CreateTwoInstances) {
  auto first = CreateInstance();
  ASSERT_NE(first, nullptr);
  auto second = CreateInstance();
  ASSERT_NE(second, nullptr);
  EXPECT_TRUE(second->IsActive());
  EXPECT_FALSE(first->IsActive());
  EXPECT_EQ(ImGuiInstance::GetActive(), second.get());

  first->SetActive();
  EXPECT_TRUE(first->IsActive());
  EXPECT_FALSE(second->IsActive());
  EXPECT_EQ(ImGuiInstance::GetActive(), first.get());

  // Each instance draws on its own.
  for (ImGuiInstance* instance : {first.get(), second.get()}) {
    instance->SetActive();
    ASSERT_TRUE(instance->LoadFonts());
    NewFrame();
    AddRects(ImGui::GetForegroundDrawList(), 2);
    DrawFrame(instance);
    EXPECT_EQ(GetDrawnIndexCount(), 12);
  }

  // Destroying an inactive instance leaves the active one.
  first.reset();
  EXPECT_EQ(ImGuiInstance::GetActive(), second.get());
}

TEST_F(ImGuiInstanceTest, FailedCreateKeepsActiveInstance) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  state_.fail_create_texture = true;
  EXPECT_EQ(CreateInstance(), nullptr);
  EXPECT_EQ(ImGuiInstance::GetActive(), instance.get());
}

TEST_F(ImGuiInstanceTest, CreateFailsWithoutRenderResources) {
  using State = TestRenderBackend::State;
  for (bool State::* fail :
       {&State::fail_create_texture, &State::fail_create_texture_array,
        &State::fail_create_shader_code, &State::fail_create_scene,
        &State::fail_create_pipeline, &State::fail_create_vertex_buffer,
        &State::fail_create_index_buffer}) {
    state_.ResetState();
    state_.*fail = true;
    EXPECT_EQ(CreateInstance(), nullptr);
    EXPECT_EQ(ImGuiInstance::GetActive(), nullptr);
  }
}

TEST_F(ImGuiInstanceTest, LoadFontsOnlyOnce) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());
  auto* material = static_cast<Material*>(ImGui::GetIO().Fonts->TexID);
  ASSERT_NE(material, nullptr);
  const Texture* font_texture =
      material->GetMaterialBindingData()->GetTexture(0);
  EXPECT_NE(font_texture, nullptr);

  ASSERT_TRUE(instance->LoadFonts());
  EXPECT_EQ(ImGui::GetIO().Fonts->TexID, material);
  EXPECT_EQ(material->GetMaterialBindingData()->GetTexture(0), font_texture);
}

TEST_F(ImGuiInstanceTest, AddTexture) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ResourceSet resources;
  Texture* texture = render_system_->CreateTexture(
      &resources, DataVolatility::kStaticReadWrite, 4, 4);
  ASSERT_NE(texture, nullptr);
  TextureArray* texture_array = render_system_->CreateTextureArray(
      &resources, DataVolatility::kStaticReadWrite, 2, 4, 4);
  ASSERT_NE(texture_array, nullptr);

  ImTextureID texture_id = instance->AddTexture(texture);
  ASSERT_NE(texture_id, nullptr);
  EXPECT_EQ(static_cast<Material*>(texture_id)
                ->GetMaterialBindingData()
                ->GetTexture(0),
            texture);
  EXPECT_EQ(instance->AddTexture(texture), texture_id);

  ImTextureID layer_0_id = instance->AddTexture(texture_array, 0);
  ImTextureID layer_1_id = instance->AddTexture(texture_array, 1);
  ASSERT_NE(layer_0_id, nullptr);
  ASSERT_NE(layer_1_id, nullptr);
  EXPECT_NE(layer_0_id, layer_1_id);
  EXPECT_NE(layer_0_id, texture_id);
  EXPECT_EQ(static_cast<Material*>(layer_1_id)
                ->GetMaterialBindingData()
                ->GetTextureArray(1),
            texture_array);
  EXPECT_EQ(instance->AddTexture(texture_array, 0), layer_0_id);
  EXPECT_EQ(instance->AddTexture(texture_array, 1), layer_1_id);
}

TEST_F(ImGuiInstanceTest, AddInvalidTexture) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ResourceSet resources;
  TextureArray* texture_array = render_system_->CreateTextureArray(
      &resources, DataVolatility::kStaticReadWrite, 2, 4, 4);
  ASSERT_NE(texture_array, nullptr);

  EXPECT_EQ(instance->AddTexture(static_cast<Texture*>(nullptr)), nullptr);
  EXPECT_EQ(instance->AddTexture(static_cast<TextureArray*>(nullptr), 0),
            nullptr);
  EXPECT_EQ(instance->AddTexture(texture_array, -1), nullptr);
  EXPECT_EQ(instance->AddTexture(texture_array, 2), nullptr);
}

TEST_F(ImGuiInstanceTest, DrawGrowsMesh) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());

  for (int count : {10, 300, 10000, 10}) {
    NewFrame();
    AddRects(ImGui::GetForegroundDrawList(), count);
    DrawFrame(instance.get());
    EXPECT_EQ(GetDrawnIndexCount(), count * 6) << "Where count=" << count;
  }
}

TEST_F(ImGuiInstanceTest, DrawSkipsFrameOverMeshLimit) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());

  // Each draw list must stay under 64K vertices, so the frame is split
  // across two of them.
  NewFrame();
  AddRects(ImGui::GetBackgroundDrawList(), 8500);
  AddRects(ImGui::GetForegroundDrawList(), 8500);
  DrawFrame(instance.get());
  EXPECT_EQ(GetDrawnIndexCount(), 0);

  NewFrame();
  AddRects(ImGui::GetForegroundDrawList(), 10);
  DrawFrame(instance.get());
  EXPECT_EQ(GetDrawnIndexCount(), 60);
}

TEST_F(ImGuiInstanceTest, DrawRecoversFromEditFailure) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());

  state_.vertex_buffer_config.fail_edit_begin = true;
  NewFrame();
  AddRects(ImGui::GetForegroundDrawList(), 10);
  DrawFrame(instance.get());
  EXPECT_EQ(GetDrawnIndexCount(), 0);

  state_.vertex_buffer_config.fail_edit_begin = false;
  NewFrame();
  AddRects(ImGui::GetForegroundDrawList(), 10);
  DrawFrame(instance.get());
  EXPECT_EQ(GetDrawnIndexCount(), 60);
}

TEST_F(ImGuiInstanceTest, DrawAfterResetRenderState) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());

  NewFrame();
  ImDrawList* draw_list = ImGui::GetForegroundDrawList();
  AddRects(draw_list, 1);
  draw_list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
  AddRects(draw_list, 1);
  DrawFrame(instance.get());
  EXPECT_EQ(GetDrawnIndexCount(), 12);

  // Everything a draw needs is set again after the reset.
  const std::vector<DrawCommand>& commands = state_.draw_commands;
  auto reset = std::find_if(commands.begin(), commands.end(),
                            [](const DrawCommand& command) {
                              return command.type == DrawCommand::Type::kReset;
                            });
  ASSERT_NE(reset, commands.end());
  std::vector<DrawCommand::Type> types;
  for (auto it = reset + 1;
       it != commands.end() && it->type != DrawCommand::Type::kDraw; ++it) {
    types.push_back(it->type);
  }
  EXPECT_THAT(
      types,
      IsSupersetOf(
          {DrawCommand::Type::kPipeline, DrawCommand::Type::kMaterialData,
           DrawCommand::Type::kVertices, DrawCommand::Type::kIndices,
           DrawCommand::Type::kInstanceData, DrawCommand::Type::kScissor}));
}

TEST_F(ImGuiInstanceTest, DrawClipsToFrame) {
  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());

  NewFrame();
  ImDrawList* draw_list = ImGui::GetForegroundDrawList();
  draw_list->PushClipRect(ImVec2(-10, -20), ImVec2(kWidth + 10, kHeight + 20));
  AddRects(draw_list, 1);
  draw_list->PopClipRect();

  // Nothing can be drawn in an empty clip rectangle.
  draw_list->PushClipRect(ImVec2(10, 10), ImVec2(10, 20));
  AddRects(draw_list, 1);
  draw_list->PopClipRect();
  DrawFrame(instance.get());
  EXPECT_EQ(GetDrawnIndexCount(), 6);

  std::vector<DrawCommand::Rect> scissors;
  for (const DrawCommand& command : state_.draw_commands) {
    if (command.type == DrawCommand::Type::kScissor) {
      scissors.push_back(command.rect);
    }
  }
  ASSERT_EQ(scissors.size(), 1);
  EXPECT_EQ(scissors[0].x, 0);
  EXPECT_EQ(scissors[0].y, 0);
  EXPECT_EQ(scissors[0].width, kWidth);
  EXPECT_EQ(scissors[0].height, kHeight);
}

TEST_F(ImGuiInstanceTest, SavesAndLoadsSettings) {
  static constexpr const char* kIniFilename = "mem:/imgui.ini";

  auto instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());
  ImGui::GetIO().IniFilename = kIniFilename;
  NewFrame();
  ImGui::SetNextWindowPos(ImVec2(123, 45));
  ImGui::Begin("Test");
  ImGui::End();
  DrawFrame(instance.get());
  instance.reset();

  std::string settings;
  ASSERT_TRUE(file_system_->ReadFile(kIniFilename, &settings));
  EXPECT_THAT(settings, HasSubstr("[Window][Test]"));

  instance = CreateInstance();
  ASSERT_NE(instance, nullptr);
  ASSERT_TRUE(instance->LoadFonts());
  ImGui::GetIO().IniFilename = kIniFilename;
  NewFrame();
  ImGui::Begin("Test");
  EXPECT_EQ(ImGui::GetWindowPos().x, 123);
  EXPECT_EQ(ImGui::GetWindowPos().y, 45);
  ImGui::End();
  DrawFrame(instance.get());
}

}  // namespace
}  // namespace gb
