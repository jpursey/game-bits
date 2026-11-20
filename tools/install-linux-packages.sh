#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Installs the Ubuntu 24.04 packages Game Bits builds and runs with on Linux
# (see Linux in CLAUDE.md), and what install-vulkan-sdk.sh downloads the Vulkan
# SDK with.
#
# Usage:
#   sudo tools/install-linux-packages.sh

set -euo pipefail

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y \
  clang cmake ninja-build \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
  libxss-dev libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
  libegl-dev libdbus-1-dev libudev-dev \
  mesa-vulkan-drivers xvfb gdb \
  curl xz-utils
