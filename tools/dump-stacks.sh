#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Prints the call stack of each thread in a running program, to see where it is
# stuck when it hangs. This is the Linux version of dump-stacks.ps1.
#
# Usage:
#   tools/dump-stacks.sh <process id>
#
# For example, for a program the user is running:
#   tools/dump-stacks.sh "$(pgrep -n render_check)"
#
# Each thread is listed with its name. Functions and source lines come from the
# program's debug information, so a Release build shows only functions.
#
# This attaches gdb to the program, which stops it while its stacks are read.
# Where ptrace is limited to a process's descendants (as Ubuntu's Yama does by
# default), gdb can only attach to a program this was started under, or as
# root. run-program.sh starts its program that way.

set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <process id>" >&2
  exit 1
fi

# exec, so that gdb is the same process as this script, and so an ancestor of
# any program this script is an ancestor of. run-program.sh relies on this.
exec gdb --batch --nx --pid "$1" \
  -ex "set pagination off" \
  -ex "set print thread-events off" \
  -ex "set debuginfod enabled off" \
  -ex "thread apply all backtrace"
