// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <memory>
#include <utility>

#include "SDL_main.h"
#include "absl/status/statusor.h"
#include "gb/base/context.h"
#include "gb/base/context_builder.h"
#include "gb/base/validated_context.h"
#include "gb/cli/cli.h"
#include "gb/cli/cli_command_line.h"
#include "gb/cli/cli_path.h"
#include "gb/cli/run_command_line.h"
#include "gb/game/game.h"
#include "gb/pilot/pilot.h"
#include "gb/pilot/script/cli_pilot.h"

// Game includes
#include "render_check.h"

namespace {

bool Run(gb::ValidatedContext& context,
         const gb::CliCommandLine& command_line) {
  // Null without --pilot, so the checks are run by hand.
  absl::StatusOr<std::unique_ptr<gb::Pilot>> pilot =
      gb::CreateCliPilot(context, command_line);
  if (!pilot.ok()) {
    return false;
  }

  // Frames are paced by the swap chain alone.
  RenderCheck render_check;
  return render_check.Run(gb::ContextBuilder()
                              .SetParent(context)
                              .SetOwned(*std::move(pilot))
                              .SetValue<int>(gb::Game::kKeyMaxFps, 0)
                              .Build(),
                          {});
}

const gb::CliCommand kProgram = {
    .name = "render_check",
    .help =
        "Exercises the Vulkan backend: by hand, or with a pilot script that "
        "presses the panel's buttons and saves frames.",
    .flags = {&gb::kCliPilotScript, &gb::kCliPilotOutput},
    .run = Run,
};

}  // namespace

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  gb::Context context;
  context.SetOwned(gb::CreateCliFileSystem());
  return gb::RunCommandLine(&context, &kProgram, gb::GetMainArgs(argc, argv));
}
