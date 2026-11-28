#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Installs naga, the shader converter a web build of Game Bits converts each
# shader to WGSL with (see gb_embed_shader in
# CMake/GameBitsTargetCommands.cmake). It builds naga-cli from source with
# Rust's cargo, which must be installed, taking about three minutes.
#
# Usage:
#   tools/install-naga.sh <dir> <version>
#
# <version> is a naga-cli version, such as 30.0.2. naga is installed as
# <dir>/bin/naga. Put <dir>/bin on the PATH in each shell that configures a web
# build.

set -euo pipefail

if [[ $# -ne 2 || -z "$1" || -z "$2" ]]; then
  echo "Usage: $0 <dir> <version>" >&2
  exit 2
fi
dir="$1"
version="$2"

if ! command -v cargo > /dev/null; then
  echo "cargo was not found: install Rust (https://rustup.rs) first" >&2
  exit 1
fi
cargo install naga-cli --version "${version}" --locked --root "${dir}"
echo "Installed naga ${version} in ${dir}/bin."
echo "Put ${dir}/bin on the PATH to use it."
