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
#   tools/install-vulkan-sdk.sh <dir> <version>
#
# <version> is the one CI uses, its VULKAN_SDK_VERSION
# (.github/workflows/cmake.yml). The SDK is installed in <dir>/<version>. Source <dir>/<version>/setup-env.sh
# in each shell that builds or runs Game Bits programs, as with the full SDK.
# It sets VULKAN_SDK, which CMake finds Vulkan from, and puts the SDK's loader,
# layer, and glslc ahead of any the system has.

set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <dir> <version>" >&2
  exit 2
fi
dir="$1"
version="$2"

url="https://sdk.lunarg.com/sdk/download/${version}/linux/vulkansdk-linux-x86_64-${version}.tar.xz"
sdk="${version}/x86_64"
mkdir -p "${dir}"
curl -fsSL "${url}" | tar -xJ -C "${dir}" \
  "${version}/setup-env.sh" \
  "${sdk}/include/vulkan" \
  "${sdk}/include/vk_video" \
  "${sdk}/lib/VulkanLoader" \
  "${sdk}/lib/libVkLayer_khronos_validation.so" \
  "${sdk}/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json" \
  "${sdk}/bin/glslc"
echo "Installed the Vulkan SDK ${version} in ${dir}/${version}."
echo "Source ${dir}/${version}/setup-env.sh to use it."
