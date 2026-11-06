// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <memory>
#include <string_view>
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
#include "gb/pilot/pilot.h"
#include "gb/pilot/script/cli_pilot.h"

// Game includes
#include "block_world.h"

namespace {

const gb::CliFlag<std::string_view> kState = {
    .name = "state",
    .value_name = "name",
    .help = "The game state to start in, such as PlayState.",
    .default_value = "TitleState",
};

bool Run(gb::ValidatedContext& context,
         const gb::CliCommandLine& command_line) {
  // Null without --pilot, so the game is played by hand.
  absl::StatusOr<std::unique_ptr<gb::Pilot>> pilot =
      gb::CreateCliPilot(context, command_line);
  if (!pilot.ok()) {
    return false;
  }

  BlockWorld game(*command_line.Get(kState));
  return game.Run(gb::ContextBuilder()
                      .SetParent(context)
                      .SetOwned(*std::move(pilot))
                      .Build(),
                  {});
}

const gb::CliCommand kProgram = {
    .name = "block_world",
    .help =
        "A voxel world to walk around in: by hand, or with a pilot script "
        "that gives it keys and saves frames.",
    .flags = {&kState, &gb::kCliPilotScript, &gb::kCliPilotOutput},
    .run = Run,
};

}  // namespace

SDLMAIN_DECLSPEC int main(int argc, char* argv[]) {
  gb::Context context;
  context.SetOwned(gb::CreateCliFileSystem());
  return gb::RunCommandLine(&context, &kProgram, gb::GetMainArgs(argc, argv));
}
