# Backlog

Work that isn't being done yet, for Game Bits itself or asked for by the
projects built on it. The worklogs in `docs/worklog/` describe what was built;
this is what has not been. See [workflow.md](workflow.md) for how items are
written, picked up, and requested by other projects.

The list is a rough stack rank: the order the items look worth doing, as a best
guess rather than a commitment. Re-order it freely.

Each item carries:

- **Layers**: the Game Bits libraries it touches, lower tiers first. More than
  one usually means more than one CL.
- **Size**: a guess. *Small* is a single CL. *Medium* is a few. *Large* is
  many, usually after a design.
- **Feature workflow**: whether the item follows the feature workflow, with a
  design and a `docs/worklog/` plan of CLs. Anything that comes down to one or
  two simple CLs doesn't, and is done as an ordinary change.
- **Depends on**: other items that should come first, or "nothing".
- **Requested by**: the project and item that need it, for items another
  project asked for. For ranking only.
- **Background**: where the context is, if anywhere.

## Profiler module

- **Layers:** a new `gb/profile` library
- **Size:** large
- **Feature workflow:** yes
- **Depends on:** *Function hooks*, for timing calls through function pointers
- **Requested by:** JPRSurf *Profiler*
- **Background:** none

A profiler for a program with a main loop, cheap enough to leave on all the
time in a realtime program:
- **Points**, each defined once in code where it is used, of five kinds:
  frames (one iteration of the program's loop), scopes (a section of the
  program's own code), calls (a call out to another system), counters (work
  done, such as messages sent), and values (numbers describing the workload,
  set when they change).
- **Self time:** each timed point records its time less the time of the points
  inside it. A call out to another system that calls back into the program is
  split correctly between the two.
- **Frames:** a histogram of frame times, with a few buckets for each power of
  two so percentiles are cheap, and the breakdown by point of the slowest frame
  so far. A frame over a threshold the program sets is reported with its
  breakdown.
- **Cost:** a timed point is two reads of the CPU's timestamp counter
  (`__rdtsc`), and an add into a fixed slot, with no lookups or allocation,
  aiming at 20-25ns. The profiler measures its own cost when it starts, reports
  its share of each frame, and checks it against a budget the program gives it
  (a fixed time per frame, a fraction of the frame, or the larger of the two).
- **Calls through function pointers:** a hook type for *Function hooks* that
  times every call through the pointer as a call point.
- **Output:** a plain text report, with the values as a header, and one line
  per point in a stable order, so two reports diff cleanly.
- **For tests:** counts can be read by name, and everything can be reset.

Unit tested with a timestamp source the test controls, so no test reads real
time. To confirm while designing it: that the timestamp counter is invariant on
the machines it runs on, and what a timed point actually costs.

## Reformat the files clang-format has drifted from

- **Layers:** any
- **Size:** small
- **Feature workflow:** no
- **Depends on:** nothing
- **Background:** CLAUDE.md (Format)

About 18 files were formatted with an older clang-format, so formatting any of
them churns unrelated lines, and CLAUDE.md has to warn about it. One commit
that only reformats them, with nothing else in it, would remove the warning.
