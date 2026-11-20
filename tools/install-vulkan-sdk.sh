#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Installs the parts of LunarG's Vulkan SDK for Linux that Game Bits builds and
# runs with: the headers, the loader, the validation layer, and glslc. The full
# SDK is 1.9 GB unpacked, mostly shader compilers and tools Game Bits doesn't
# use; these parts are about 450 MB, nearly all of it the validation layer.
#
# Usage:
#   tools/install-vulkan-sdk.sh <dir>
#
# The SDK is installed in <dir>/<version>. Source <dir>/<version>/setup-env.sh
# in each shell that builds or runs Game Bits programs, as with the full SDK.
# It sets VULKAN_SDK, which CMake finds Vulkan from, and puts the SDK's loader,
# layer, and glslc ahead of any the system has.
#
# The version is the one CI uses on Windows (.github/workflows/cmake.yml), and
# changes with it.

set -euo pipefail

readonly kVersion="1.4.363.1"

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <dir>" >&2
  exit 2
fi
dir="$1"

url="https://sdk.lunarg.com/sdk/download/${kVersion}/linux/vulkansdk-linux-x86_64-${kVersion}.tar.xz"
sdk="${kVersion}/x86_64"
mkdir -p "${dir}"
curl -fsSL "${url}" | tar -xJ -C "${dir}" \
  "${kVersion}/setup-env.sh" \
  "${sdk}/include/vulkan" \
  "${sdk}/include/vk_video" \
  "${sdk}/lib/VulkanLoader" \
  "${sdk}/lib/libVkLayer_khronos_validation.so" \
  "${sdk}/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json" \
  "${sdk}/bin/glslc"
echo "Installed the Vulkan SDK ${kVersion} in ${dir}/${kVersion}."
echo "Source ${dir}/${kVersion}/setup-env.sh to use it."
