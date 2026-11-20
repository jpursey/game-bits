#!/usr/bin/env bash
# Copyright (c) 2026 John Pursey
#
# Use of this source code is governed by an MIT-style License that can be found
# in the LICENSE file or at https://opensource.org/licenses/MIT.

# Runs a program built on Game Bits with a pilot script, so a session can check
# what it draws and how it responds without the user. This is the Linux version
# of run-program.ps1.
#
# Usage:
#   tools/run-program.sh --exe <path> [--script <file> | --steps <steps>]
#                        [--arguments <arguments>] [--out-dir <dir>]
#                        [--working-directory <dir>] [--timeout-secs <seconds>]
#
# The program must take the pilot flags (--pilot and --pilot-output, from
# gb_pilot_script), and runs the script by itself: src/gb/pilot/script/
# pilot_script.h lists the steps. --steps is a script written inline, with ";"
# between steps (so ";" can't appear in a step's text), which is saved to
# <out-dir>/steps.txt. With neither, the steps save one frame once the program
# has had time to draw. --arguments are the program's other arguments, as they
# would be typed after it on a command line (quotes group words, but nothing is
# expanded).
#
# The program runs in a virtual X server of its own (xvfb-run, with a 1920x1080
# screen), so it never appears on a desktop and needs no display.
#
# The program exits once the script ends. The run fails if it exits with an
# error code, which it does if a step failed or an error was logged, or is still
# running <timeout-secs> after it started. In that last case, the stack of each
# of its threads is saved to <out-dir>/hang-stacks.txt (see dump-stacks.sh)
# before the program is ended.
#
# Saved frames go to <out-dir>, which defaults to out/run/<program name> in the
# current directory, as <name>.png. The program's stdout and stderr (the log)
# are saved there too. At the end, the frames this run saved, the log's errors,
# and the run's summary are printed.

set -euo pipefail

tools_dir="$(dirname "$(realpath "$0")")"
exe=""
script=""
steps="hide window; wait 30; save frame"
has_steps=false
arguments=""
out_dir=""
working_directory=""
timeout_secs=60

fail() {
  echo "$1" >&2
  exit 1
}

while [[ $# -gt 0 ]]; do
  if [[ $# -lt 2 ]]; then
    fail "$1 needs a value."
  fi
  case "$1" in
    --exe) exe="$2" ;;
    --script) script="$2" ;;
    --steps) steps="$2"; has_steps=true ;;
    --arguments) arguments="$2" ;;
    --out-dir) out_dir="$2" ;;
    --working-directory) working_directory="$2" ;;
    --timeout-secs) timeout_secs="$2" ;;
    *) fail "Unknown option $1." ;;
  esac
  shift 2
done

if [[ -z "$exe" ]]; then
  fail "--exe is required."
fi
if [[ ! -x "$exe" ]]; then
  fail "$exe is not built."
fi
if [[ -n "$script" && "$has_steps" == true ]]; then
  fail "Give --script or --steps, not both."
fi
exe="$(realpath "$exe")"
name="$(basename "$exe")"
if [[ -z "$out_dir" ]]; then
  out_dir="out/run/$name"
fi
if [[ -z "$working_directory" ]]; then
  working_directory="$PWD"
fi

# Full paths, as the program runs in its own working directory.
mkdir -p "$out_dir"
out_dir="$(realpath "$out_dir")"
if [[ -n "$script" ]]; then
  script="$(realpath "$script")"
else
  script="$out_dir/steps.txt"
  printf '%s\n' "${steps//;/$'\n'}" > "$script"
fi

# xargs splits the arguments as a shell would, honoring quotes, without
# expanding anything.
program_args=()
if [[ -n "$arguments" ]]; then
  mapfile -t program_args < <(printf '%s' "$arguments" | xargs printf '%s\n')
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
touch "$work/started"
log="$out_dir/stderr.txt"
stacks="$out_dir/hang-stacks.txt"
rm -f "$stacks"

# The program runs as a descendant of this subshell, which becomes gdb if the
# program times out: where ptrace is limited to a process's descendants (as
# Ubuntu's Yama does by default), only an ancestor can read its stacks. The
# program writes its own process ID to $work/pid, as xvfb-run runs it in a shell
# of its own.
status=0
(
  xvfb-run -a -s "-screen 0 1920x1080x24" \
    bash -c 'echo $$ > "$1"; cd "$2"; exec "${@:5}" > "$3" 2> "$4"' \
    -- "$work/pid" "$working_directory" "$out_dir/stdout.txt" "$log" \
    "$exe" "${program_args[@]}" --pilot "$script" --pilot-output "$out_dir" &
  runner=$!
  deadline=$((SECONDS + timeout_secs))
  while [[ -n "$(jobs -rp)" ]]; do
    if ((SECONDS >= deadline)); then
      touch "$work/timed-out"
      exec "$tools_dir/dump-stacks.sh" "$(cat "$work/pid")" > "$stacks" 2>&1
    fi
    sleep 0.1
  done
  wait "$runner"
) || status=$?

failure=""
if [[ -e "$work/timed-out" ]]; then
  echo "Saved thread stacks to $stacks"
  pid="$(cat "$work/pid")"
  kill "$pid" 2> /dev/null || true
  for _ in $(seq 100); do
    if ! kill -0 "$pid" 2> /dev/null; then
      break
    fi
    sleep 0.1
  done
  kill -9 "$pid" 2> /dev/null || true
  failure="$name was still running $timeout_secs seconds after it started."
elif [[ $status -ne 0 ]]; then
  failure="$name exited with code $status."
fi

find "$out_dir" -maxdepth 1 -name '*.png' -newer "$work/started" \
  -printf '%T@ %p\n' | sort -n | cut -d' ' -f2- | sed 's/^/Saved /'
echo "Log: $log"

# Errors and fatal errors, as Abseil logs them, and the pilot's summary.
grep -E '^[EF][0-9]{4} |Pilot run ' "$log" || true
if [[ -n "$failure" ]]; then
  fail "$failure"
fi
