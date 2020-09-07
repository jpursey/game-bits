// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "block_world.h"

#include <filesystem>

#include "absl/strings/str_cat.h"
#include "gb/base/context_builder.h"
#include "gb/file/local_file_protocol.h"
#include "gb/file/path.h"
#include "gb/render/render_system.h"
#include "gb/render/vulkan/sdl/sdl_vulkan_window.h"
#include "gb/render/vulkan/vulkan_backend.h"
#include "gb/resource/resource_system.h"
#include "glog/logging.h"
#include "imgui_impl_sdl.h"
#include "states.h"

namespace {
namespace fs = std::filesystem;
}  // namespace

bool BlockWorld::Init(const std::vector<std::string_view>& args) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    LOG(ERROR) << "Unable to initialize SDL: " << SDL_GetError();
    return false;
  }

  // TODO: Use a proper command line parser. Right now, we only need one
  //       argument for testing.
  if (!args.empty()) {
    init_state_name_ = absl::StrCat(args.front());
  }

  window_ = SDL_CreateWindow("Block World", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 1280, 720,
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
  if (window_ == nullptr) {
    LOG(ERROR) << "Could not create SDL window.";
    return false;
  }

  context_ = Contract(gb::Game::Context());
  if (!context_.IsValid() || !context_.SetPtr<BlockWorld>(this) ||
      !context_.SetPtr<SDL_Window>(window_)) {
    return false;
  }
  return InitFileSystem() && InitResourceSystem() && InitMessages() &&
         InitRenderSystem() && InitGui() && InitStates();
}

bool BlockWorld::InitFileSystem() {
  std::error_code error;
  fs::path game_root_path = fs::current_path(error);
  if (error) {
    LOG(ERROR) << "Could not determine current working directory: "
               << error.message();
    return false;
  }
  fs::path os_root_path = game_root_path.root_path();

  while (true) {
    if (fs::is_directory(game_root_path / "assets", error)) {
      break;
    }
    if (game_root_path == os_root_path) {
      LOG(ERROR)
          << "Could not determine game root from current working directory.";
      return false;
    }
    game_root_path = game_root_path.parent_path();
  }

  auto file_system = std::make_unique<gb::FileSystem>();

  const std::string game_root =
      gb::NormalizePath(game_root_path.generic_u8string());
  if (!file_system->Register(
          gb::LocalFileProtocol::Create(
              gb::ContextBuilder()
                  .SetValue<std::string>(gb::LocalFileProtocol::kKeyRoot,
                                         game_root)
                  .Build()),
          "game")) {
    LOG(ERROR) << "Failed to register 'game' protocol under path: "
               << game_root;
    return false;
  }
  const std::string asset_root = gb::JoinPath(game_root, "assets");
  if (!file_system->Register(
          gb::LocalFileProtocol::Create(
              gb::ContextBuilder()
                  .SetValue<std::string>(gb::LocalFileProtocol::kKeyRoot,
                                         asset_root)
                  .Build()),
          "asset")) {
    LOG(ERROR) << "Failed to register 'asset' protocol under path: "
               << asset_root;
    return false;
  }

  context_.SetOwned(std::move(file_system));
  return true;
}

bool BlockWorld::InitResourceSystem() {
  context_.SetOwned(gb::ResourceSystem::Create());
  return true;
}

bool BlockWorld::InitMessages() {
  CHECK(context_.SetOwned<gb::MessageSystem>(
      gb::MessageSystem::Create(&dispatcher_)));
  message_system_ = context_.GetPtr<gb::MessageSystem>();
  sdl_channel_ = message_system_->AddChannel("sdl");
  if (!context_.SetValue<gb::MessageEndpointId>(kKeySdlEndpointId,
                                                sdl_channel_)) {
    return false;
  }
  auto stack_endpoint = gb::MessageStackEndpoint::Create(
      message_system_, gb::MessageStackOrder::kTopDown);
  stack_endpoint->Subscribe(sdl_channel_);
  if (!context_.SetOwned<gb::MessageStackEndpoint>(std::move(stack_endpoint))) {
    return false;
  }
  return true;
}

bool BlockWorld::InitRenderSystem() {
  auto window = gb::SdlVulkanWindow::Create(
      gb::ContextBuilder().SetParent(context_).Build());
  auto backend = gb::VulkanBackend::Create(
      gb::ContextBuilder()
          .SetParent(context_)
          .SetValue<std::string>(gb::VulkanBackend::kKeyAppName, "Block World")
          .SetOwned<gb::VulkanWindow>(std::move(window))
          .Build());
  if (backend == nullptr) {
    LOG(ERROR) << "Cound not create vulkan backend.";
    return false;
  }
  render_backend_ = backend.get();
  auto render_system = gb::RenderSystem::Create(
      gb::ContextBuilder()
          .SetParent(context_)
          .SetOwned<gb::RenderBackend>(std::move(backend))
          .Build());
  if (render_system == nullptr) {
    LOG(ERROR) << "Count not create render system.";
    return false;
  }
  render_system_ = render_system.get();
  if (!context_.SetOwned(std::move(render_system))) {
    return false;
  }
  return true;
}

bool BlockWorld::InitGui() {
  auto gui_instance = gb::ImGuiInstance::Create(
      gb::ContextBuilder().SetParent(context_).Build());
  if (gui_instance == nullptr) {
    LOG(ERROR) << "Could not create GUI instance";
    return false;
  }

  GuiFonts fonts;
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->AddFontDefault();
  fonts.title = io.Fonts->AddFontFromFileTTF(
      "asset:/fonts/chunk_five/ChunkFive-Regular.otf", 100.0f);
  fonts.prompt = io.Fonts->AddFontFromFileTTF(
      "asset:/fonts/komika_text/KOMTXT__.ttf", 40.0f);
  fonts.console = io.Fonts->AddFontFromFileTTF(
      "asset:/fonts/source_code_pro/SourceCodePro-Medium.otf", 15.0f);
  if (fonts.title == nullptr || fonts.prompt == nullptr ||
      fonts.console == nullptr) {
    LOG(ERROR) << "Failed to load one or more fonts for GUI";
    return false;
  }
  if (!gui_instance->LoadFonts()) {
    LOG(ERROR) << "Failed to initialize fonts for GUI";
    return false;
  }
  gui_instance_ = gui_instance.get();
  context_.SetOwned(std::move(gui_instance));
  context_.SetValue<GuiFonts>(fonts);
  ImGui_ImplSDL2_InitForVulkan(window_);
  return true;
}

bool BlockWorld::InitStates() {
  if (!context_.SetOwned<gb::GameStateMachine>(gb::GameStateMachine::Create(
          gb::ContextBuilder().SetParent(context_).Build()))) {
    return false;
  }
  state_machine_ = context_.GetPtr<gb::GameStateMachine>();
  RegisterStates(state_machine_);
  auto init_state = state_machine_->GetRegisteredId(init_state_name_);
  if (init_state == gb::kNoGameStateId) {
    init_state = gb::GetGameStateId<TitleState>();
  }
  state_machine_->ChangeState(gb::kNoGameStateId, init_state);
  UpdateStateMachine(absl::ZeroDuration());
  return true;
}

void BlockWorld::UpdateStateMachine(absl::Duration delta_time) {
  ImGui_ImplSDL2_NewFrame(window_);
  ImGui::NewFrame();

  state_machine_->Update(delta_time);

  ImGui::EndFrame();
}

bool BlockWorld::Update(absl::Duration delta_time) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT || state_machine_->GetTopState() == nullptr) {
      return false;
    }
    message_system_->Send<SDL_Event>(sdl_channel_, event);
  }
  dispatcher_.Update();
  UpdateStateMachine(delta_time);
  return true;
}

void BlockWorld::CleanUp() {
  // Exit any existing states, to allow it to clean up while everything still
  // exists.
  if (state_machine_ != nullptr && state_machine_->GetTopState() != nullptr) {
    state_machine_->ChangeState(gb::kNoGameStateId, gb::kNoGameStateId);
    UpdateStateMachine(absl::ZeroDuration());
  }

  // Extract resources from the context, so we can make sure the destruction
  // order is explicit.
  auto message_system = context_.Release<gb::MessageSystem>();
  auto state_machine = context_.Release<gb::GameStateMachine>();
  auto gui_instance = context_.Release<gb::ImGuiInstance>();
  auto render_system = context_.Release<gb::RenderSystem>();
  auto resource_system = context_.Release<gb::ResourceSystem>();
  LOG_IF(ERROR, !context_.Complete()) << "Contract constraints violated!";
  state_machine.reset();
  ImGui_ImplSDL2_Shutdown();
  gui_instance.reset();
  render_system.reset();
  resource_system.reset();
  message_system.reset();

  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}