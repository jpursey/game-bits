#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Installs the Ubuntu 24.04 packages Game Bits builds and runs with on Linux:
# clang, CMake, and Ninja; SDL's windowing headers (without them SDL builds, but
# can't open a window); Mesa's Vulkan drivers (including lavapipe, which renders
# on the CPU where there is no GPU); a virtual X server (Xvfb, for where there is
# no display); and gdb, which saves a hung program's stacks. The Vulkan SDK
# comes from install-vulkan-sdk.sh instead, as Ubuntu's is too old.
#
# Usage:
#   sudo tools/install-linux-packages.sh
#
# CI and the devcontainer (.devcontainer/) install with this too.

set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  clang cmake ninja-build \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libegl-dev libdbus-1-dev libudev-dev \
  mesa-vulkan-drivers xvfb gdb
