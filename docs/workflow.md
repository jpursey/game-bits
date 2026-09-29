# Workflow

How work is done in every project built on Game Bits, including Game Bits
itself. Each project's CLAUDE.md imports this file and adds what is specific to
the project: its commands, its libraries and their dependency order, the checks
every change gets, and its own conventions.

This file is shared, so it only says what holds in every project. Improve it
from whichever project shows the need (see [Game Bits](#game-bits)).

## Working with the user

- Commit only after the user explicitly approves the change, or says to commit.
  Passing tests, answered questions, or no objection to "if you're happy I'll
  commit" are not approval. Show the final diff, with any temporary code
  removed, and wait.
- Don't `git push`, or suggest pushing. The user pushes.
- Before handing a significant change (real code, not a one-line tweak or a doc
  edit) to the user, run `/simplify` and apply its fixes, then run
  `/code-review` at `medium`, and report what each found.
- Don't add new dependencies without asking.
- Don't reformat files you aren't otherwise changing.

### Names

- Offer concise, plain words: two or three candidates, the existing terms each
  one avoids colliding with, and a recommendation. No jargon, abbreviations, or
  catch-alls ("building block", "manager", "helper").
- "Add" creates something that the owner registers and keeps (`AddChild()`).
  "Create" is for factories that return a `unique_ptr` to the caller.
- A private helper behind a public function of the same meaning takes a `Do`
  prefix (`DoAddChild()` behind `AddChild()`), or an `Impl` suffix for a type.
- Don't add symmetric APIs nothing needs, such as a `Remove` no caller uses.

## Feature workflow

A feature is designed first, then built and reviewed as a series of small
changes (CLs):
- Break the feature into CLs that are each limited to one library where
  possible, built in the project's dependency order.
- Track the plan in `docs/worklog/<feature>.md`, starting from a copy of
  `docs/worklog/template.md`: a design summary, then each CL with its
  dependencies, a status (`[ ]` not started, `[~]` in progress, `[x]`
  submitted), and **Verify** steps. The steps are the checks every change gets
  (see the project's CLAUDE.md), plus feature-specific tests and any
  performance measurements.
- The first edit for a CL flips its status to `[~]`, and it becomes `[x]` in the
  CL's own commit.
- After writing each CL, self-review it before handing it to the user. Check
  that it is correct, clean, simple, and not wasteful, and look for brittle
  design: ask "what does a caller have to remember to get this right?" (paired
  Add/Remove or Register/Unregister calls, state that must be manually kept in
  sync, ordering assumptions). Prefer designs that enforce it, such as RAII
  handles, private internals, and types that make misuse impossible, and call
  out any remaining brittleness.
- Build and check the CL, then the user reviews it (and tests it, where the
  project needs testing by hand). Once the user approves, mark the CL complete
  in the plan and commit it.
- The feature is complete when the user says so, not when the last planned CL
  lands. Only then, replace the per-CL plan with a summary of the final
  implementation (behavior, structure, and reusable building blocks), so the
  doc stays a useful reference. Move anything left undone (ideas set aside,
  known limitations worth fixing, follow-ups the user asked for) into the
  backlog as part of the same change, rather than into the summary.

Work that comes down to one or two simple CLs doesn't follow this workflow. It
is done as an ordinary change, reviewed and committed without a worklog plan.

## Backlog

`docs/backlog.md` is the one home for work that isn't being done yet, so ideas
don't scatter across the worklogs. It is a flat list in rough stack rank order,
each item a heading with **Layers**, **Size**, **Feature workflow**, **Depends
on**, and **Background** fields, then a short description. Worklog docs have no
"Future ideas" section of their own.
- Anything noticed that is worth doing but out of scope belongs there, rather
  than in a comment or a worklog.
- **Feature workflow** says whether the item follows the workflow above.
- When an item that follows the feature workflow is picked up, it moves into its
  own `docs/worklog/<feature>.md` plan, titled with the item's name, and comes
  out of the backlog. Any other item comes out of the backlog in the commit
  that does it, whose message names the item.

### Across projects

- Each item is work in its own project only. Work that another project needs
  is an item in that project's backlog.
- An item that needs another project's item names it in **Depends on**, with
  the project: "Game Bits *Profiler module*". Links don't work across
  repositories, so it is named, not linked.
- Before picking up an item, check where each item it depends on stands, and
  say so if one isn't done. Read the other project's committed `main` (such as
  `git -C <project> show main:docs/backlog.md`), not its working tree, which
  may hold work not yet submitted. Leaving the backlog doesn't mean done:
  - **Not started:** the item is still in the backlog.
  - **A feature:** a worklog is titled with the item's name. It is done once
    every CL in it is `[x]`, or its plan has been replaced by a summary.
  - **Any other item:** it is done once a commit on `main` names it
    (`git -C <project> log main --grep "<item name>"`).
- An item added for another project stands on its own, in its own project's
  terms: what it does, and the constraints it must meet. It also carries a
  **Requested by** field naming the project and item that need it. That is for
  ranking only; the item's design and tests never depend on the project that
  asked for it.

## Game Bits

Game Bits is the shared base library. Every project that uses it is checked out
beside it, as a sibling directory (`../game-bits`), and finds it through the
`GB_DIR` environment variable.
- Code that knows nothing of the project using it, such as generic utilities,
  test support, or profiling, belongs in Game Bits rather than in the project.
- Game Bits code is only written in Game Bits sessions. A session in another
  project may make two kinds of change there, both docs: adding an item to its
  backlog, and improving this file. Each is committed in Game Bits after the
  user's review.
- Projects build against Game Bits' main checkout as it stands, uncommitted
  changes included, and read this file from it. Game Bits' CLAUDE.md says how
  its own sessions keep that checkout clean.
- Game Bits is verified by its own unit tests, never by building the projects
  that use it. A change that breaks a public API says so in its backlog item,
  so each project can add its own follow-up.

## Parallel sessions

Work can run in parallel sessions, each in its own git worktree. The project's
CLAUDE.md says which work needs the main checkout, and who lands a side
session's commit on `main`.
- A side session builds and runs the unit tests in its worktree, gets the
  user's review, and commits on the worktree's branch as usual.
- Once the user approves and the change is committed, it sends a message to the
  session named in its prompt with the commit hash, the branch, and a short
  summary of the change and how it was verified.
- Message exactly the session named in the prompt (such as `XTouch utility
  buttons [0eff92]`). If the prompt names none, or that session isn't
  reachable, tell the user instead of picking another session.

When a session starts a side task, whether by spawning it directly or by
suggesting one the user can start later, **call ListAgents first**, before
writing the prompt. Its first line ("This session is `<name> [<hash>]`") is the
only way to learn this session's name and reference, which differ in every
session. The prompt must be self-contained and include the project's parallel
session rules along with that name and reference; without it, the side session
is stranded and has to ask the user who to message.

## Tests

- Test against fakes that hold state, not mocks. A test checks what the code
  produced and how it responded, rather than scripting the calls it makes
  (no `EXPECT_CALL` sequences), so it doesn't break when an implementation
  changes how it gets there.
- Tests never wait on real time. Time comes from something the test controls.

## C++ style

Beyond the Google C++ style guide
(https://google.github.io/styleguide/cppguide.html) and clang-format:
- All comments are `//` style (not `///` or `/*...*/`).
- Comments on a public API say what it does and what a caller needs to know to
  use it correctly, not how it works. Implementation comments only explain what
  isn't obvious from the code itself.
- A comment that follows code at the same indent level has a blank line above
  it. A comment that opens a block (right after a `{`) doesn't need one.
- Sections in a file are separated by `//=====` blocks (extending to column 80)
  surrounding descriptive text: a one line section description, and if
  necessary further description in additional paragraphs. Sections within a
  class, or between groups of related functions, use `//-----` blocks the same
  way.
- Always use a brace block for the body of `if`, `else`, `for`, `while`, `do`,
  and similar statements, even when the body is a single statement. This keeps
  crash callstacks accurate to the line, and lets breakpoints be set on the body
  separately from the condition.
- Don't use `size_t`. Use `int` for indexing, counts, and arithmetic, or a
  sized signed type like `int64_t` when the range needs it. Unsigned types are
  for opaque IDs and bit patterns. Cast container sizes at the boundary:
  `const int count = static_cast<int>(items.size());`.
- Mutable globals, including file-local ones in an anonymous namespace, take a
  `g_` prefix, and static data members and function-local statics take `s_`.
  Constants use `kName`.
- Include the header that declares a type rather than forward declaring it.
  Forward declarations are only for breaking an include cycle within a library,
  or a circular reference within the header itself.
- A local helper is a private member function, not a one-off lambda inside
  another function. Lambdas are for APIs that need a callable, ideally as a thin
  call to a member function.
- Keep hierarchies shallow. For a capability some subclasses need, add a
  protected virtual that does nothing by default to the existing base, rather
  than an intermediate base class. A virtual meant to be overridden is
  `protected`, never `private`.
- Prefer existing libraries over hand-rolled utilities: Game Bits itself, then
  Abseil and the other Google open source libraries vendored in Game Bits'
  `third_party/`.
- Where Abseil and the C++ standard library both have a type, use Abseil's:
  `absl::Span`, not `std::span`.
- Don't use iostreams. Build text in a `std::string` with Abseil's string
  utilities (`absl::StrCat`, `absl::StrAppend`, `absl::StrFormat`), and write
  files through Game Bits' `gb/file` when an abstraction over writing is
  needed.
- Files in the working tree use CRLF line endings (git `core.autocrlf` is true);
  leave them that way. In Git Bash, `sed -i` rewrites files as LF-only, so
  prefer the Edit tool. If sed is used, restore CRLF afterwards (watch for files
  without a trailing newline) and check `git diff --stat`.
