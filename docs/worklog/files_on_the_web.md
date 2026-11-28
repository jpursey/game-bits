# Files on the web

A program names its assets in its CMake with `gb_pack_assets()`: a folder,
and the files and folders in it that the program reads. In a web build, a page
gets them packed in one file beside it (`<name>.data`), which the page fetches
before `main()` and puts in its file system, which is in memory. From then on
they are ordinary files: every read in `gb_file` (and so in `gb_resource`,
`gb_image`, and ImGui's fonts, which read through it) returns at once, without
threads, as it does elsewhere. On Windows and Linux, and for a program for Node
in a web build, the command only checks that each named path is there: the
program reads its assets where they are, as now. [Web
support](../designs/web_support.md) is the design for the whole project, with
its scope and the decisions every feature shares.

Nothing in `gb_file` or `gb_resource` changes. A page's current folder is the
root of its file system, and a packed folder is at `/<its name>` there, so a
program that finds its asset folder by name in or above its current folder, as
block-world does, finds it on every platform with no code of its own for the
web.

`templates/sdl` becomes the program that proves it: it gets an `assets` folder
with one file, packs it, and reads it at startup, on every platform, so CI's
run of the template in a headless browser checks a page's assets on every run.
render-check and block-world are built for the web in *Web demos*, which uses
the command as [Examples](#examples) describes.

## Behavior

- `gb_pack_assets(<target> <folder> <path>...)`, in
  `CMake/GameBitsTargetCommands.cmake`, names a target's assets:
  - `<folder>`: the folder the program finds its assets in, relative to the
    current source directory or absolute (such as `"${GB_DIR}/assets"`).
  - `<path>...`: at least one file or folder in `<folder>`, relative to it, to
    pack. A folder packs everything in it.
  - It may be called more than once for a target, with other folders, or more
    paths in the same one. Two folders of the same name for one target are an
    error, as they would be the same folder in a page.
- On every platform, configuring fails if a named path isn't there, or isn't
  in `<folder>` (an absolute path, or one with `..`), so a mistake is found on
  Windows and Linux too, not only in a web build.
- On Windows and Linux, and for a program for Node in a web build (which reads
  the machine's own files, as on Linux), that is all it does.
- In a web build, for a page (a windowed executable, see *Programs in the
  browser*):
  - The named files are packed into `<name>.data`, beside `<name>.html`,
    `.js`, and `.wasm` in `GB_BIN_DIR`, when the page is linked. The page is
    linked again whenever a packed file changes, or a file is added to or
    removed from a packed folder.
  - The page fetches `<name>.data` from beside itself, before `main()`, and
    `main()` starts once it has arrived. Each packed file is at
    `/<folder's name>/<path>` in the page's file system, and the page's
    current folder is `/`.
  - Packed files can be written, renamed, and deleted, like any file in a
    page, until the page closes (keeping them is *Saved files on the web*).
  - If `<name>.data` can't be fetched (such as a page copied without it),
    the browser's console shows the error, and `main()` never starts. The
    shell sets `gbExit` to the error's message, so `tools/run-program.sh`
    fails the run at once, as for an abort.
- Assets of more than 15 MB in all: configuring warns that the page's
  `<name>.data` can't be beside a Claude artifact's page, which allows 15 MB
  a file. The page still builds, and works from a web server (GitHub Pages
  allows 100 MB a file). Splitting the package so a page can have more is the
  backlog's *Asset packages over 15 MB*.
- `CMake/GameBitsShell.html` keeps working with a package: its `preRun`, which
  writes a tool's `gbFiles`, becomes a list, as Emscripten's package loader
  adds its own step to it. A page that can't start its program (its package
  can't be fetched) ends with `gbExit` set to the error's message.

## Design

### Names

- `gb_pack_assets()` says what a web build does with them, beside
  `gb_embed_data()` and `gb_embed_shader()` (verb, then what). Elsewhere it
  only checks them, which the name doesn't hide: the files are packed with the
  program where the platform needs it. `gb_add_assets()` would read as
  creating a target, as `gb_add_library()`, `gb_add_executable()`, and
  `gb_add_test()` do, and `gb_target_assets()` follows CMake's
  `target_sources()`, which no `gb_` command does. Recommended:
  `gb_pack_assets()`.
- "Assets" is what README.md, block-world (`asset:`), and the `assets/` folder
  already call them. "Package" is Emscripten's word for `<name>.data` (its
  file packager), used here only for that file, not for the command.

### gb_pack_assets

```
# Names files a program reads at run time, its assets, so that they are there
# on every platform:
#
#   gb_pack_assets(<target> <folder> <path>...)
#
# For example, gb_pack_assets(block_world "${GB_DIR}/assets" fonts textures)
# packs the fonts and textures folders in Game Bits' assets folder.
#  <folder>  The folder the program finds its assets in, relative to the
#            current source directory or absolute.
#  <path>    A file or folder in <folder> to pack, relative to it, with no
#            "..". A folder packs everything in it.
#
# In a web build, a page (see gb_set_web_page_properties) has them in its file
# system before main(), at /<folder's name>/<path>, and its current folder is
# /. So a program that finds its assets folder by name in or above its current
# folder finds it in a page too, as it does on Windows and Linux when run from
# its project's root or from GB_BIN_DIR. They are packed into <name>.data,
# beside the page, which must be served with it. The page is linked again
# whenever a packed file changes, or one is added to a packed folder.
#
# Everywhere else, including a program for Node in a web build, the program
# reads them where they are, and this only checks that each path is there.
#
# Configuring warns if a page's assets are more than 15 MB in all, as a Claude
# artifact allows no more for any file beside its page.
function(gb_pack_assets TARGET FOLDER)
```

- It is in `GameBitsTargetCommands.cmake` beside `gb_embed_data()`, as every
  project that uses Game Bits' CMake commands gets it with them.
- In a web build, for a page, it adds one link option for each path,
  `--preload-file <folder>/<path>@/<name>/<path>` (with each `@` in the
  path written `@@`, as Emscripten reads `@` as the separator), and adds every
  packed file to the target's `LINK_DEPENDS`, so a change relinks it. A
  folder's files are found with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`, so
  Ninja looks for new or removed files on each build, as a `CMakeLists.txt`
  change would.
- Emscripten's linker runs its file packager on them, which writes
  `<name>.data` (the files' bytes, one after another) and puts the code that
  fetches it in `<name>.js`. That code adds a step to `Module.preRun` that
  waits for the fetch and makes each file in the file system, and holds back
  `main()` until it is done (Emscripten's run dependencies). The files are
  views of the fetched bytes, kept in JavaScript's memory, not the program's,
  until a read copies them in.
- Whether a target is a page is read from its `WIN32_EXECUTABLE` property, as
  `gb_set_target_properties()` does, so it is called after the target is
  defined.
- The folder's name must not be one Emscripten's file system already has at
  its root (`dev`, `home`, `proc`, `tmp`), or `pilot`, which
  `tools/run-page.js` uses; configuring fails if it is.
- The total is summed with `file(SIZE)` as it configures, which is exactly the
  size of `<name>.data`, as the packager adds nothing between files.
- Cost: none on Windows and Linux beyond configuring. In a web build, a page
  whose 2.7 MB of assets changed relinked in 0.9 s in Debug (see [To
  confirm](#to-confirm)), and loading them delays `main()` by the time to
  fetch them.

**Brittleness:**
- `<name>.data` must be served beside the page. A page copied or published
  without it never starts `main()`, with only a console error, which a tool
  running it reports at once. *Web demos* publishes the page's folder whole.
- A program must find its assets folder by name, in or above its current
  folder, or open it relative to its current folder with that folder being
  the project's root. A program that opens assets relative to its executable
  instead (as none in Game Bits does) wouldn't find them in a page.
- A program reading a file it didn't name in `gb_pack_assets()` works on
  Windows and Linux, and fails only in a page. It is found the first time the
  page runs it, which a pilot script in CI does for the template.

### The shell

`CMake/GameBitsShell.html` sets `Module.preRun` to a function, and the
package's loader adds its step with `Module.preRun.push()`, which fails ("is
not a function") and leaves the packed files out. `preRun` becomes a list with
the shell's function in it, which Emscripten accepts as well. The shell's step
runs first, then the package's, so a tool's `gbFiles` must not name a packed
file (the tools only write `/pilot`).

The package's loader fetches it with nothing to catch a failure, so a failed
fetch is a promise rejection nothing handles, and the program never starts.
Until Emscripten calls `onRuntimeInitialized`, just before `main()`, the shell
takes such a rejection as the program's end, and sets `gbExit` to its message
(such as "Error: 404: http://localhost/tiny.data"), which `tools/run-page.js`
reports as an abort. After that, rejections are left to the browser, as now.

### Choosing how to pack

Four ways for files to be in a page's file system before `main()`, each tried
with a scratch page (see [To confirm](#to-confirm)):
- **`--preload-file` (chosen).** Emscripten's linker packs the files into
  `<name>.data` beside the page, and the page fetches it before `main()`. The
  `.wasm` doesn't grow, the files are in JavaScript's memory until read, and
  it needs nothing beyond the Emscripten SDK. A page and its package are
  separate files, each held to an artifact's 15 MB on its own.
- **`--embed-file`.** The files are built into the `.wasm` itself, which
  grows by their size: 2.7 MB of assets made a 40 KB `.wasm` 2.7 MB, and 16.6
  MB made it 16.6 MB. They are in the program's memory for as long as it
  runs, and they count against the `.wasm`'s 15 MB in an artifact, which a
  Release `.wasm` of block-world may already come close to (*Web demos*).
  Loading took as long as preloading.
- **Emscripten's file packager, run by the build.** The same packager, run as
  a custom command, with its loader given to the page with `--pre-js`. It does
  what `--preload-file` does, with a command and a loader file of its own to
  keep. It is what *Asset packages over 15 MB* needs, to make several
  packages for one page, and nothing needs it until then.
- **Fetching each file in the shell's `preRun`.** One request for each file,
  each held to 15 MB on its own, from a list the build writes and the shell
  reads, holding `main()` back with Emscripten's run dependencies. It would be
  Game Bits' own loader, in the shell, doing what the packager does, with one
  request for each file where a server or browser limits how many are made at
  once, and an artifact allows 255 files in a publish. Its one advantage, no
  limit on the total, is what splitting the package also gives.

Neither form needs threads: the fetch is the browser's, before `main()`, and
every read after it is from memory.

### Paths on every platform

Programs find their assets relative to their current folder, and that folder
differs by how a program is run:

| Run from | Current folder | Assets |
|---|---|---|
| Visual Studio, Windows | `GB_BIN_DIR` (`VS_DEBUGGER_WORKING_DIRECTORY`) | in its parent, such as `bin/../assets` |
| `tools/run-program.sh`, Linux | the folder it is run in, the project's root by default | in it |
| A page | `/` | `/assets` |

A program that looks for its asset folder by name, in its current folder and
then each folder above it (block-world's `InitFileSystem()`), finds it in each
case. In a page, the folder above `/` is `/` itself, so the search ends there.
This needs no change to `LocalFileProtocol`: its root is the found folder, as
now (`/assets` in a page), and a protocol from `CreateOsRoot()`, as in
`CreateCliFileSystem()`, opens `assets/...` relative to `/`.

Finding the folder this way is code each program has (block-world, and the
template after CL2). It could be a function in `gb_file` once a third program
needs it; until then, the template's copy is what new projects start from.

### gb_file and gb_resource

Nothing changes. A page's file system is Emscripten's in memory one, which
`std::filesystem` and the C library reach directly, so `LocalFileProtocol`
lists, reads, queries, and writes packed files as it does any file in a page.
`ResourceSystem` and `LoadImage()` read through `FileSystem`, and ImGui's
fonts through `ImFileOpen()`, which opens them in the context's `FileSystem`,
so each reads a packed file at once. Writes stay in memory until the page
closes (*Saved files on the web*).

### Examples

- **render-check** reads no files: its shaders are built into it with
  `gb_embed_shader()`. It doesn't call `gb_pack_assets()`.
- **block-world** reads three fonts (through ImGui), `textures/block.png`, and
  three compiled shaders from `asset:/shaders`, from Game Bits' `assets`
  folder, which it finds by name from its current folder. *Web demos* compiles
  its shaders in the build, embedded as render-check's are, so the page packs
  only its fonts and textures:
  `gb_pack_assets(block_world "${GB_DIR}/assets" fonts textures)`. That is
  2,665,844 bytes: the fonts folder's 25 fonts and their 3 licenses (2,565,666
  bytes) and the 100,178 byte texture. Packing only the three fonts it uses,
  with their licenses, would be about 300 KB; the folders are simpler, and far
  under 15 MB. Its ImGui layout (`imgui.ini`) is written in memory.
- Assets shared between a project and Game Bits (as block-world's are) are
  named from wherever they are: `<folder>` may be outside the project.

### To confirm

Checked with scratch pages outside the repository, built with Emscripten 7.0.1
with a page's link options (`-sJSPI -sENVIRONMENT=web
-sEXPORTED_RUNTIME_METHODS=FS -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1
-sSTACK_SIZE=1MB`) and Game Bits' shell, run with `tools/run-program.sh` in
headless Chromium 158:
- With the shell as it is, `--preload-file` packed the files but the page
  reported "Module.preRun.push is not a function", and `main()` started
  without them. With `preRun` as a list, both the shell's step and the
  package's ran. (CL1)
- `-O2`, packing all of `assets/fonts` and `assets/textures` (30 files,
  2,665,844 bytes), and a 14,000,000 byte file with the fonts (16,565,666
  bytes):

  | Form | `.wasm` | `.data` | `main()` started (ms after the page opened) | Every file read |
  |---|---|---|---|---|
  | Nothing packed | 40,215 | | 94 to 121 | |
  | `--preload-file`, 2.7 MB | 40,215 | 2,665,844 | 146 to 176 | 4.3 to 7.2 ms |
  | `--preload-file`, 16.6 MB | 40,215 | 16,565,666 | 556 to 707 | 6.1 to 7.3 ms |
  | `--embed-file`, 2.7 MB | 2,705,056 | | 171 to 199 | 3.6 to 7.0 ms |
  | `--embed-file`, 16.6 MB | 16,605,063 | | 581 to 619 | 5.3 to 6.2 ms |

  The page was served by Playwright from the local disk, so the times are
  the browser's own work: on a network, fetching the bytes is most of it, and
  it is the same for both forms. `<name>.js` grew by 6.5 KB with the
  package's loader. (CL1)
- A page whose `<name>.data` was missing logged "404" as a page error, and
  `main()` never started, so the run ended at its timeout. CL1's shell ends
  such a run in a second instead, with the error's message.
- The page's current folder is `/`. (CL1)
- With Game Bits' own web build, a prototype of `gb_pack_assets()` in a
  scratch project (a `gb_add_win_executable()` linking `gb_file`, `gb_image`,
  and `gb_cli`, Debug, with the shell's `preRun` as a list) packed
  `"${GB_DIR}/assets"`'s `fonts` and `textures/block.png` (30 files,
  2,665,844 bytes, the sum configuring printed). Its page found `/assets` by
  name from `/` as block-world does, and through a `LocalFileProtocol` rooted
  there listed all 30 files, read each with the size `GetPathInfo()` gave,
  loaded the PNG with `LoadImage()` (256x256), and wrote a new file. A
  `CreateCliFileSystem()` read `assets/fonts/...` relative to `/`. All of it
  took 37 to 56 ms. The Debug `.wasm` was 13 MB. (CL1)
- Touching a packed file relinked the page in 0.9 s; a build with no change
  did nothing. (CL1)

Settled by CL1, with the command and shell in the repository:
- Configuring fails, with a message naming the path, for a path that isn't
  there, an absolute path or one with `..`, no paths, a folder that isn't
  there, a target that isn't defined, a folder named `tmp`, and two folders
  named `assets` for one target. Packing the same folder twice is allowed.
- A file named `at@name.txt` and a folder named `with space` are packed and
  read back by their names.
- A build with no change does nothing; touching a packed file relinks the
  page; adding a file to a packed folder reconfigures (Ninja's glob check)
  and relinks it. Removing one, or adding one older than the page (`cp -p`),
  changes no file the page depends on, so the command also keeps a list of
  the packed files in the build tree (`<target>_packed_assets.txt`), written
  only when it changes, which the page depends on too. With it, both relink
  the page, and its `.data` follows.
- Paths that overlap (a folder, and a file in it, or a folder named twice)
  are counted once toward the 15 MB, as the packager packs them once. A path
  written with backslashes is checked as if written with `/`, so
  `..\other` fails on every platform.
- A page of 15,500,000 bytes of assets warns as it configures, and builds.
- A page with no assets, and one with assets, both still get the tool's
  `gbFiles` (the pilot script), and a program for Node that calls
  `gb_pack_assets()` gets no `.data` and reads the files on disk.
- The spike above, rebuilt with the repository's command and shell, passes
  as before. Built with threads, it links, and ran the same in the page
  runner (which serves cross-origin isolation headers).
- The shell takes any promise rejection nothing handles before `main()` as
  the program's end, as nothing else in a page's start leaves one. A tool's
  `gbFiles` can't name a packed file: the packager's step then fails, and
  the program doesn't start.

Not checked:
- A page and its `<name>.data` as a Claude artifact. The design's findings say
  a `.wasm` beside an artifact's page loads, which is the same kind of fetch,
  from the same folder. An artifact takes only standard web media types, so
  `<name>.data` may need publishing as `application/octet-stream`. (*Web
  demos*)
- Whether GitHub Pages or an artifact compresses `<name>.data` when it serves
  it. Fonts compress well; a texture already compressed doesn't. Emscripten
  can pack with LZ4 (`--lz4`), keeping the bytes compressed in memory and
  decompressing each read, which nothing here needs. (*Web demos*)
- Load times over a real network. (*Web demos*)

## CLs

### CL1 [x] build: gb_pack_assets

Depends on: *Programs in the browser* CL1.

- `GameBitsTargetCommands.cmake`: `gb_pack_assets()`, as in
  [gb_pack_assets](#gb_pack_assets). The file's header comment lists it.
- `CMake/GameBitsShell.html`: `preRun` is a list, and a failed fetch of the
  package before `main()` sets `gbExit`, as in [The shell](#the-shell). Its
  comment says a page's packed assets are in its file system before `main()`.
- CLAUDE.md's Build system: a bullet for `gb_pack_assets()`, beside
  `gb_embed_data()`. Its Web section: a page's assets are in `<name>.data`
  beside it.
- Unused in Game Bits' own build, which has no windowed program.

**Verify**
- Standard checks (the CMake change is for every platform). Configuring with a
  scratch project that calls it with a path that isn't there, or with `..`,
  fails on Linux.
- Web without threads: a scratch page (in the session's scratchpad, a
  `gb_add_win_executable()` that links `gb_file` and `gb_image`) packs
  `"${GB_DIR}/assets"`'s `fonts` and `textures`, and, run with
  `tools/run-program.sh`, finds the `assets` folder from its current folder as
  block-world does, lists every packed file through a `LocalFileProtocol`
  rooted there, reads each with the size `GetPathInfo()` gives, loads
  `textures/block.png` with `LoadImage()`, and writes a new file there. A
  `gbFiles` pilot script still reaches it. Changing a packed file relinks the
  page; building again without a change doesn't.
- Web with threads: the scratch page links.
- Linux: the same scratch project builds, makes no `.data`, and run from the
  repository's root reads the same files.

### CL2 [ ] templates: templates/sdl reads its assets

Depends on: CL1, *Programs in the browser* CL5 and CL6.

- `templates/sdl/assets/`, with one small text file (such as `welcome.txt`),
  and `gb_pack_assets(sdl_template "${CURRENT_DIR}/assets" welcome.txt)` in
  its `src/CMakeLists.txt` (`CURRENT_DIR` is the template's root, as its
  top-level `CMakeLists.txt` sets).
- `templates/sdl/src/main.cc`: `Init()` finds the `assets` folder by name from
  the current folder, as in [Paths on every platform](#paths-on-every-platform),
  registers it as the `asset` protocol of a `FileSystem` in the game's
  context, reads `asset:/welcome.txt`, and logs it. Init fails if it can't, so
  the program exits with an error. Its deps gain `gb_file`.
- `templates/README.md`: a project's assets, `gb_pack_assets()`, and serving
  `<name>.data` with the page.
- A cross-platform change: the template reads a file at startup, and needs
  its `assets` folder in or above its current folder.

**Verify**
- Standard checks, and the template builds on Linux and runs with
  `tools/run-program.sh --steps 'hide window; wait 2'`, from the template's
  root and from its `bin`, logging the file's text. Run from `/tmp`, it fails
  with its error.
- Web: the template builds as a page with `sdl_template.data` beside it, and
  the same steps pass in the headless browser, with the file's text in
  `stdout.txt` or `stderr.txt`. With `sdl_template.data` moved away, the run
  fails at once, with the 404 in its log.
- A `workflow_dispatch` run of the CMake workflow on the PR's branch passes,
  with `build-templates` on Windows and Linux and the template's page run in
  `build-web-template`, which now checks its assets too.
