#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "SDL.h"
#include "SDL_main.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "gb/base/context_builder.h"
#include "gb/base/init_logging.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gb/cli/cli_path.h"
#include "gb/cli/run_command_line.h"
#include "gb/game/game.h"
#include "gb/pilot/pilot.h"
#include "gb/pilot/script/cli_pilot.h"
#include "gb/pilot/sdl/pilot_sdl.h"

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;

// A game with an SDL window, which closing it or pressing Escape quits. It logs
// the input it gets, to show where a game would handle it.
class SdlTemplate final : public gb::Game {
 protected:
  bool Init(const std::vector<std::string_view>& args) override;
  bool Update(absl::Duration delta_time) override;
  void CleanUp() override;

 private:
  // Handles one event, returning false if the game should quit.
  bool HandleEvent(const SDL_Event& event);

  SDL_Window* window_ = nullptr;
  std::unique_ptr<gb::PilotSdl> pilot_sdl_;
};

bool SdlTemplate::Init(const std::vector<std::string_view>& args) {
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    LOG(ERROR) << "Unable to initialize SDL: " << SDL_GetError();
    return false;
  }
  window_ = SDL_CreateWindow("SDL Template", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, kWindowWidth,
                             kWindowHeight, SDL_WINDOW_RESIZABLE);
  if (window_ == nullptr) {
    LOG(ERROR) << "Could not create SDL window: " << SDL_GetError();
    return false;
  }
  int width = 0;
  int height = 0;
  SDL_GetWindowSize(window_, &width, &height);
  LOG(INFO) << "Window size: " << width << "x" << height;

  // With a pilot script (--pilot), the pilot gives the game its input.
  gb::Pilot* pilot = Context().GetPtr<gb::Pilot>();
  if (pilot != nullptr) {
    pilot_sdl_ = gb::PilotSdl::Create(pilot, window_);
    if (pilot_sdl_ == nullptr) {
      return false;
    }
  }
  return true;
}

void SdlTemplate::CleanUp() {
  pilot_sdl_.reset();
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
  }
  SDL_Quit();
}

bool SdlTemplate::Update(absl::Duration delta_time) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (!HandleEvent(event)) {
      return false;
    }
  }

  // TODO: Update and draw the game.
  return true;
}

bool SdlTemplate::HandleEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_QUIT:
      return false;
    case SDL_KEYDOWN:
      LOG(INFO) << "Key down: " << SDL_GetKeyName(event.key.keysym.sym);
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        return false;
      }
      break;
    case SDL_MOUSEBUTTONDOWN:
      LOG(INFO) << "Mouse button " << static_cast<int>(event.button.button)
                << " down at " << event.button.x << "," << event.button.y;
      break;
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        LOG(INFO) << "Window size: " << event.window.data1 << "x"
                  << event.window.data2;
      }
      break;
  }
  return true;
}

bool Run(gb::ValidatedContext& context,
         const gb::CliCommandLine& command_line) {
  absl::StatusOr<std::unique_ptr<gb::Pilot>> pilot =
      gb::CreateCliPilot(context, command_line);
  if (!pilot.ok()) {
    return false;
  }

  SdlTemplate game;
  return game.Run(gb::ContextBuilder()
                      .SetParent(context)
                      .SetOwned(*std::move(pilot))
                      .Build(),
                  {});
}

const gb::CliCommand kProgram = {
    .name = "sdl_template",
    .help = "A bare bones game with an SDL window. Escape quits it.",
    .flags = {&gb::kCliPilotScript, &gb::kCliPilotOutput},
    .run = Run,
};

}  // namespace

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  gb::InitLogging();
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);

  return gb::RunCommandLine(
      gb::ContextBuilder().SetOwned(gb::CreateCliFileSystem()).Build(),
      &kProgram, gb::GetMainArgs(argc, argv));
}
