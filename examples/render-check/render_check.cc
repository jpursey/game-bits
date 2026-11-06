// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "render_check.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <utility>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/log_entry.h"
#include "absl/log/log_sink_registry.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
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
#include "pool_shaders.h"
#include "quad_shaders.h"
#include "storage_shaders.h"
#include "volume_shaders.h"

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

// Extra GUI rectangles (4 vertices each) are drawn in a block below the grid.
// kOverLimitRects alone nearly reaches the mesh's vertex limit, so with the
// panel the GUI is over it.
constexpr int kMaxExtraRects = 8000;
constexpr int kOverLimitRects = gb::kMaxMeshVertices / 4;
constexpr int kOverLimitFrames = 60;
constexpr int kExtraRectColumns = 128;
constexpr float kExtraRectTop = kGridTop + kGridRows * kCellSize + 40.0f;

// The blend check is a row of cells below the extra rectangles, each with a
// quad over a background that is black in its top half and grey below.
constexpr float kBlendTop = kExtraRectTop + 280.0f;
constexpr int kBlendCells = 5;
constexpr float kBlendCellSize = 100.0f;
constexpr float kBlendQuadWidth = 60.0f;
constexpr float kBlendQuadHeight = 80.0f;
constexpr float kBlendHoleSize = 30.0f;

// The texture formats are a row right of the extra rectangles, each texture
// drawn at full size above a small copy that shows its mipmaps. A few frames
// in, a white stripe is written at an odd x, so in the 1 and 2 byte formats it
// is uploaded from an offset that isn't a multiple of 4.
constexpr float kFormatLeft = kGridLeft + kExtraRectColumns * 2.0f + 40.0f;
constexpr float kFormatTop = kExtraRectTop + 20.0f;
constexpr int kFormatSize = 64;
constexpr int kFormatStripeX = 33;
constexpr int kFormatStripeWidth = 2;
constexpr int kFormatStripeFrame = 10;
constexpr float kFormatSmallSize = 16.0f;
constexpr float kFormatSpacing = 72.0f;

// The volume textures are below the texture formats, a row for each format.
// Each slice has its own pattern, and a row's quads sample from the first
// slice's center to the last one's in steps of half a slice, so with filtering
// every other quad is an even blend of two slices. The last quad moves through
// every depth, wrapping from the last slice to the first. A few frames in, a
// square is written to the middle two slices, at an odd x as for the stripes.
// It is written as texels rather than cleared, so it is copied a row and a
// slice at a time.
constexpr int kVolumeSize = 32;
constexpr int kVolumeDepth = 4;
constexpr int kVolumeQuads = kVolumeDepth * 2;
constexpr float kVolumeTop = kFormatTop + 130.0f;
constexpr float kVolumeQuadSize = 32.0f;
constexpr float kVolumeSpacing = 36.0f;
constexpr float kVolumeRowSpacing = 56.0f;
constexpr int kVolumeRegionX = 9;
constexpr int kVolumeRegionY = 8;
constexpr int kVolumeRegionSize = 14;
constexpr float kVolumeFramesPerCycle = 480.0f;

// The storage buffers are below the blend check, a row of quads for each
// volatility, side by side. Each quad reads its color from the buffer at its
// own index, which is an instance constant. The last quad's color changes
// every second, written as a single value, so frames a second apart differ
// only in that quad.
constexpr int kStorageQuads = 8;
constexpr float kStorageTop = kBlendTop + kBlendCellSize + 24.0f;
constexpr float kStorageQuadSize = 32.0f;
constexpr float kStorageSpacing = 36.0f;
constexpr float kStorageRowSpacing = kStorageQuads * kStorageSpacing + 40.0f;
constexpr float kStorageHueStep = 0.37f;

// The mesh pool's shapes are a row left of the texture formats, below the
// extra rectangles (which only reach them while the GUI is over its vertex
// limit). They have three sides and up, and evenly spaced hues, read from a
// storage buffer by draw index. The last is drawn as two ranges, the second in
// white. The pool starts too small for them, so it grows before they are first
// drawn, and again to hold replaced shapes until their space can be reused.
// Each replacement turns its shape by kPoolTurn for each shape replaced before
// it. The same shapes are drawn again kPoolIndirectOffset below, in one
// indirect draw.
constexpr int kPoolShapes = 8;
constexpr int kPoolDraws = kPoolShapes + 1;
constexpr int kPoolFirstSides = 3;
constexpr int kPoolStartVertices = 16;
constexpr int kPoolStartTriangles = 16;
constexpr float kPoolTop = kExtraRectTop + 160.0f;
constexpr float kPoolRadius = 14.0f;
constexpr float kPoolSpacing = 34.0f;
constexpr float kPoolTurn = 0.1f;
constexpr float kPoolIndirectOffset = 56.0f;

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

// Quads are drawn without culling or depth, unless a check needs them.
gb::MaterialConfig QuadConfig() {
  return gb::MaterialConfig()
      .SetCullMode(gb::CullMode::kNone)
      .SetDepthMode(gb::DepthMode::kNone);
}

std::string PixelString(gb::Pixel pixel) {
  return absl::StrFormat("(%d, %d, %d, %d)", pixel.r, pixel.g, pixel.b,
                         pixel.a);
}

// Returns the result of a check to show, and logs an error if it isn't what
// was expected, which fails a pilot's run.
std::string ResultString(std::string_view check, bool result, bool expected) {
  const char* const outcome = result ? "succeeded" : "failed";
  if (result != expected) {
    LOG(ERROR) << check << " " << outcome << ", which is unexpected";
  }
  return absl::StrCat(check, ": ", outcome, " (",
                      result == expected ? "expected" : "UNEXPECTED", ")");
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

bool RenderCheck::Init(const std::vector<std::string_view>& args) {
  absl::AddLogSink(&log_counter_);
  return InitWindow() && InitPilot() && InitRenderSystem() && InitGui() &&
         InitQuads() && InitTextures() && InitBlendQuads() &&
         InitFormatTextures() && InitVolumes() && InitStorageBuffers() &&
         InitMeshPool();
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

bool RenderCheck::InitPilot() {
  gb::Pilot* pilot = Context().GetPtr<gb::Pilot>();
  if (pilot == nullptr) {
    return true;
  }
  pilot_sdl_ = gb::PilotSdl::Create(pilot, window_);
  if (pilot_sdl_ == nullptr) {
    return false;
  }
  gui_under_limit_ = pilot->AddCondition("gui_under_limit",
                                         [this] { return IsGuiUnderLimit(); });
  return true;
}

bool RenderCheck::InitRenderSystem() {
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

  // The render system and GUI take the file system and the pilot from the
  // game's context.
  render_system_ = gb::RenderSystem::Create(
      gb::ContextBuilder()
          .SetParent(Context())
          .SetOwned<gb::RenderBackend>(std::move(backend))
          .SetPtr<gb::ResourceSystem>(resource_system_.get())
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
          .SetParent(Context())
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
      QuadConfig());
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

bool RenderCheck::InitBlendQuads() {
  gb::Material* none_material =
      CreateQuadMaterial(QuadConfig().SetBlendMode(gb::BlendMode::kNone));
  gb::Material* premultiplied_material = CreateQuadMaterial(
      QuadConfig().SetBlendMode(gb::BlendMode::kPremultiplied));
  gb::Material* additive_material =
      CreateQuadMaterial(QuadConfig().SetBlendMode(gb::BlendMode::kAdditive));
  gb::Material* depth_only_material = CreateQuadMaterial(
      QuadConfig().SetDepthMode(gb::DepthMode::kWrite).SetColorChannels({}));
  gb::Material* depth_test_material =
      CreateQuadMaterial(QuadConfig().SetDepthMode(gb::DepthMode::kTest));

  // The background, black above grey, drawn without blending.
  const float width = kBlendCells * kBlendCellSize;
  const float half_height = kBlendCellSize / 2.0f;
  if (!AddBlendQuad(none_material, kGridLeft, kBlendTop, width, half_height,
                    gb::PixelColor::kBlack) ||
      !AddBlendQuad(none_material, kGridLeft, kBlendTop + half_height, width,
                    half_height, gb::PixelColor::kGrey)) {
    return false;
  }

  // A half transparent orange quad in each blend mode, the premultiplied one
  // with its color scaled by its alpha so it matches kAlpha. In the last cell,
  // a depth-only square cuts a hole in the blue quad drawn after it.
  struct BlendQuad {
    gb::Material* material;
    int cell;
    float width;
    float height;
    gb::Pixel color;
  };
  const gb::Pixel orange(255, 128, 0, 128);
  const BlendQuad quads[] = {
      {none_material, 0, kBlendQuadWidth, kBlendQuadHeight, orange},
      {quad_material_, 1, kBlendQuadWidth, kBlendQuadHeight, orange},
      {premultiplied_material, 2, kBlendQuadWidth, kBlendQuadHeight,
       orange * (orange.a / 255.0f)},
      {additive_material, 3, kBlendQuadWidth, kBlendQuadHeight, orange},
      {depth_only_material, 4, kBlendHoleSize, kBlendHoleSize,
       gb::PixelColor::kWhite},
      {depth_test_material, 4, kBlendQuadWidth, kBlendQuadHeight,
       gb::Pixel(64, 128, 255)},
  };
  for (const BlendQuad& quad : quads) {
    const float x = kGridLeft + quad.cell * kBlendCellSize +
                    (kBlendCellSize - quad.width) / 2.0f;
    const float y = kBlendTop + (kBlendCellSize - quad.height) / 2.0f;
    if (!AddBlendQuad(quad.material, x, y, quad.width, quad.height,
                      quad.color)) {
      return false;
    }
  }
  return true;
}

bool RenderCheck::InitFormatTextures() {
  struct Format {
    gb::TextureFormat format;
    const char* name;
  };
  const Format formats[] = {
      {gb::TextureFormat::kRgba8Srgb, "Rgba8Srgb"},
      {gb::TextureFormat::kRgba8, "Rgba8"},
      {gb::TextureFormat::kRg8, "Rg8"},
      {gb::TextureFormat::kR8, "R8"},
      {gb::TextureFormat::kRgba4, "Rgba4"},
  };
  for (const Format& format : formats) {
    // Red increases to the right, green downward, and blue to the left.
    const int texel_size = gb::GetTexelSize(format.format);
    std::vector<uint8_t> texels(kFormatSize * kFormatSize * texel_size);
    for (int y = 0; y < kFormatSize; ++y) {
      for (int x = 0; x < kFormatSize; ++x) {
        const gb::Pixel pixel(static_cast<uint8_t>(x * 4),
                              static_cast<uint8_t>(y * 4),
                              static_cast<uint8_t>(255 - x * 4));
        const uint32_t texel = gb::PixelToTexel(format.format, pixel);
        std::memcpy(&texels[(y * kFormatSize + x) * texel_size], &texel,
                    texel_size);
      }
    }
    gb::Texture* texture = render_system_->CreateTexture(
        &resources_, gb::DataVolatility::kPerFrame, kFormatSize, kFormatSize,
        gb::SamplerOptions(), format.format);
    if (texture == nullptr ||
        !texture->Set(texels.data(), static_cast<int64_t>(texels.size()))) {
      LOG(ERROR) << "Could not create " << format.name << " texture";
      return false;
    }
    format_textures_.push_back(
        {format.name, texture, gui_instance_->AddTexture(texture)});
  }
  return true;
}

bool RenderCheck::InitVolumes() {
  // The volume quads are drawn in the quads' scene, with the same scene and
  // instance bindings.
  const gb::MaterialType* quad_type = quad_material_->GetType();
  const gb::Binding volume_binding =
      gb::Binding()
          .SetShaders(gb::ShaderType::kFragment)
          .SetLocation(gb::BindingSet::kMaterial, 0)
          .SetVolumeTexture();
  gb::Shader* vertex_shader = render_system_->CreateShader(
      &resources_, gb::ShaderType::kVertex,
      render_system_->CreateShaderCode(kVolumeVertexShader,
                                       sizeof(kVolumeVertexShader)),
      quad_type->GetVertexShader()->GetBindings(),
      {gb::ShaderParam(gb::ShaderValue::kVec2, 0)},
      {gb::ShaderParam(gb::ShaderValue::kVec3, 0)});
  gb::Shader* fragment_shader = render_system_->CreateShader(
      &resources_, gb::ShaderType::kFragment,
      render_system_->CreateShaderCode(kVolumeFragmentShader,
                                       sizeof(kVolumeFragmentShader)),
      {volume_binding}, {gb::ShaderParam(gb::ShaderValue::kVec3, 0)},
      {gb::ShaderParam(gb::ShaderValue::kVec4, 0)});
  if (vertex_shader == nullptr || fragment_shader == nullptr) {
    LOG(ERROR) << "Could not create volume shaders";
    return false;
  }

  gb::MaterialType* material_type = render_system_->CreateMaterialType(
      &resources_, quad_type->GetSceneType(), quad_type->GetVertexType(),
      vertex_shader, fragment_shader, QuadConfig());
  if (material_type == nullptr) {
    LOG(ERROR) << "Could not create volume material type";
    return false;
  }

  // Each slice is lit in its own color, all with full red, so R8 shows every
  // pattern.
  const gb::Pixel slice_colors[kVolumeDepth] = {
      gb::Pixel(255, 0, 0), gb::Pixel(255, 255, 0), gb::Pixel(255, 0, 255),
      gb::Pixel(255, 255, 255)};
  struct Format {
    gb::TextureFormat format;
    gb::DataVolatility volatility;
    const char* name;
  };
  const Format formats[] = {
      {gb::TextureFormat::kR8, gb::DataVolatility::kStaticReadWrite,
       "R8, kStaticReadWrite"},
      {gb::TextureFormat::kRgba4, gb::DataVolatility::kPerFrame,
       "Rgba4, kPerFrame"},
  };
  for (const Format& format : formats) {
    const int texel_size = gb::GetTexelSize(format.format);
    std::vector<uint8_t> texels(kVolumeSize * kVolumeSize * kVolumeDepth *
                                texel_size);
    for (int z = 0; z < kVolumeDepth; ++z) {
      for (int y = 0; y < kVolumeSize; ++y) {
        for (int x = 0; x < kVolumeSize; ++x) {
          // Horizontal stripes, vertical stripes, a checkerboard, and a
          // square, in blocks of 8 texels.
          const int bx = x / 8;
          const int by = y / 8;
          const bool lit[kVolumeDepth] = {
              by % 2 == 0, bx % 2 == 0, (bx + by) % 2 == 0,
              (bx == 1 || bx == 2) && (by == 1 || by == 2)};
          const uint32_t texel = gb::PixelToTexel(
              format.format, lit[z] ? slice_colors[z] : gb::PixelColor::kBlack);
          std::memcpy(
              &texels[((z * kVolumeSize + y) * kVolumeSize + x) * texel_size],
              &texel, texel_size);
        }
      }
    }
    gb::VolumeTexture* volume = render_system_->CreateVolumeTexture(
        &resources_, format.volatility, kVolumeSize, kVolumeSize, kVolumeDepth,
        gb::SamplerOptions().SetMipmap(false), format.format);
    gb::Material* material =
        render_system_->CreateMaterial(&resources_, material_type);
    if (volume == nullptr ||
        !volume->Set(texels.data(), static_cast<int64_t>(texels.size())) ||
        material == nullptr) {
      LOG(ERROR) << "Could not create " << format.name << " volume texture";
      return false;
    }
    material->GetMaterialBindingData()->SetVolumeTexture(0, volume);

    VolumeRow row = {format.name, volume, material, {}};
    for (int i = 0; i < kVolumeQuads; ++i) {
      auto quad = material->CreateInstanceBindingData();
      if (quad == nullptr) {
        LOG(ERROR) << "Could not create " << format.name << " volume quad";
        return false;
      }
      row.quads.emplace_back(std::move(quad));
    }
    volume_rows_.emplace_back(std::move(row));
  }
  return true;
}

bool RenderCheck::InitStorageBuffers() {
  const gb::RenderDataType* color_type =
      render_system_->RegisterConstantsType<QuadColor>("QuadColor");
  if (color_type == nullptr) {
    LOG(ERROR) << "Could not register the storage buffer's color type";
    return false;
  }

  // The storage buffer quads are drawn in the quads' scene, with the same
  // scene and instance bindings.
  const gb::MaterialType* quad_type = quad_material_->GetType();
  const gb::Binding storage_binding =
      gb::Binding()
          .SetShaders(gb::ShaderType::kFragment)
          .SetLocation(gb::BindingSet::kMaterial, 0)
          .SetStorageBuffer(color_type);
  gb::Shader* vertex_shader = render_system_->CreateShader(
      &resources_, gb::ShaderType::kVertex,
      render_system_->CreateShaderCode(kStorageVertexShader,
                                       sizeof(kStorageVertexShader)),
      quad_type->GetVertexShader()->GetBindings(),
      {gb::ShaderParam(gb::ShaderValue::kVec2, 0)},
      {gb::ShaderParam(gb::ShaderValue::kInt, 0)});
  storage_fragment_shader_ = render_system_->CreateShader(
      &resources_, gb::ShaderType::kFragment,
      render_system_->CreateShaderCode(kStorageFragmentShader,
                                       sizeof(kStorageFragmentShader)),
      {storage_binding}, {gb::ShaderParam(gb::ShaderValue::kInt, 0)},
      {gb::ShaderParam(gb::ShaderValue::kVec4, 0)});
  if (vertex_shader == nullptr || storage_fragment_shader_ == nullptr) {
    LOG(ERROR) << "Could not create storage buffer shaders";
    return false;
  }

  gb::MaterialType* material_type = render_system_->CreateMaterialType(
      &resources_, quad_type->GetSceneType(), quad_type->GetVertexType(),
      vertex_shader, storage_fragment_shader_, QuadConfig());
  if (material_type == nullptr) {
    LOG(ERROR) << "Could not create storage buffer material type";
    return false;
  }

  // The quads before the last are evenly spaced hues. UpdateStorageBuffers()
  // sets the last one.
  std::vector<QuadColor> colors(kStorageQuads);
  for (int i = 0; i < kStorageQuads - 1; ++i) {
    colors[i] = ToQuadColor(HueColor(static_cast<float>(i) / kStorageQuads));
  }
  struct Volatility {
    gb::DataVolatility volatility;
    const char* name;
  };
  const Volatility volatilities[] = {
      {gb::DataVolatility::kStaticReadWrite, "Storage, kStaticReadWrite"},
      {gb::DataVolatility::kPerFrame, "Storage, kPerFrame"},
  };
  for (const Volatility& volatility : volatilities) {
    gb::StorageBuffer* storage_buffer = render_system_->CreateStorageBuffer(
        &resources_, color_type, volatility.volatility, kStorageQuads);
    gb::Material* material =
        render_system_->CreateMaterial(&resources_, material_type);
    if (storage_buffer == nullptr ||
        !storage_buffer->Set<QuadColor>(0, colors) || material == nullptr) {
      LOG(ERROR) << "Could not create " << volatility.name << " buffer";
      return false;
    }
    material->GetMaterialBindingData()->SetStorageBuffer(0, storage_buffer);

    const int row_index = static_cast<int>(storage_rows_.size());
    const float left = kGridLeft + row_index * kStorageRowSpacing;
    StorageRow row = {volatility.name, left, storage_buffer, material, {}};
    for (int i = 0; i < kStorageQuads; ++i) {
      auto quad = material->CreateInstanceBindingData();
      if (quad == nullptr) {
        LOG(ERROR) << "Could not create " << volatility.name << " quad";
        return false;
      }
      const InstanceData data = {{left + i * kStorageSpacing, kStorageTop,
                                  kStorageQuadSize, kStorageQuadSize},
                                 {static_cast<float>(i), 0.0f, 0.0f, 0.0f}};
      quad->SetConstants<InstanceData>(0, data);
      row.quads.emplace_back(std::move(quad));
    }
    storage_rows_.emplace_back(std::move(row));
  }
  return true;
}

bool RenderCheck::InitMeshPool() {
  // The shapes are drawn in the quads' scene, with the storage buffer quads'
  // fragment shader, which reads a storage buffer of colors. The vertex shader
  // takes the quad's bindings, and only reads the scene's.
  const gb::MaterialType* quad_type = quad_material_->GetType();
  gb::Shader* vertex_shader = render_system_->CreateShader(
      &resources_, gb::ShaderType::kVertex,
      render_system_->CreateShaderCode(kPoolVertexShader,
                                       sizeof(kPoolVertexShader)),
      quad_type->GetVertexShader()->GetBindings(),
      {gb::ShaderParam(gb::ShaderValue::kVec2, 0)},
      {gb::ShaderParam(gb::ShaderValue::kInt, 0)});
  if (vertex_shader == nullptr) {
    LOG(ERROR) << "Could not create mesh pool shaders";
    return false;
  }
  gb::MaterialType* material_type = render_system_->CreateMaterialType(
      &resources_, quad_type->GetSceneType(), quad_type->GetVertexType(),
      vertex_shader, storage_fragment_shader_, QuadConfig());
  if (material_type == nullptr) {
    LOG(ERROR) << "Could not create mesh pool material type";
    return false;
  }

  // A color for each shape, and white for the last shape's second range.
  std::vector<QuadColor> colors(kPoolDraws);
  for (int i = 0; i < kPoolShapes; ++i) {
    colors[i] = ToQuadColor(HueColor(static_cast<float>(i) / kPoolShapes));
  }
  colors[kPoolShapes] = ToQuadColor(gb::PixelColor::kWhite);
  gb::StorageBuffer* storage_buffer = render_system_->CreateStorageBuffer(
      &resources_, render_system_->GetConstantsType("QuadColor"),
      gb::DataVolatility::kStaticReadWrite, kPoolDraws);
  pool_material_ = render_system_->CreateMaterial(&resources_, material_type);
  if (storage_buffer == nullptr || !storage_buffer->Set<QuadColor>(0, colors) ||
      pool_material_ == nullptr) {
    LOG(ERROR) << "Could not create mesh pool colors";
    return false;
  }
  pool_material_->GetMaterialBindingData()->SetStorageBuffer(0, storage_buffer);

  // The instance data's rectangle is the offset of the shapes' row.
  pool_instance_ = pool_material_->CreateInstanceBindingData();
  pool_indirect_instance_ = pool_material_->CreateInstanceBindingData();
  if (pool_instance_ == nullptr || pool_indirect_instance_ == nullptr) {
    LOG(ERROR) << "Could not create mesh pool instances";
    return false;
  }
  pool_instance_->SetConstants<InstanceData>(0, {});
  pool_indirect_instance_->SetConstants<InstanceData>(
      0, {{0.0f, kPoolIndirectOffset, 0.0f, 0.0f}, {}});

  // The draws the indirect row reads, written as the shapes change.
  const gb::RenderDataType* draw_type =
      render_system_->RegisterConstantsType<gb::MeshDraw>("MeshDraw");
  if (draw_type == nullptr) {
    LOG(ERROR) << "Could not register the mesh pool's draw type";
    return false;
  }
  pool_draws_ = render_system_->CreateStorageBuffer(
      &resources_, draw_type, gb::DataVolatility::kStaticReadWrite, kPoolDraws);
  mesh_pool_ = render_system_->CreateMeshPool(
      &resources_, quad_type->GetVertexType(), gb::IndexType::kUint32,
      kPoolStartVertices, kPoolStartTriangles);
  if (pool_draws_ == nullptr || mesh_pool_ == nullptr) {
    LOG(ERROR) << "Could not create mesh pool";
    return false;
  }
  for (int i = 0; i < kPoolShapes; ++i) {
    auto shape = CreatePoolShape(i, 0.0f);
    if (shape == nullptr) {
      return false;
    }
    pool_shapes_.emplace_back(std::move(shape));
  }
  SetPoolDraws();
  return true;
}

void RenderCheck::CleanUp() {
  instances_.clear();
  blend_instances_.clear();
  volume_rows_.clear();
  storage_rows_.clear();
  pool_shapes_.clear();
  pool_instance_.reset();
  pool_indirect_instance_.reset();
  scene_.reset();
  if (gui_instance_ != nullptr) {
    ImGui_ImplSDL2_Shutdown();
    gui_instance_.reset();
  }
  resources_.RemoveAll();
  render_system_.reset();
  resource_system_.reset();
  gui_under_limit_ = {};
  pilot_sdl_.reset();
  message_system_.reset();
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
  absl::RemoveLogSink(&log_counter_);
}

bool RenderCheck::Update(absl::Duration delta_time) {
  if (!ProcessEvents()) {
    return false;
  }
  ++frame_;
  time_ += delta_time;
  UpdateFrameStats();
  UpdateInstances();
  UpdateTextures();
  UpdateStorageBuffers();
  UpdateMeshPool();
  Draw();
  return true;
}

void RenderCheck::Draw() {
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();
  DrawGui();

  if (!render_system_->BeginFrame()) {
    ++begin_frame_failures_;
    ImGui::EndFrame();

    // Nothing is presented (such as while minimized), so don't spin.
    SDL_Delay(10);
    return;
  }
  ++frames_drawn_;
  DrawQuads();
  gui_instance_->Draw();
  render_system_->EndFrame();
}

bool RenderCheck::ProcessEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      return false;
    }
    if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
      OnKeyDown(event.key.keysym.sym);
    }
    ImGui_ImplSDL2_ProcessEvent(&event);
    message_system_->Send<SDL_Event>(sdl_channel_, event);
  }
  return true;
}

void RenderCheck::OnKeyDown(SDL_Keycode key) {
  switch (key) {
    case SDLK_p:
      paint_ = !paint_;
      break;
    case SDLK_1:
      TwoRegionUpdate();
      break;
    case SDLK_2:
      UpdatePartOfDrawnTexture();
      break;
    case SDLK_3:
      UpdatePartOfMipmappedTexture();
      break;
    case SDLK_m:
      replace_shapes_ = !replace_shapes_;
      break;
    case SDLK_i:
      multi_draw_ = !multi_draw_;
      break;
  }
}

bool RenderCheck::IsGuiUnderLimit() const { return over_limit_frames_ == 0; }

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
  if (frame_ == kFormatStripeFrame) {
    AddFormatStripes();
    AddVolumeRegions();
  }
  if (cycle_layer_) {
    SetArrayLayer(read_write_array_, 0, HueColor(frame_ / 120.0f));
  }
}

void RenderCheck::UpdateStorageBuffers() {
  const int64_t second = absl::ToInt64Seconds(time_);
  if (second == storage_second_) {
    return;
  }
  storage_second_ = second;
  const QuadColor color = ToQuadColor(HueColor(second * kStorageHueStep));
  for (const StorageRow& row : storage_rows_) {
    if (!row.storage_buffer->Set<QuadColor>(kStorageQuads - 1,
                                            absl::MakeConstSpan(&color, 1))) {
      LOG(ERROR) << "Could not change a color in the " << row.name << " buffer";
    }
  }
}

void RenderCheck::UpdateMeshPool() {
  backend_->SetMultiDrawEnabled(multi_draw_);
  if (!replace_shapes_) {
    return;
  }

  // The replacement is created while the old shape is still in the pool. The
  // old shape was last drawn in a frame that has already ended, so it can be
  // destroyed now.
  const int index = static_cast<int>(shapes_replaced_ % kPoolShapes);
  auto shape = CreatePoolShape(index, shapes_replaced_ * kPoolTurn);
  if (shape == nullptr) {
    return;
  }
  pool_shapes_[index] = std::move(shape);
  ++shapes_replaced_;
  SetPoolDraws();
}

std::vector<gb::MeshDraw> RenderCheck::GetPoolDraws() const {
  // Each draw's first instance is the index of its color.
  std::vector<gb::MeshDraw> draws;
  draws.reserve(kPoolDraws);
  for (int i = 0; i < kPoolShapes - 1; ++i) {
    draws.push_back(pool_shapes_[i]->GetDraw(i));
  }
  const gb::PooledMesh* last = pool_shapes_.back().get();
  const int half = last->GetTriangleCount() / 2;
  draws.push_back(last->GetDraw(0, half, kPoolShapes - 1));
  draws.push_back(
      last->GetDraw(half, last->GetTriangleCount() - half, kPoolShapes));
  return draws;
}

void RenderCheck::SetPoolDraws() {
  if (!pool_draws_->Set<gb::MeshDraw>(0, GetPoolDraws())) {
    LOG(ERROR) << "Could not write the mesh pool's draws";
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
  DrawGuiCheckGui();
  DrawBlendGui();
  DrawFormatGui();
  DrawVolumeGui();
  DrawStorageGui();
  DrawMeshPoolGui();
  DrawLogGui();
  ImGui::End();
  DrawExtraRects();
  DrawFormatTextures();
  DrawVolumeLabels();
  DrawStorageLabels();
  DrawMeshPoolLabel();
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
  ImGui::Checkbox("Paint a square each frame (P)", &paint_);
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
  if (ImGui::Button("New texture, two regions (1)")) {
    TwoRegionUpdate();
  }
  if (ImGui::Button("Update part of the drawn texture (2)")) {
    UpdatePartOfDrawnTexture();
  }
  if (ImGui::Button("Update part of a mipmapped texture (3)")) {
    UpdatePartOfMipmappedTexture();
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
    const bool result = read_write_array_->Get(1, &pixels);
    read_write_array_result_ =
        absl::StrCat(ResultString("Get", result, /*expected=*/true), ", ",
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
    static_write_array_result_ = ResultString(
        "Set", static_write_array_->Set(0, pixels), /*expected=*/false);
  }
  ImGui::TextWrapped("%s", static_write_array_result_.c_str());
}

void RenderCheck::DrawGuiCheckGui() {
  if (!ImGui::CollapsingHeader("GUI", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::SliderInt("Extra rectangles", &extra_rects_, 0, kMaxExtraRects);
  if (ImGui::Button("Go over the vertex limit for a second")) {
    over_limit_frames_ = kOverLimitFrames;
  }
  ImGui::Checkbox("Reset render state in this panel", &reset_render_state_);
  if (reset_render_state_) {
    ImGui::GetWindowDrawList()->AddCallback(ImDrawCallback_ResetRenderState,
                                            nullptr);
    ImGui::Text("This line is drawn after the reset.");
  }
}

void RenderCheck::DrawBlendGui() {
  if (!ImGui::CollapsingHeader("Blend modes", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::TextWrapped(
      "Below the grid, half transparent orange over black and grey, from the "
      "left: kNone is solid orange; kAlpha and kPremultiplied match; "
      "kAdditive matches them over black and is brighter over grey. Last is a "
      "blue quad with a square hole, cut by a depth-only quad drawn before "
      "it.");
}

void RenderCheck::DrawFormatGui() {
  if (!ImGui::CollapsingHeader("Texture formats",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::TextWrapped(
      "Right of the grid's lower half, a texture in each format, above a "
      "mipmapped copy. Red increases to the right, green downward, and blue "
      "to the left. Rgba8 is brighter than Rgba8Srgb, Rg8 has no blue, R8 is "
      "red alone, and Rgba4 matches Rgba8 in bands. Each has a white stripe "
      "(red for R8, yellow for Rg8) just right of its middle.");
}

void RenderCheck::DrawVolumeGui() {
  if (!ImGui::CollapsingHeader("Volume textures",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::TextWrapped(
      "Below the texture formats, a volume texture in each of two formats, "
      "whose slices hold red horizontal stripes, yellow vertical stripes, a "
      "magenta checkerboard, and a white square (all red in R8). The quads "
      "sample from the first slice to the last, every other one an even blend "
      "of the slices either side, and the last quad moves smoothly through "
      "them. The middle two slices have a blue-grey square (dark red in R8) "
      "over their pattern, which the first and last quads lack.");
}

void RenderCheck::DrawVolumeLabels() {
  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  const int count = static_cast<int>(volume_rows_.size());
  for (int i = 0; i < count; ++i) {
    draw_list->AddText(
        {kFormatLeft, kVolumeTop + i * kVolumeRowSpacing - 16.0f},
        IM_COL32_WHITE, volume_rows_[i].name);
  }
}

void RenderCheck::DrawStorageGui() {
  if (!ImGui::CollapsingHeader("Storage buffers",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::TextWrapped(
      "Below the blend check, a storage buffer of colors in each of two "
      "volatilities, drawn by a row of quads that each read the color at its "
      "own index. The first seven are evenly spaced hues from red, and the "
      "last changes color every second.");
}

void RenderCheck::DrawStorageLabels() {
  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  for (const StorageRow& row : storage_rows_) {
    draw_list->AddText({row.left, kStorageTop - 16.0f}, IM_COL32_WHITE,
                       row.name);
  }
}

void RenderCheck::DrawMeshPoolGui() {
  if (!ImGui::CollapsingHeader("Mesh pools", ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }
  ImGui::Checkbox("Replace a shape each frame (M)", &replace_shapes_);
  ImGui::Checkbox("Multi-draw (I)", &multi_draw_);
  ImGui::SameLine();
  ImGui::TextUnformatted(backend_->IsMultiDrawSupported() ? "Supported"
                                                          : "Not supported");
  const gb::MeshPoolStats stats = mesh_pool_->GetStats();
  ImGui::Text("Meshes: %d   Replaced: %lld", stats.mesh_count,
              static_cast<long long>(shapes_replaced_));
  ImGui::Text("Vertices: %d of %d used, %d waiting, %d free ranges",
              stats.vertices.used, stats.vertices.capacity,
              stats.vertices.waiting, stats.vertices.free_ranges);
  ImGui::Text("Triangles: %d of %d used, %d waiting, %d free ranges",
              stats.triangles.used, stats.triangles.capacity,
              stats.triangles.waiting, stats.triangles.free_ranges);
  ImGui::TextWrapped(
      "Left of the texture formats, a row of shapes from one 32-bit mesh pool, "
      "from a triangle to a decagon, in evenly spaced hues from red. The "
      "decagon is drawn as two ranges, its second half white. While "
      "replacing, the shapes turn as they are replaced, and the pool grows to "
      "hold the replaced shapes until their space is reused. Below them, the "
      "same shapes in one indirect draw, from a storage buffer of the same "
      "ranges, so the rows match: in one call with multi-draw, and a call "
      "for each range without it.");
}

void RenderCheck::DrawMeshPoolLabel() {
  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  draw_list->AddText({kGridLeft, kPoolTop - 16.0f}, IM_COL32_WHITE,
                     "Mesh pool, 32-bit");
  const bool multi_draw = multi_draw_ && backend_->IsMultiDrawSupported();
  draw_list->AddText(
      {kGridLeft, kPoolTop + kPoolIndirectOffset - 16.0f}, IM_COL32_WHITE,
      multi_draw ? "Indirect, multi-draw" : "Indirect, a call per range");
}

void RenderCheck::AddVolumeRegions() {
  for (const VolumeRow& row : volume_rows_) {
    const gb::TextureFormat format = row.volume->GetFormat();
    const uint32_t texel = gb::PixelToTexel(format, gb::Pixel(128, 128, 255));
    const int count = kVolumeRegionSize * kVolumeRegionSize * 2;
    std::vector<uint8_t> texels(count * gb::GetTexelSize(format));
    gb::FillTexels(format, &texel, count, texels.data());
    if (!row.volume->SetRegion(kVolumeRegionX, kVolumeRegionY, 1,
                               kVolumeRegionSize, kVolumeRegionSize, 2,
                               texels.data(),
                               static_cast<int64_t>(texels.size()))) {
      LOG(ERROR) << "Could not add a region to the " << row.name
                 << " volume texture";
    }
  }
}

void RenderCheck::AddFormatStripes() {
  for (const FormatTexture& format : format_textures_) {
    const gb::TextureFormat texture_format = format.texture->GetFormat();
    const uint32_t white =
        gb::PixelToTexel(texture_format, gb::PixelColor::kWhite);
    std::vector<uint8_t> texels(kFormatStripeWidth * kFormatSize *
                                gb::GetTexelSize(texture_format));
    gb::FillTexels(texture_format, &white, kFormatStripeWidth * kFormatSize,
                   texels.data());
    if (!format.texture->SetRegion(kFormatStripeX, 0, kFormatStripeWidth,
                                   kFormatSize, texels.data(),
                                   static_cast<int64_t>(texels.size()))) {
      LOG(ERROR) << "Could not add a stripe to the " << format.name
                 << " texture";
    }
  }
}

void RenderCheck::DrawFormatTextures() {
  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  const int count = static_cast<int>(format_textures_.size());
  for (int i = 0; i < count; ++i) {
    const FormatTexture& format = format_textures_[i];
    const float x = kFormatLeft + i * kFormatSpacing;
    draw_list->AddText({x, kFormatTop - 16.0f}, IM_COL32_WHITE, format.name);
    draw_list->AddImage(format.id, {x, kFormatTop},
                        {x + kFormatSize, kFormatTop + kFormatSize});
    const float small_top = kFormatTop + kFormatSize + 8.0f;
    draw_list->AddImage(format.id, {x, small_top},
                        {x + kFormatSmallSize, small_top + kFormatSmallSize});
  }
}

void RenderCheck::DrawExtraRects() {
  int count = extra_rects_;
  if (over_limit_frames_ > 0) {
    count = kOverLimitRects;
    --over_limit_frames_;
  }
  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  for (int i = 0; i < count; ++i) {
    const float x = kGridLeft + (i % kExtraRectColumns) * 2.0f;
    const float y = kExtraRectTop + (i / kExtraRectColumns) * 2.0f;
    const gb::Pixel color = HueColor(i / static_cast<float>(kMaxExtraRects));
    draw_list->AddRectFilled({x, y}, {x + 2.0f, y + 2.0f},
                             IM_COL32(color.r, color.g, color.b, 255));
  }
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
  render_system_->Draw(scene_.get(), blend_draw_list_);
  DrawVolumes();
  DrawStorageBuffers();
  DrawMeshPool();
}

void RenderCheck::DrawVolumes() {
  const int row_count = static_cast<int>(volume_rows_.size());
  for (int row = 0; row < row_count; ++row) {
    const VolumeRow& volume_row = volume_rows_[row];
    const float y = kVolumeTop + row * kVolumeRowSpacing;
    for (int i = 0; i < kVolumeQuads; ++i) {
      float x = kFormatLeft + i * kVolumeSpacing;
      float depth = (i + 1) / (2.0f * kVolumeDepth);
      if (i == kVolumeQuads - 1) {
        x += kVolumeSpacing / 2.0f;
        depth = std::fmod(frame_ / kVolumeFramesPerCycle, 1.0f);
      }
      const InstanceData data = {{x, y, kVolumeQuadSize, kVolumeQuadSize},
                                 {depth, 0.0f, 0.0f, 0.0f}};
      gb::BindingData* quad = volume_row.quads[i].get();
      quad->SetConstants<InstanceData>(0, data);
      render_system_->Draw(scene_.get(), quad_mesh_, volume_row.material, quad);
    }
  }
}

void RenderCheck::DrawStorageBuffers() {
  for (const StorageRow& row : storage_rows_) {
    for (const auto& quad : row.quads) {
      render_system_->Draw(scene_.get(), quad_mesh_, row.material, quad.get());
    }
  }
}

void RenderCheck::DrawMeshPool() {
  // The top row is drawn a range at a time, and the row below from the same
  // ranges in the storage buffer.
  gb::DrawList draw_list;
  draw_list.SetMeshPool(mesh_pool_, pool_material_, pool_instance_.get());
  for (const gb::MeshDraw& draw : GetPoolDraws()) {
    draw_list.Draw(draw);
  }
  draw_list.SetInstanceData(pool_indirect_instance_.get());
  draw_list.DrawIndirect(pool_draws_, 0, kPoolDraws);
  render_system_->Draw(scene_.get(), draw_list);
}

std::unique_ptr<gb::PooledMesh> RenderCheck::CreatePoolShape(int index,
                                                             float angle) {
  // A fan of triangles around the center vertex, with its first corner up
  // before turning.
  const int sides = kPoolFirstSides + index;
  const float center_x = kGridLeft + index * kPoolSpacing + kPoolRadius;
  const float center_y = kPoolTop + kPoolRadius;
  std::vector<QuadVertex> vertices = {{center_x, center_y}};
  std::vector<uint32_t> indices;
  for (int i = 0; i < sides; ++i) {
    const float corner_angle = angle +
                               2.0f * std::numbers::pi_v<float> * i / sides -
                               std::numbers::pi_v<float> / 2.0f;
    vertices.push_back({center_x + kPoolRadius * std::cos(corner_angle),
                        center_y + kPoolRadius * std::sin(corner_angle)});
    indices.insert(indices.end(), {0, static_cast<uint32_t>(1 + i),
                                   static_cast<uint32_t>(1 + (i + 1) % sides)});
  }
  auto shape = mesh_pool_->CreateMesh<QuadVertex>(vertices, indices);
  if (shape == nullptr) {
    LOG(ERROR) << "Could not create a mesh pool shape with " << sides
               << " sides";
  }
  return shape;
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
  SetQuadData(
      instance, kGridLeft + (index % kGridColumns) * kCellSize,
      kGridTop + (index / kGridColumns) * kCellSize, kQuadSize, kQuadSize,
      HueColor(index / static_cast<float>(kInstanceCount) + frame_ / 240.0f));
}

void RenderCheck::SetQuadData(gb::BindingData* instance, float x, float y,
                              float width, float height, gb::Pixel color) {
  InstanceData data = {};
  data.rect[0] = x;
  data.rect[1] = y;
  data.rect[2] = width;
  data.rect[3] = height;
  std::memcpy(data.color, ToQuadColor(color).color, sizeof(data.color));
  instance->SetConstants<InstanceData>(0, data);
}

RenderCheck::QuadColor RenderCheck::ToQuadColor(gb::Pixel color) {
  return {
      {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f}};
}

gb::Material* RenderCheck::CreateQuadMaterial(
    const gb::MaterialConfig& config) {
  const gb::MaterialType* quad_type = quad_material_->GetType();
  gb::MaterialType* material_type = render_system_->CreateMaterialType(
      &resources_, quad_type->GetSceneType(), quad_type->GetVertexType(),
      quad_type->GetVertexShader(), quad_type->GetFragmentShader(), config);
  if (material_type == nullptr) {
    return nullptr;
  }
  return render_system_->CreateMaterial(&resources_, material_type);
}

bool RenderCheck::AddBlendQuad(gb::Material* material, float x, float y,
                               float width, float height, gb::Pixel color) {
  std::unique_ptr<gb::BindingData> instance;
  if (material != nullptr) {
    instance = material->CreateInstanceBindingData();
  }
  if (instance == nullptr) {
    LOG(ERROR) << "Could not create blend quad";
    return false;
  }
  SetQuadData(instance.get(), x, y, width, height, color);
  blend_draw_list_.SetMesh(quad_mesh_, material, instance.get());
  blend_draw_list_.Draw();
  blend_instances_.emplace_back(std::move(instance));
  return true;
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
      absl::StrCat(ResultString("Regions", failed == 0, /*expected=*/true),
                   " (a red square on grey, above a blue band)");
}

void RenderCheck::UpdatePartOfDrawnTexture() {
  if (static_write_texture_.texture == nullptr) {
    return;
  }
  const std::vector<gb::Pixel> pixels(kSquareSize * kSquareSize,
                                      gb::Pixel(255, 255, 255));
  static_write_texture_.result =
      ResultString("Update drawn",
                   static_write_texture_.texture->SetRegion(
                       0, 0, kSquareSize, kSquareSize, pixels),
                   /*expected=*/false);
}

void RenderCheck::UpdatePartOfMipmappedTexture() {
  gb::Texture* texture = render_system_->CreateTexture(
      &resources_, gb::DataVolatility::kStaticWrite, 128, 128);
  if (texture == nullptr) {
    static_write_texture_.result = "Could not create texture";
    return;
  }
  const std::vector<gb::Pixel> square(kSquareSize * kSquareSize,
                                      gb::Pixel(255, 255, 255));
  const std::vector<gb::Pixel> all(128 * 128, gb::Pixel(255, 255, 255));

  // The partial update must come first: once a whole update is pending, a
  // partial one succeeds.
  const bool partial_result =
      texture->SetRegion(0, 0, kSquareSize, kSquareSize, square);
  const bool whole_result = texture->Set(all);
  static_write_texture_.result = absl::StrCat(
      ResultString("Mipmapped partial", partial_result, /*expected=*/false),
      ", ", ResultString("whole", whole_result, /*expected=*/true));
}
