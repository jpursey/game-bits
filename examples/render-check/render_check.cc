// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "render_check.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink_registry.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "gb/base/context_builder.h"
#include "gb/render/material.h"
#include "gb/render/material_config.h"
#include "gb/render/material_type.h"
#include "gb/render/mesh.h"
#include "gb/render/render_scene.h"
#include "gb/render/sampler_options.h"
#include "gb/render/shader.h"
#include "gb/render/texture.h"
#include "gb/render/texture_array.h"
#include "gb/render/vulkan/sdl/sdl_vulkan_window.h"
#include "gb/render/vulkan/vulkan_allocator.h"
#include "imgui_impl_sdl.h"

// Game includes
#include "quad_shaders.h"

// Windows is included last, as its macros rename functions declared above.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// Must be after windows.h.
#include <psapi.h>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 800;

// The GUI panel is on the left, and the instance grid to its right.
constexpr float kPanelWidth = 560.0f;
constexpr int kGridColumns = 24;
constexpr int kGridRows = 12;
constexpr int kInstanceCount = kGridColumns * kGridRows;
constexpr float kGridLeft = kPanelWidth + 40.0f;
constexpr float kGridTop = 40.0f;
constexpr float kCellSize = 22.0f;
constexpr float kQuadSize = 18.0f;

constexpr int kTextureSize = 256;
constexpr int kSquareSize = 16;
constexpr int kArrayCount = 4;
constexpr int kArraySize = 64;

// Converts a hue in [0, 1) to a fully saturated color.
gb::Pixel HueColor(float hue) {
  hue = hue - std::floor(hue);
  const float h = hue * 6.0f;
  const float x = 1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f);
  float r = 0.0f, g = 0.0f, b = 0.0f;
  if (h < 1.0f) {
    r = 1.0f, g = x;
  } else if (h < 2.0f) {
    r = x, g = 1.0f;
  } else if (h < 3.0f) {
    g = 1.0f, b = x;
  } else if (h < 4.0f) {
    g = x, b = 1.0f;
  } else if (h < 5.0f) {
    r = x, b = 1.0f;
  } else {
    r = 1.0f, b = x;
  }
  return gb::Pixel(static_cast<uint8_t>(r * 255.0f),
                   static_cast<uint8_t>(g * 255.0f),
                   static_cast<uint8_t>(b * 255.0f));
}

std::string PixelString(gb::Pixel pixel) {
  return absl::StrFormat("(%d, %d, %d, %d)", pixel.r, pixel.g, pixel.b,
                         pixel.a);
}

std::string ResultString(bool result, bool expected) {
  return absl::StrCat(result ? "succeeded" : "failed",
                      result == expected ? " (expected)" : " (UNEXPECTED)");
}

}  // namespace

//==============================================================================
// LogCounter
//==============================================================================

void LogCounter::Send(const absl::LogEntry& entry) {
  if (entry.log_severity() < absl::LogSeverity::kWarning) {
    return;
  }
  absl::MutexLock lock(&mutex_);
  if (entry.log_severity() == absl::LogSeverity::kWarning) {
    ++counts_.warnings;
  } else {
    ++counts_.errors;
  }
  if (static_cast<int>(counts_.recent.size()) == kMaxRecent) {
    counts_.recent.erase(counts_.recent.begin());
  }
  counts_.recent.emplace_back(absl::StrCat(
      absl::LogSeverityName(entry.log_severity()), ": ", entry.text_message()));
}

LogCounter::Counts LogCounter::GetCounts() const {
  absl::MutexLock lock(&mutex_);
  return counts_;
}

void LogCounter::Reset() {
  absl::MutexLock lock(&mutex_);
  counts_ = {};
}

//==============================================================================
// RenderCheck
//==============================================================================

RenderCheck::~RenderCheck() { CleanUp(); }

bool RenderCheck::Init() {
  absl::AddLogSink(&log_counter_);
  return InitWindow() && InitRenderSystem() && InitGui() && InitQuads() &&
         InitTextures();
}

bool RenderCheck::InitWindow() {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    LOG(ERROR) << "Unable to initialize SDL: " << SDL_GetError();
    return false;
  }
  window_ = SDL_CreateWindow(
      "Render Check", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kWindowWidth, kWindowHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (window_ == nullptr) {
    LOG(ERROR) << "Could not create SDL window: " << SDL_GetError();
    return false;
  }

  // Window events are handled on the dispatcher's thread, so the backend's
  // size changed callback is called on another thread.
  message_system_ = gb::MessageSystem::Create(&dispatcher_);
  if (message_system_ == nullptr) {
    return false;
  }
  sdl_channel_ = message_system_->AddChannel("sdl");
  return true;
}

bool RenderCheck::InitRenderSystem() {
  file_system_ = std::make_unique<gb::FileSystem>();
  resource_system_ = gb::ResourceSystem::Create();
  if (resource_system_ == nullptr) {
    return false;
  }

  auto window = gb::SdlVulkanWindow::Create(
      gb::ContextBuilder()
          .SetPtr<SDL_Window>(window_)
          .SetPtr<gb::MessageSystem>(message_system_.get())
          .SetValue<gb::MessageEndpointId>(
              gb::SdlVulkanWindow::kKeySdlEndpointId, sdl_channel_)
          .Build());
  if (window == nullptr) {
    LOG(ERROR) << "Could not create Vulkan window";
    return false;
  }
  // Validation is enabled in every build, so a release build (which needs no
  // debug runtime) can be run directly and still reports validation errors.
  auto backend = gb::VulkanBackend::Create(
      gb::ContextBuilder()
          .SetValue<std::string>(gb::VulkanBackend::kKeyAppName, "Render Check")
          .SetValue<bool>(gb::VulkanBackend::kKeyEnableDebug, true)
          .SetOwned<gb::VulkanWindow>(std::move(window))
          .Build());
  if (backend == nullptr) {
    LOG(ERROR) << "Could not create Vulkan backend";
    return false;
  }
  backend_ = backend.get();
  render_system_ = gb::RenderSystem::Create(
      gb::ContextBuilder()
          .SetOwned<gb::RenderBackend>(std::move(backend))
          .SetPtr<gb::ResourceSystem>(resource_system_.get())
          .SetPtr<gb::FileSystem>(file_system_.get())
          .Build());
  if (render_system_ == nullptr) {
    LOG(ERROR) << "Could not create render system";
    return false;
  }
  render_system_->SetClearColor(gb::Pixel(32, 32, 48));
  return true;
}

bool RenderCheck::InitGui() {
  gui_instance_ = gb::ImGuiInstance::Create(
      gb::ContextBuilder()
          .SetPtr<gb::FileSystem>(file_system_.get())
          .SetPtr<gb::RenderSystem>(render_system_.get())
          .Build());
  if (gui_instance_ == nullptr) {
    LOG(ERROR) << "Could not create GUI instance";
    return false;
  }
  ImGui_ImplSDL2_InitForVulkan(window_);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.Fonts->AddFontDefault();
  if (!gui_instance_->LoadFonts()) {
    LOG(ERROR) << "Could not load GUI fonts";
    return false;
  }
  return true;
}

bool RenderCheck::InitQuads() {
  const gb::VertexType* vertex_type =
      render_system_->RegisterVertexType<QuadVertex>("QuadVertex",
                                                     {gb::ShaderValue::kVec2});
  const gb::RenderDataType* scene_constants =
      render_system_->RegisterConstantsType<SceneData>("SceneData");
  const gb::RenderDataType* instance_constants =
      render_system_->RegisterConstantsType<InstanceData>("InstanceData");
  if (vertex_type == nullptr || scene_constants == nullptr ||
      instance_constants == nullptr) {
    LOG(ERROR) << "Could not register quad types";
    return false;
  }

  const gb::Binding scene_binding =
      gb::Binding()
          .SetShaders(gb::ShaderType::kVertex)
          .SetLocation(gb::BindingSet::kScene, 0)
          .SetConstants(scene_constants, gb::DataVolatility::kPerFrame);
  const gb::Binding instance_binding =
      gb::Binding()
          .SetShaders(gb::ShaderType::kVertex)
          .SetLocation(gb::BindingSet::kInstance, 0)
          .SetConstants(instance_constants, gb::DataVolatility::kPerFrame);
  gb::RenderSceneType* scene_type = render_system_->RegisterSceneType(
      "QuadScene", {scene_binding, instance_binding});
  if (scene_type == nullptr) {
    LOG(ERROR) << "Could not register quad scene type";
    return false;
  }

  gb::Shader* vertex_shader = render_system_->CreateShader(
      &resources_, gb::ShaderType::kVertex,
      render_system_->CreateShaderCode(kQuadVertexShader,
                                       sizeof(kQuadVertexShader)),
      {scene_binding, instance_binding},
      {gb::ShaderParam(gb::ShaderValue::kVec2, 0)},
      {gb::ShaderParam(gb::ShaderValue::kVec4, 0)});
  gb::Shader* fragment_shader = render_system_->CreateShader(
      &resources_, gb::ShaderType::kFragment,
      render_system_->CreateShaderCode(kQuadFragmentShader,
                                       sizeof(kQuadFragmentShader)),
      {}, {gb::ShaderParam(gb::ShaderValue::kVec4, 0)},
      {gb::ShaderParam(gb::ShaderValue::kVec4, 0)});
  if (vertex_shader == nullptr || fragment_shader == nullptr) {
    LOG(ERROR) << "Could not create quad shaders";
    return false;
  }

  gb::MaterialType* material_type = render_system_->CreateMaterialType(
      &resources_, scene_type, vertex_type, vertex_shader, fragment_shader,
      gb::MaterialConfig()
          .SetCullMode(gb::CullMode::kNone)
          .SetDepthMode(gb::DepthMode::kNone));
  if (material_type == nullptr) {
    LOG(ERROR) << "Could not create quad material type";
    return false;
  }
  quad_material_ = render_system_->CreateMaterial(&resources_, material_type);
  if (quad_material_ == nullptr) {
    LOG(ERROR) << "Could not create quad material";
    return false;
  }

  quad_mesh_ = render_system_->CreateMesh(&resources_, vertex_type,
                                          gb::DataVolatility::kStaticWrite,
                                          /*max_vertices=*/4,
                                          /*max_triangles=*/2);
  const QuadVertex vertices[] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  const gb::Triangle triangles[] = {{0, 1, 2}, {0, 2, 3}};
  if (quad_mesh_ == nullptr ||
      !quad_mesh_->Set<QuadVertex>(vertices, triangles)) {
    LOG(ERROR) << "Could not create quad mesh";
    return false;
  }

  scene_ = render_system_->CreateScene(scene_type);
  if (scene_ == nullptr) {
    LOG(ERROR) << "Could not create quad scene";
    return false;
  }

  CreateInstances();
  return !instances_.empty();
}

bool RenderCheck::InitTextures() {
  const gb::SamplerOptions sharp =
      gb::SamplerOptions().SetFilter(false).SetMipmap(false);

  read_write_texture_.texture = render_system_->CreateTexture(
      &resources_, gb::DataVolatility::kStaticReadWrite, kTextureSize,
      kTextureSize, sharp);
  per_frame_texture_.texture =
      render_system_->CreateTexture(&resources_, gb::DataVolatility::kPerFrame,
                                    kTextureSize, kTextureSize, sharp);
  if (read_write_texture_.texture == nullptr ||
      per_frame_texture_.texture == nullptr) {
    LOG(ERROR) << "Could not create textures";
    return false;
  }
  ResetCheckerboard(read_write_texture_.texture);
  ResetCheckerboard(per_frame_texture_.texture);
  read_write_texture_.id =
      gui_instance_->AddTexture(read_write_texture_.texture);
  per_frame_texture_.id = gui_instance_->AddTexture(per_frame_texture_.texture);
  TwoRegionUpdate();

  // Mipmapped, so the small images show each layer's mipmaps.
  read_write_array_ = render_system_->CreateTextureArray(
      &resources_, gb::DataVolatility::kStaticReadWrite, kArrayCount,
      kArraySize, kArraySize);
  static_write_array_ = render_system_->CreateTextureArray(
      &resources_, gb::DataVolatility::kStaticWrite, 2, kArraySize, kArraySize,
      sharp);
  if (read_write_array_ == nullptr || static_write_array_ == nullptr) {
    LOG(ERROR) << "Could not create texture arrays";
    return false;
  }
  for (int i = 0; i < kArrayCount; ++i) {
    SetArrayLayer(read_write_array_, i,
                  HueColor(static_cast<float>(i) / kArrayCount));
    read_write_array_ids_.push_back(
        gui_instance_->AddTexture(read_write_array_, i));
  }
  for (int i = 0; i < 2; ++i) {
    SetArrayLayer(static_write_array_, i, HueColor(0.25f + 0.5f * i));
    static_write_array_ids_.push_back(
        gui_instance_->AddTexture(static_write_array_, i));
  }
  return true;
}

void RenderCheck::CleanUp() {
  instances_.clear();
  scene_.reset();
  if (gui_instance_ != nullptr) {
    ImGui_ImplSDL2_Shutdown();
    gui_instance_.reset();
  }
  resources_.RemoveAll();
  render_system_.reset();
  resource_system_.reset();
  file_system_.reset();
  message_system_.reset();
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
  absl::RemoveLogSink(&log_counter_);
}

void RenderCheck::Run() {
  while (ProcessEvents()) {
    ++frame_;
    UpdateFrameStats();
    UpdateInstances();
    UpdateTextures();

    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    DrawGui();

    if (!render_system_->BeginFrame()) {
      ++begin_frame_failures_;
      ImGui::EndFrame();

      // Nothing is presented (such as while minimized), so don't spin.
      SDL_Delay(10);
      continue;
    }
    ++frames_drawn_;
    DrawQuads();
    gui_instance_->Draw();
    render_system_->EndFrame();
  }
}

bool RenderCheck::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return false;
    }
    ImGui_ImplSDL2_ProcessEvent(&event);
    message_system_->Send<SDL_Event>(sdl_channel_, event);
  }
  return true;
}

void RenderCheck::UpdateFrameStats() {
  if (frame_ % 30 != 1) {
    return;
  }
  VmaStats stats = {};
  vmaCalculateStats(backend_->GetAllocator(), &stats);
  gpu_bytes_ = static_cast<int64_t>(stats.total.usedBytes);
  peak_gpu_bytes_ = std::max(peak_gpu_bytes_, gpu_bytes_);

  PROCESS_MEMORY_COUNTERS_EX counters = {};
  if (K32GetProcessMemoryInfo(
          GetCurrentProcess(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
          sizeof(counters))) {
    process_bytes_ = static_cast<int64_t>(counters.PrivateUsage);
    peak_process_bytes_ = std::max(peak_process_bytes_, process_bytes_);
  }
}

void RenderCheck::UpdateInstances() {
  if (churn_) {
    // The old instances were last drawn in a frame that has already ended.
    CreateInstances();
  }
}

void RenderCheck::UpdateTextures() {
  if (paint_) {
    PaintSquare(read_write_texture_.texture, paint_step_);
    PaintSquare(per_frame_texture_.texture, paint_step_);
    ++paint_step_;
  }
  if (cycle_layer_) {
    SetArrayLayer(read_write_array_, 0, HueColor(frame_ / 120.0f));
  }
}

void RenderCheck::DrawGui() {
  ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({kPanelWidth, 0}, ImGuiCond_Always);
  ImGui::Begin("Render Check", nullptr,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoCollapse);
  DrawFrameGui();
  DrawInstanceGui();
  DrawTextureGui();
  DrawTextureArrayGui();
  DrawLogGui();
  ImGui::End();
}

void RenderCheck::DrawFrameGui() {
  if (!ImGui::CollapsingHeader("Frames", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  const gb::FrameDimensions size = render_system_->GetFrameDimensions();
  ImGui::Text("Frame size: %d x %d", size.width, size.height);
  ImGui::Text("Frames drawn: %d   BeginFrame failures: %d", frames_drawn_,
              begin_frame_failures_);
  ImGui::Text("GPU memory: %.2f MB (peak %.2f MB)", gpu_bytes_ / 1.0e6,
              peak_gpu_bytes_ / 1.0e6);
  ImGui::Text("Process memory: %.1f MB (peak %.1f MB)", process_bytes_ / 1.0e6,
              peak_process_bytes_ / 1.0e6);
  if (ImGui::Button("Reset peaks")) {
    peak_gpu_bytes_ = gpu_bytes_;
    peak_process_bytes_ = process_bytes_;
  }
}

void RenderCheck::DrawInstanceGui() {
  if (!ImGui::CollapsingHeader("Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::Checkbox("Recreate every instance each frame", &churn_);
  ImGui::Text("Instances: %d   Created: %lld   Failed: %d",
              static_cast<int>(instances_.size()), instances_created_,
              failed_instances_);
  ImGui::TextWrapped(
      "The grid to the right should keep %d evenly spaced quads, with colors "
      "that shift smoothly while recreating.",
      kInstanceCount);
}

void RenderCheck::DrawTextureGui() {
  if (!ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::Checkbox("Paint a square each frame", &paint_);
  ImGui::SameLine();
  if (ImGui::Button("Reset checkerboards")) {
    ResetCheckerboard(read_write_texture_.texture);
    ResetCheckerboard(per_frame_texture_.texture);
    paint_step_ = 0;
  }
  const ImVec2 size(kTextureSize * 0.75f, kTextureSize * 0.75f);
  ImGui::BeginGroup();
  ImGui::Text("kStaticReadWrite");
  ImGui::Image(read_write_texture_.id, size);
  ImGui::EndGroup();
  ImGui::SameLine();
  ImGui::BeginGroup();
  ImGui::Text("kPerFrame");
  ImGui::Image(per_frame_texture_.id, size);
  ImGui::EndGroup();

  ImGui::Separator();
  ImGui::Text("kStaticWrite");
  ImGui::Image(static_write_texture_.id, {96, 96});
  ImGui::SameLine();
  ImGui::BeginGroup();
  if (ImGui::Button("New texture, two regions")) {
    TwoRegionUpdate();
  }
  if (ImGui::Button("Update part of the drawn texture")) {
    const std::vector<gb::Pixel> pixels(kSquareSize * kSquareSize,
                                        gb::Pixel(255, 255, 255));
    static_write_texture_.result =
        absl::StrCat("Update drawn: ",
                     ResultString(static_write_texture_.texture->SetRegion(
                                      0, 0, kSquareSize, kSquareSize, pixels),
                                  /*expected=*/false));
  }
  if (ImGui::Button("Update part of a mipmapped texture")) {
    gb::Texture* texture = render_system_->CreateTexture(
        &resources_, gb::DataVolatility::kStaticWrite, 128, 128);
    const std::vector<gb::Pixel> square(kSquareSize * kSquareSize,
                                        gb::Pixel(255, 255, 255));
    const std::vector<gb::Pixel> all(128 * 128, gb::Pixel(255, 255, 255));

    // The partial update must come first: once a whole update is pending, a
    // partial one succeeds.
    const bool partial_result =
        texture->SetRegion(0, 0, kSquareSize, kSquareSize, square);
    const bool whole_result = texture->Set(all);
    static_write_texture_.result = absl::StrCat(
        "Mipmapped partial: ", ResultString(partial_result, /*expected=*/false),
        ", whole: ", ResultString(whole_result, /*expected=*/true));
  }
  ImGui::TextWrapped("%s", static_write_texture_.result.c_str());
  ImGui::EndGroup();
}

void RenderCheck::DrawTextureArrayGui() {
  if (!ImGui::CollapsingHeader("Texture arrays",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::Text("kStaticReadWrite, mipmapped (64 px and 16 px)");
  for (int i = 0; i < kArrayCount; ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::Image(read_write_array_ids_[i], {kArraySize, kArraySize});
    ImGui::Image(read_write_array_ids_[i], {16, 16});
    ImGui::EndGroup();
  }
  ImGui::Checkbox("Change layer 0 each frame", &cycle_layer_);
  ImGui::SameLine();
  if (ImGui::Button("Get layer 1")) {
    std::vector<gb::Pixel> pixels;
    const bool result = read_write_array_->Get(1, pixels);
    read_write_array_result_ =
        absl::StrCat("Get: ", ResultString(result, /*expected=*/true), ", ",
                     pixels.size(), " pixels, first ",
                     pixels.empty() ? "none" : PixelString(pixels.front()));
  }
  ImGui::TextWrapped("%s", read_write_array_result_.c_str());

  ImGui::Separator();
  ImGui::Text("kStaticWrite");
  for (int i = 0; i < 2; ++i) {
    if (i > 0) {
      ImGui::SameLine();
    }
    ImGui::Image(static_write_array_ids_[i], {kArraySize, kArraySize});
  }
  ImGui::SameLine();
  if (ImGui::Button("Set layer 0 of the drawn array")) {
    const std::vector<gb::Pixel> pixels(kArraySize * kArraySize,
                                        gb::Pixel(255, 255, 255));
    static_write_array_result_ =
        absl::StrCat("Set: ", ResultString(static_write_array_->Set(0, pixels),
                                           /*expected=*/false));
  }
  ImGui::TextWrapped("%s", static_write_array_result_.c_str());
}

void RenderCheck::DrawLogGui() {
  if (!ImGui::CollapsingHeader("Log", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  const LogCounter::Counts counts = log_counter_.GetCounts();
  ImGui::Text("Errors: %d   Warnings: %d", counts.errors, counts.warnings);
  ImGui::SameLine();
  if (ImGui::Button("Reset log")) {
    log_counter_.Reset();
  }
  for (const std::string& message : counts.recent) {
    ImGui::TextWrapped("%s", message.c_str());
  }
}

void RenderCheck::DrawQuads() {
  const gb::FrameDimensions size = render_system_->GetFrameDimensions();
  SceneData scene_data = {};
  scene_data.pixel_to_clip[0] = 2.0f / size.width;
  scene_data.pixel_to_clip[1] = 2.0f / size.height;
  scene_->GetSceneBindingData()->SetConstants<SceneData>(0, scene_data);
  for (const auto& instance : instances_) {
    render_system_->Draw(scene_.get(), quad_mesh_, quad_material_,
                         instance.get());
  }
}

void RenderCheck::CreateInstances() {
  std::vector<std::unique_ptr<gb::BindingData>> instances;
  instances.reserve(kInstanceCount);
  for (int i = 0; i < kInstanceCount; ++i) {
    auto instance = quad_material_->CreateInstanceBindingData();
    if (instance == nullptr) {
      ++failed_instances_;
      continue;
    }
    SetInstanceData(i, instance.get());
    instances.emplace_back(std::move(instance));
  }
  instances_created_ += static_cast<int>(instances.size());
  instances_ = std::move(instances);
}

void RenderCheck::SetInstanceData(int index, gb::BindingData* instance) {
  InstanceData data = {};
  data.rect[0] = kGridLeft + (index % kGridColumns) * kCellSize;
  data.rect[1] = kGridTop + (index / kGridColumns) * kCellSize;
  data.rect[2] = kQuadSize;
  data.rect[3] = kQuadSize;
  const gb::Pixel color =
      HueColor(index / static_cast<float>(kInstanceCount) + frame_ / 240.0f);
  data.color[0] = color.r / 255.0f;
  data.color[1] = color.g / 255.0f;
  data.color[2] = color.b / 255.0f;
  data.color[3] = 1.0f;
  instance->SetConstants<InstanceData>(0, data);
}

void RenderCheck::PaintSquare(gb::Texture* texture, int step) {
  // Squares are painted around a circle, leaving the checkerboard elsewhere.
  // The pixels are tightly packed, and narrower than the texture.
  const float angle = step * 0.25f;
  const int center = (kTextureSize - kSquareSize) / 2;
  const int x = center + static_cast<int>(std::cos(angle) * 90.0f);
  const int y = center + static_cast<int>(std::sin(angle) * 90.0f);
  const std::vector<gb::Pixel> pixels(kSquareSize * kSquareSize,
                                      HueColor(step / 50.0f));
  texture->SetRegion(x, y, kSquareSize, kSquareSize, pixels);
}

void RenderCheck::ResetCheckerboard(gb::Texture* texture) {
  std::vector<gb::Pixel> pixels(kTextureSize * kTextureSize);
  for (int y = 0; y < kTextureSize; ++y) {
    for (int x = 0; x < kTextureSize; ++x) {
      const bool light = ((x / 32) + (y / 32)) % 2 == 0;
      pixels[y * kTextureSize + x] =
          light ? gb::Pixel(200, 200, 200) : gb::Pixel(90, 90, 90);
    }
  }
  texture->Set(pixels);
}

void RenderCheck::SetArrayLayer(gb::TextureArray* texture_array, int index,
                                gb::Pixel color) {
  // A checkerboard of the color and black, which mipmaps blend to a darker
  // version of the color.
  std::vector<gb::Pixel> pixels(kArraySize * kArraySize);
  for (int y = 0; y < kArraySize; ++y) {
    for (int x = 0; x < kArraySize; ++x) {
      const bool lit = ((x / 8) + (y / 8)) % 2 == 0;
      pixels[y * kArraySize + x] = lit ? color : gb::Pixel(0, 0, 0);
    }
  }
  texture_array->Set(index, pixels);
}

void RenderCheck::TwoRegionUpdate() {
  // A new texture is not in use, so partial updates of it succeed. The red
  // square is written first, and every later region is below or beside it
  // without containing it. Together they cover the texture, whose pixels are
  // otherwise undefined.
  gb::Texture* texture = render_system_->CreateTexture(
      &resources_, gb::DataVolatility::kStaticWrite, 128, 128,
      gb::SamplerOptions().SetFilter(false).SetMipmap(false));
  if (texture == nullptr) {
    static_write_texture_.result = "Could not create texture";
    return;
  }
  struct Region {
    int x, y, width, height;
    gb::Pixel color;
  };
  const gb::Pixel grey(128, 128, 128);
  const Region regions[] = {
      {16, 16, 32, 32, gb::Pixel(255, 0, 0)},  // Red square
      {0, 64, 128, 64, gb::Pixel(0, 0, 255)},  // Blue band
      {0, 0, 128, 16, grey},                   // Above the square
      {0, 48, 128, 16, grey},                  // Below the square
      {0, 16, 16, 32, grey},                   // Left of the square
      {48, 16, 80, 32, grey},                  // Right of the square
  };
  int failed = 0;
  for (const Region& region : regions) {
    const std::vector<gb::Pixel> pixels(region.width * region.height,
                                        region.color);
    if (!texture->SetRegion(region.x, region.y, region.width, region.height,
                            pixels)) {
      ++failed;
    }
  }
  static_write_texture_.texture = texture;
  static_write_texture_.id = gui_instance_->AddTexture(texture);
  static_write_texture_.result =
      absl::StrCat("Regions: ", ResultString(failed == 0, /*expected=*/true),
                   " (a red square on grey, above a blue band)");
}
