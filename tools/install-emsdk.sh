#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Installs the Emscripten SDK, which builds Game Bits for the web: the compiler,
# its tools, and the Node it runs programs and tests with. It clones
# Emscripten's emsdk repository, then has emsdk install and activate one
# version, about 1.6 GB in all.
#
# Usage:
#   tools/install-emsdk.sh <dir> <version>
#
# <version> is an Emscripten version, such as 7.0.1. The SDK is installed in
# <dir>, which is emsdk's own checkout. Running this again on an existing <dir>
# updates the checkout, so newer versions are known, and installs and activates
# <version> beside any others already there. Source <dir>/emsdk_env.sh in each
# shell that builds or runs Game Bits for the web. It puts emcc, emcmake, and
# Node on the PATH.

set -euo pipefail

if [[ $# -ne 2 || -z "$1" || -z "$2" ]]; then
  echo "Usage: $0 <dir> <version>" >&2
  exit 2
fi
dir="$1"
version="$2"

if [[ -d "${dir}/.git" ]]; then
  git -C "${dir}" pull --ff-only --quiet
else
  git clone --quiet https://github.com/emscripten-core/emsdk.git "${dir}"
fi
"${dir}/emsdk" install "${version}"
"${dir}/emsdk" activate "${version}"
echo "Installed the Emscripten SDK ${version} in ${dir}."
echo "Source ${dir}/emsdk_env.sh to use it."
