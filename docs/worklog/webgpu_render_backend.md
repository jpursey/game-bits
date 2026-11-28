# WebGPU render backend

`gb_render` draws in a browser. A new backend, `WebGpuBackend` in
`gb_render_webgpu`, implements `RenderBackend` on WebGPU, through Dawn's
bindings for Emscripten (the `emdawnwebgpu` port, which Emscripten 7.0.1 pins
to Dawn's release of 5 November 2026). A program creates it in place of
`VulkanBackend` and otherwise uses `gb_render` exactly as it does on Windows
and Linux: the same resources, materials, passes, draw lists, and pilot frame
saves. A web build converts every shader `gb_embed_shader` compiles to WGSL,
so a project's CMake doesn't change. ImGui draws with it, and render-check runs
as a page: by hand in a browser, and with its seven pilot scripts in headless
Chromium, in sessions and CI, saving frames as it does with Vulkan on Linux.
[Web support](../designs/web_support.md) is the design for the whole project,
with its scope and the decisions every feature shares, and
[Programs in the browser](programs_in_the_browser.md) is the feature this
builds on: pages, the frame wait, and `tools/run-program.sh` for pages.

`RenderBackend` and `gb_render`'s API don't change. On Windows and Linux,
gb_imgui's font atlas is clamped to its edge rather than to a transparent
border, which draws the same pixels, and render-check chooses its backend
through a small interface of its own. Nothing else changes there.

## Behavior

- `WebGpuBackend::Create()` makes a backend that draws into the page's canvas
  (`#canvas`, which SDL's window is). It blocks until the browser gives it a
  device, and fails, with an error, if there is none, or the device lacks
  `indirect-first-instance`, as `VulkanBackend` fails without its equivalent.
- The frame is the canvas's size, which SDL sets as the window is created and
  resized, read as each frame begins.
- Frames look as they do with Vulkan: the same colors (the frame is sRGB, as
  Vulkan's swap chain is), the same orientation and winding, multisampled with
  4 samples (Vulkan takes up to 8, so edges differ by a shade), and
  `EndFrameAndReadImage()` returns the same pixels.
- What WebGPU can't do, and what the backend does instead:
  - `kRgba4` textures are kept as `kRgba4` for the CPU (`Get()`, `Edit()`, and
    saving give back what was set), and expanded to 8 bits a channel for the
    GPU, which reads the same values.
  - Indirect draws are one call per draw, as on Vulkan without multi-draw.
    `IsMultiDrawSupported()` is false.
  - `kClampBorder` fails to create the texture, with an error naming
    `kClampEdge`. gb_imgui no longer uses it.
  - `RasterMode::kLine` fails to create the pipeline, with an error.
  - A vertex type with a 3-channel 8 or 16-bit attribute (`kI8Norm3`,
    `kU16Vec3`, and the like) fails to create the pipeline, with an error.
    WebGPU has no 3-channel vertex formats narrower than 32 bits.
- Limits that are lower than `gb_render`'s, each failing creation with an
  error: a texture array has at most the device's `maxTextureArrayLayers`
  (256 on SwiftShader; `gb_render` allows 2048), a binding index is at most
  499 (`kMaxBindingIndex` is 1023), and a pipeline has at most 8 constants
  bindings across its three sets.
- `CreateShaderCode()` takes WGSL text, which `gb_embed_shader` makes in a web
  build. A shader with an error fails to create, with the browser's message.
- Every WebGPU error the browser reports is logged as an error, so a pilot run
  fails on it, as one fails on a Vulkan validation error.
- The page exits once the program returns, after the backend is destroyed:
  the backend releases every WebGPU object it holds, without which the page's
  runtime stays alive.
- In a web build, `gb_embed_shader` compiles GLSL to SPIR-V with `glslc` as
  before, then converts it to WGSL with `spirv-opt` and `naga`, and embeds the
  WGSL. `tools/install-vulkan-sdk.sh` installs `spirv-opt`, and
  `tools/install-naga.sh` installs `naga`.
- render-check builds for the web, and `tools/run-program.sh --exe
  examples/render-check/bin/render_check.html --script ...` runs each of its
  scripts, as on Linux. CI does the same, and uploads the frames.

## Design

### Names

- `gb_render_webgpu`, in `src/gb/render/webgpu`, beside `gb_render_vulkan`.
  Its classes are `WebGpuBackend`, `WebGpuTexture`, and so on, and its files
  `webgpu_backend.cc` and so on, mirroring `Vulkan`. `Wgpu` would read as
  Dawn's own namespace (`wgpu::Texture`), and `Dawn` names one implementation,
  where the browser's WebGPU may be another's. Recommended: `WebGpu`.
- The files are named for the API, as `vulkan_` files are, not `web_`: `web_`
  names a platform's implementation of a portable interface, and this backend
  is a library of its own, chosen by the program.
- `CMake/GameBitsWgslBindings.cmake` renumbers a shader's bindings (see
  [Shaders](#shaders)). `GameBitsShaderBindings.cmake` would hide that it
  works on WGSL text.
- `tools/install-naga.sh`, beside `tools/install-vulkan-sdk.sh` and
  `tools/install-emsdk.sh`.
- render-check's `RenderCheckBackend`, in `render_check_backend.h`, is what
  render-check needs of a backend beyond `RenderBackend`.
  `vulkan_render_check_backend.cc` and `webgpu_render_check_backend.cc`
  implement it, named for the backend as the libraries are.
  `BackendSupport` says less, and `CheckedBackend` reads as the backend under
  check.
- gb_imgui's shaders move behind `imgui_shaders.h`, with the pasted SPIR-V in
  `gen_imgui_shaders.cc` (every platform but the web) and WGSL in
  `web_imgui_shaders.cc`. These are platform files in the usual sense: the
  build picks one.
- A depth target's *sampled copy* is the `r32float` texture that shaders
  sample it through (see [Depth targets](#depth-targets)).

### Shaders

A web build of `gb_embed_shader` runs four steps, all in its one custom
command, so a project's CMake doesn't change:

```
glslc -Werror -MD -MF <out>.d -o <out>.spv <in>
spirv-opt --split-combined-image-sampler <out>.spv -o <out>.split.spv
naga --validate 0 <out>.split.spv <out>.raw.wgsl
cmake -DINPUT=<out>.raw.wgsl -DOUTPUT=<out>.bound.wgsl \
  -P GameBitsWgslBindings.cmake
naga <out>.bound.wgsl <out>.wgsl
```

and embeds `<out>.wgsl` with `gb_embed_data`, as the SPIR-V is embedded
elsewhere. The tools are found as `glslc` is: `spirv-opt` beside `glslc` in
the Vulkan SDK, and `naga` on the `PATH`. A web build without them fails to
configure where a shader is embedded, with a message naming the install
script.

**Binding numbers.** WGSL gives a texture and its sampler a binding each,
where GLSL's combined image sampler is one. A binding's WGSL number depends
only on its own GLSL number `n`: a sampler is at `2n + 1`, and everything else
(a texture, constants, or a storage buffer) at `2n`. Every shader of a
pipeline agrees, whatever else each declares, and the backend computes the
same numbers from a `Binding`'s index alone.

`spirv-opt --resolve-binding-conflicts` can't do this: it numbers by what each
shader declares, so `pass_mist.frag` (only binding 1) and `pass_water.frag`
(bindings 0 and 1) would put `world_depth` at different numbers. Instead,
`naga` converts without validation (`--validate 0`), which leaves a texture
and its sampler on one binding, `GameBitsWgslBindings.cmake` renumbers each
`@binding(n)` by the type of the `var` that follows it (`sampler` or
`sampler_comparison`, or anything else), and `naga` reads the result back,
validating it, and writes it out again (which, unlike validating alone,
prints nothing). The script is about 50 lines of CMake, so the conversion
needs no tool beyond those the design names.

What else the conversion does, checked on render-check's shaders:
- `naga` flips `gl_Position.y` (its default, `--keep-coordinate-space`
  unset), so a GLSL shader written for Vulkan's clip space draws the right way
  up. The frame's pixels are where Vulkan puts them, and so is
  `gl_FragCoord`.
- `gl_InstanceIndex` becomes `instance_index`, which includes the first
  instance, as in Vulkan.
- Entry points are `main`.

gb_imgui's two shaders are converted by the same steps from
`assets/shaders/imgui_shader.vert` and `.frag` (the sources of the pasted
SPIR-V), and pasted into `web_imgui_shaders.cc` as raw string literals, with a
comment giving the commands, so gb_imgui still needs no tools to build.

**Brittleness:** the renumbering reads `naga`'s WGSL text, so a `naga` that
writes bindings differently breaks it. `naga` is pinned, the second `naga`
validates the result, and a mismatch between a shader and the backend's
layout is a pipeline error in the first page run. A WGSL shader written by
hand for `CreateShaderCode()` must number its bindings the same way, which
`WebGpuBackend`'s class comment says.

### WebGpuBackend

```
class WebGpuBackend final : public RenderBackend {
 public:
  // OPTIONAL: The CSS selector of the canvas to draw into.
  static inline constexpr const char* kKeyCanvasSelector = "CanvasSelector";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(
      kConstraintCanvasSelector, kInOptional, std::string,
      kKeyCanvasSelector, "#canvas");
  // SCOPED: This backend instance.
  static GB_CONTEXT_CONSTRAINT(kConstraintBackend, kScoped, WebGpuBackend);
  using Contract =
      ContextContract<kConstraintCanvasSelector, kConstraintBackend>;

  // Creates the backend, waiting for the browser's adapter and device. Returns
  // null, having logged why, if there is no device with what gb_render needs.
  // Call it from main(), in a page linked with JSPI, on the browser's main
  // thread.
  static std::unique_ptr<WebGpuBackend> Create(Contract contract);

  // Bytes of GPU memory in the buffers and textures the backend holds.
  int64_t GetAllocatedBytes() const;

  // RenderBackend overrides...
};
```

- `gb_render_webgpu` is built only in a web build (`if (EMSCRIPTEN)` in
  `src/gb/render/CMakeLists.txt`), as `gb_render_vulkan` is built only where
  there is Vulkan. Its CMake adds `--use-port=emdawnwebgpu` to its compile and
  link options, as `PUBLIC`, so whatever links it gets the bindings. The port
  is downloaded from GitHub the first time, into the Emscripten SDK's cache.
- **Header:** Dawn's `webgpu_cpp.h`. Its handles release their object when
  destroyed and are reference counted, so nothing is released by hand, its
  enums and flags are typed, and its callbacks are lambdas. It is said not to
  be stable, but Emscripten pins it, so a change only arrives with a new
  Emscripten, as compile errors. The C header (`webgpu.h`) would need a
  `wgpu*Release()` for every object, by hand. The experiments below used
  `webgpu_cpp.h`.
- **No SDL part.** `VulkanBackend` needs `VulkanWindow` for a surface and for
  size changes. Here the surface comes from the canvas's selector
  (`EmscriptenSurfaceSourceCanvasHTMLSelector`), and the size from
  `emscripten_get_canvas_element_size()` as each frame begins, so there is
  nothing to adapt from SDL and no size callback (nor a message dispatcher to
  deliver it, which in render-check is a thread).
- **Creation:** an instance with `TimedWaitAny`; an adapter and a device,
  each requested with `CallbackMode::WaitAnyOnly` and waited for with
  `Instance::WaitAny()` and a timeout (`kWaitTimeoutSecs`, 10). The device
  requires `indirect-first-instance`, and takes `float32-filterable` where the
  adapter has it, and the adapter's `maxTextureArrayLayers`,
  `maxBufferSize`, and `maxStorageBufferBindingSize`, so `gb_render`'s limits
  hold where the adapter allows. Its uncaptured error callback logs each error
  as `LOG(ERROR)`, and its lost callback logs why the device was lost.
- **Waits** are `Instance::WaitAny()` with the timeout, which with JSPI
  returns to the browser until the future is ready. They are for the adapter,
  the device, a frame read back, and creating shader code and pipelines (see
  [Pipelines](#pipelines)). While one waits, the browser runs its handlers,
  SDL's included, which only queue events.
- **Threads:** WebGPU objects belong to the thread that made the device, the
  browser's main thread. The backend's own state is guarded by mutexes as
  `VulkanBackend`'s is, so its factory methods are thread-safe in the sense
  `RenderBackend` asks. In a web build with threads, a factory method called
  on any other thread logs an error and returns null, rather than touching
  WebGPU there. Nothing checks a build with threads (*Threads in the browser*
  in the backlog).
- **Destruction** releases every object, the device last. A page whose
  program returns with WebGPU objects still held never exits (see
  [To confirm](#to-confirm)).

**Brittleness:** a program must create the backend from `main()`'s thread in
a page linked with JSPI. Both are true of every page `gb_add_win_executable`
makes. A program that creates it elsewhere fails at once, in its first run.

### Resources and updates

WebGPU orders `Queue::WriteBuffer()` and `Queue::WriteTexture()` with the
submissions around them: a write made after frame N is submitted doesn't
change what frame N reads. So the Vulkan backend's per-frame copies of
`kPerFrame` resources, its host buffers, and its garbage collector all go:
- **Buffers** (`WebGpuRenderBuffer`, one class for vertices, indices,
  constants, and storage): one GPU buffer, and a CPU copy unless it is
  `kStaticWrite`. `Set()`, `SetRange()`, `Clear()`, and `Edit()` change the
  CPU copy and mark the range, and each frame that is ended writes the marked
  ranges with `WriteBuffer()`. A `kStaticWrite` buffer writes at once with
  `WriteBuffer()`, so its partial writes always succeed (Vulkan's can fail
  while the buffer is in use). Volatility only decides whether there is a CPU
  copy. Storage buffers have `Storage | Indirect | CopyDst` usage, so a
  storage buffer of `MeshDraw` can be drawn from.
- **Textures** (`WebGpuTexture`, `WebGpuTextureArray`, `WebGpuVolumeTexture`,
  sharing `WebGpuTexels`, as the Vulkan ones share `VulkanTexels`): one GPU
  texture, and a CPU copy unless `kStaticWrite`. Updates mark regions, and each
  frame that is ended writes them with `WriteTexture()`, then each mip level
  generated on the CPU with `GenerateMipmaps()` (`gb/render/mipmaps.h`), as
  Vulkan does. Formats: `kRgba8Srgb` is `rgba8unorm-srgb`, `kRgba8`
  `rgba8unorm`, `kRg8` `rg8unorm`, `kR8` `r8unorm`, and `kRgba4` is
  `rgba8unorm`, its texels expanded (each 4 bits `v` to `v * 17`) as they are
  written. A texture array's view is always `2d-array`, even with one layer.
- **Samplers**, cached by options as Vulkan's are: filter is linear or
  nearest for magnification and minification, and the mip filter linear with
  mipmaps; address modes `repeat`, `mirror-repeat`, and `clamp-to-edge`;
  `lodMaxClamp` from the tile size as Vulkan's `maxLod` is. Anisotropy is 16
  where every filter is linear (a filtered, mipmapped texture), and otherwise
  1, as WebGPU allows nothing else. `kClampBorder` fails (see
  [The gaps](#the-gaps)).
- **Release:** a resource's destructor calls `Destroy()` on its texture or
  buffer, which frees the GPU memory once work already submitted is done, so
  memory doesn't wait for JavaScript's garbage collector. Nothing is recorded
  outside `EndFrame()`, so no unsubmitted work can still use it.
- `GetAllocatedBytes()` adds and subtracts each buffer's and texture's size
  as it is created and destroyed, which render-check shows where Vulkan shows
  VMA's total.

**Brittleness:** `Destroy()` is safe only because every command is recorded
and submitted inside `EndFrame()`. A later change that records work elsewhere
(such as uploads on another thread) must keep a destroyed resource out of it.
The class comment says so.

### Bindings

Each `BindingSet` is a bind group (`kView` 0, `kMaterial` 1, `kInstance` 2),
within WebGPU's limit of 4. `WebGpuBindingLayout` makes a bind group layout
per set from the `Binding`s, numbered as in [Shaders](#shaders), visible in
the stages of the binding's `shader_types`:

| Binding | WGSL bindings | Layout entry |
|---|---|---|
| `kConstants` at `n` | `2n` | uniform buffer, dynamic offset |
| `kStorageBuffer` at `n` | `2n` | read-only storage buffer |
| `kTexture` at `n` | `2n`, `2n + 1` | `float` 2D texture, `filtering` sampler |
| `kTextureArray` at `n` | `2n`, `2n + 1` | `float` 2D array texture, `filtering` sampler |
| `kVolumeTexture` at `n` | `2n`, `2n + 1` | `float` 3D texture, `filtering` sampler |

- Every texture binding is `float` with a `filtering` sampler, which a
  nearest sampler also fits, so a layout never depends on what is bound. Every
  color format is filterable, and depth targets are sampled through their
  sampled copy (see [Depth targets](#depth-targets)).
- Constants work as in Vulkan's `VulkanBindingDataFactory`: binding data
  takes a slot in a group's uniform buffers, aligned to
  `minUniformBufferOffsetAlignment` (256), with as many slots as fit
  `maxUniformBufferBindingSize` (64 KiB), and is bound with a dynamic offset.
  Constants are written into their slot, so changing them never changes the
  bind group.
- `WebGpuBindingData` holds one bind group, made again at `EndFrame()` when a
  texture or storage buffer binding changed, or when the object behind one was
  replaced (a render target resized), which each resource marks with a
  version number, as Vulkan's descriptor sets track theirs. A null texture or
  storage buffer is bound to its `DefaultBindings` resource.
- Limits, each failing the layout or pipeline with an error: a binding index
  above 499 (WebGPU numbers bindings below `maxBindingsPerBindGroup`, 1000),
  and more than 8 constants bindings in a pipeline
  (`maxDynamicUniformBuffersPerPipelineLayout`).

**Brittleness:** the numbering is computed in two places, the CMake script
and `WebGpuBindingLayout`, which must agree. A unit test of the backend's
function (`GetWgslBinding()`) states it, and render-check's shaders exercise
every binding type through the script.

### Pipelines

`WebGpuRenderPipeline` keeps what `CreatePipeline()` was given and makes a
`wgpu::RenderPipeline` the first time it is drawn in each kind of pass, as
`VulkanRenderPipeline::Get(pass)` does, as WebGPU bakes into a pipeline the
formats it draws into and their sample count. The kind is the color format
(or none), whether there is depth and whether it is read-only, and the sample
count. A read-only depth pass needs its own pipeline, as WebGPU refuses one
that writes depth there, where Vulkan's backend already draws depth-writing
material types as test-only.
- From `MaterialConfig`: depth compare and write from `DepthMode` and
  `DepthTest`; cull mode; blend states as Vulkan's; color write mask from the
  color channels; depth bias (its constant rounded to an integer, as WebGPU's
  is). Front faces are `CW`: `gb_render`'s counter-clockwise faces are
  clockwise to WebGPU once `naga` flips y (see [To confirm](#to-confirm)).
- Vertex attributes from `VertexType`, packed as Vulkan's are, each
  `ShaderValue` to its `VertexFormat`. 3-channel 8 and 16-bit values fail.
- A pass without a color target has no color targets in its pipelines;
  WebGPU accepts a fragment shader that writes color there, as Vulkan does.
- Creating a pipeline, or shader code, is wrapped in a validation error scope,
  waited for, so a failure returns null with the browser's message instead of
  a later uncaptured error. This happens once per pipeline and kind of pass.
  Creating pipelines before their first draw is the backlog's *Pipelines
  created before their first draw*.

**Brittleness:** none beyond Vulkan's: what the pipeline needs of the pass is
looked up at draw time, not remembered by the caller.

### Frames

```
BeginFrame:  read the canvas's size; if it changed, reconfigure the canvas
             and make the frame's textures again; false if it is zero.
EndFrame:    write marked buffers, textures, and mesh pools; clear new render
             targets; make bind groups again where needed.
             Encode each pass, in order, then the frame:
               frame color (4 samples, sRGB) and depth (4 samples),
               resolved into the frame texture (1 sample, sRGB).
             Copy the frame texture into the canvas's current texture.
             (EndFrameAndReadImage: copy the frame texture to a buffer.)
             Submit.
             (EndFrameAndReadImage: map the buffer, wait, make the Image.)
```

- **Color.** The canvas is configured as `rgba8unorm` (every browser takes it;
  Windows prefers `bgra8unorm`, which costs Chrome a conversion it does
  anyway for presentation), with `RenderAttachment | CopyDst` usage. The
  frame is drawn into a multisampled `rgba8unorm-srgb` texture, resolved into
  the frame texture, also `rgba8unorm-srgb` (with `CopySrc` usage), which is
  copied into the canvas byte for byte (WebGPU copies between formats that
  differ only in sRGB).
  Shaders write linear color, which the sRGB targets encode, the clear color
  is given as Vulkan gives it (each channel over 255), and `kRgba8Srgb`
  textures are read decoded, so the bytes are Vulkan's. Resolving straight
  into an sRGB view of the canvas's texture does not work: on SwiftShader the
  canvas got the linear values. The copy costs one frame-sized copy a frame.
- **The canvas's texture** is taken in `EndFrame()`, just before it is copied
  into, and never kept: in Chromium, the canvas has a new, transparent
  texture after every return to the browser, whether it waited for an
  animation frame or only yielded with a `MessageChannel`. Every frame is drawn
  whole into the frame texture, so this changes nothing. A piloted run shows
  whichever frame was last copied when the browser draws the page.
- **Reading a frame** copies the frame texture (not the canvas's) to a
  buffer with `MapRead`, in the same submission, then waits for its map.
  Rows are 256-byte aligned, as copies need. Alpha is set opaque, as Vulkan's
  read back is.
- **Resize:** the canvas's size is read with
  `emscripten_get_canvas_element_size()` as each frame begins. A change
  reconfigures the canvas and makes the frame's textures again, and render
  targets that follow the frame resize then, as `RenderSystem` already does.
  `GetFrameDimensions()` is guarded by a mutex, as Vulkan's is.
- **Draws** are gathered by `Draw()` into a structure like
  `VulkanRenderState` (pass, view order, view, pipeline, material, buffers,
  instances, and draw lists' commands), and encoded in `EndFrame()`, in the
  same order. Scissors are set as Vulkan's are, and reset at each draw list's
  end.
- **Default bindings and the clear color** are kept as Vulkan keeps them.
- There are no frames in flight to wait for: `BeginFrame()` never blocks, and
  the browser paces frames (see *Programs in the browser*'s frame wait).

**Brittleness:** taking the canvas's texture anywhere but `EndFrame()` would
break in a piloted run, where the program yields between frames. It is only
used in one function, which says why.

### Depth targets

WebGPU can neither sample a `depth32float` texture through a `float` binding
(only `depth` or `unfilterable-float`, which take a different sampler and
WGSL type than `naga` writes for GLSL's `sampler2D`), nor resolve depth. So
each depth target is two textures:
- the depth texture, `depth32float`, that passes draw into, and
- its sampled copy, `r32float` (filterable with `float32-filterable`; where
  the device lacks it, `r16float`, which is filterable everywhere but holds
  about 3 decimal digits), which bind groups bind for it.

The sampled copy is written by a pass of its own, after each pass that writes
the depth target: a full-frame triangle whose fragment shader loads each
pixel's depth (`textureLoad`) and writes it to the copy. For a multisampled
pass, the same pass is the depth resolve: it loads the first sample of the
multisampled depth, as `RenderPassConfig` says a resolve does, and writes it
both to the depth target (`frag_depth`) and to its copy. The two WGSL shaders
are the backend's own, as strings in `webgpu_depth_copy.cc`.

`CanFilter(kDepth)` is true, so render-check's depth targets (created with
the default, filtered sampler options) work as they do on Vulkan.
- `DepthUse::kReadOnly`: the pass attaches the depth texture read-only (with
  a finite `depthClearValue`, which the browser requires even then), and
  samples its copy, which holds the same depth, so there is no hazard.
- A target sampled before the pass that draws it holds last frame's depth, as
  its copy is last frame's.
- Creating or resizing a depth target clears both textures.
- Cost: one full-frame pass for each pass that writes a depth target, each
  frame.

**Brittleness:** the copy must follow every write of the depth texture. Only
`EndFrame()`'s pass encoding writes it (passes and clears), in one function,
which adds the copy pass.

### Passes and render targets

- `WebGpuPass` keeps its config, and the textures it draws into: the
  targets' own, or for a multisampled pass, 4-sample color and depth textures
  of the targets' size, made again when they resize, which the pass resolves
  from (color with `resolveTarget`, depth with the resolve above).
- Load and store: color clears to the pass's clear color unless it keeps it;
  depth clears to 1, loads (`kKeep`), or is read-only (`kReadOnly`); a
  multisampled pass's multisampled textures are discarded after the resolve.
- Passes are encoded in the order `BeginFrame()` gave them, then the frame,
  all in one command encoder, so a later pass samples what an earlier one drew
  this frame.
- `WebGpuRenderTarget` is a `WebGpuTexture` without a CPU copy, created with
  `RenderAttachment | TextureBinding` usage, in its color format, or as a depth
  target as above. Its sampler is a normal one. A new or resized target is
  cleared at the next `EndFrame()` by a pass that only clears it.

### Mesh pools and indirect draws

- `WebGpuMeshPoolBuffers` holds a vertex and an index buffer (`uint16` or
  `uint32`). `WriteMesh()` copies the data into a pending list, written at the
  next `EndFrame()` with `WriteBuffer()`, so a write is never more than a frame
  late, as on Vulkan. `Grow()` makes larger buffers at the next `EndFrame()`
  and copies the old contents in on the GPU (`CopyBufferToBuffer`, so the
  buffers have `CopySrc` usage), then writes what is pending.
- Reuse tickets are the number of frames ended: space freed while frame `N` is
  being built can be written again once frame `N` is submitted, as later writes
  are ordered after it. `CanReuse(ticket)` is true once a frame has ended
  since. The backend registers and unregisters pools as Vulkan's does.
- `DrawCommand::kIndirect` is a `DrawIndexedIndirect()` per draw, each at the
  next 20-byte `MeshDraw` in the storage buffer. With
  `indirect-first-instance`, which the backend requires, each draw's first
  instance is its own, as `gl_InstanceIndex` needs.

### The gaps

What WebGPU lacks, and what the backend does:

| Gap | Backend | Needed by |
|---|---|---|
| Clamp to border | Fails to create, with an error | Nothing, once gb_imgui clamps to the edge |
| Wireframe (`kLine`) | Fails to create the pipeline | Nothing |
| `kRgba4` | Expanded to `rgba8unorm` for the GPU | render-check's texture formats |
| Many indirect draws in one call | One call each | Nothing (one call each draws the same) |
| Multisampling other than 1 or 4 | Always 4 | render-check's passes (Vulkan uses up to 8) |
| `indirect-first-instance` missing | Fails to create the backend | render-check's mesh pools |
| Depth sampled or resolved | A sampled copy, written by a pass | render-check's passes |
| 3-channel 8 and 16-bit vertex values | Fails to create the pipeline | Nothing |

**gb_imgui's border.** It creates its font atlas with `kClampBorder` and a
transparent border. Its texture coordinates are always within the atlas (each
glyph's rectangle, and the white texel, with the atlas's padding around them),
and it samples without filtering, so no sample ever reaches the border, and
`kClampEdge` draws exactly the same pixels. gb_imgui changes to `kClampEdge`
on every platform, which render-check's `gui.txt` frames on Linux show
unchanged, and `kClampBorder` then fails on the web, rather than drawing
something close to it. Emulating a border in the shader conversion (a branch
on the coordinates before each sample) needs the sampler's options when the
shader is built, which it doesn't have; that is the backlog's *Clamp to border
on the web*, for a program that needs it.

### render-check on the web

`RenderCheckBackend` (`render_check_backend.h`) is what render-check uses of
a backend beyond `RenderBackend`:

```
class RenderCheckBackend {
 public:
  // Creates the backend for the platform, drawing into the window.
  static std::unique_ptr<RenderCheckBackend> Create(SDL_Window* window);
  virtual ~RenderCheckBackend() = default;

  // Flags for SDL_CreateWindow(): SDL_WINDOW_VULKAN for Vulkan, none for
  // WebGPU.
  static uint32_t GetWindowFlags();

  // Passes the RenderBackend on, for RenderSystem to own. Called once.
  virtual std::unique_ptr<RenderBackend> TakeRenderBackend() = 0;

  // Initializes ImGui's SDL support for the backend.
  virtual void InitImGui(SDL_Window* window) = 0;

  // Called with every SDL event the game reads.
  virtual void OnEvent(const SDL_Event& event) = 0;

  virtual int64_t GetGpuBytes() = 0;
  virtual bool IsMultiDrawSupported() = 0;
  virtual void SetMultiDrawEnabled(bool enabled) = 0;
};
```

- `vulkan_render_check_backend.cc` holds what is Vulkan's today:
  `SdlVulkanWindow`, its message system and `ThreadMessageDispatcher` (which
  only carry window events to it, so `OnEvent()` sends them), `VulkanBackend`
  with validation on, `ImGui_ImplSDL2_InitForVulkan`, VMA's total, and
  multi-draw.
- `webgpu_render_check_backend.cc`: `WebGpuBackend`,
  `ImGui_ImplSDL2_InitForOther`, `GetAllocatedBytes()`, and no multi-draw.
  `OnEvent()` does nothing.
- `CMakeLists.txt` picks the file and the backend library with `if
  (EMSCRIPTEN)`, as Game Bits' modules pick platform files, and embeds the
  same shaders (as WGSL in a web build).
- The panel's "Vulkan" wording becomes "the render backend", and the GPU
  memory line reads "GPU memory (backend's count)" on the web. The shared code
  has no `#ifdef`.
- The scripts are unchanged. A frame's text that names a platform (the
  multi-draw line: "Indirect, a call per range") differs, as it does on a
  Vulkan device without multi-draw.

**Brittleness:** a new Vulkan-only call in render-check now has to go through
`RenderCheckBackend`, which the web build enforces by not compiling it.

### Tools and CI

- `tools/install-vulkan-sdk.sh` also extracts `bin/spirv-opt` (6.5 MB), so
  every machine with Game Bits' Vulkan SDK has it. CI's cache key already
  hashes the script, so CI picks it up.
- `tools/install-naga.sh <dir> <version>` runs `cargo install naga-cli
  --version <version> --locked --root <dir>` (about three minutes), and says
  to put `<dir>/bin` on the `PATH`. The version is `30.0.2`, set once in
  `cmake.yml` as `NAGA_VERSION` and named in CLAUDE.md.
- `.github/actions/naga` installs it, cached by version and the script's
  hash, as `.github/actions/vulkan-sdk` does. GitHub's Ubuntu runners have
  `cargo`.
- A `build-render-check-web` job, beside the Linux render-check job: the
  Vulkan SDK (for `glslc` and `spirv-opt`), `naga`, the Emscripten SDK
  (`.github/actions/emsdk`), and Playwright (`.github/actions/playwright`), then
  render-check built for the web in Release and each of its scripts run with
  `tools/run-program.sh`, uploading `out/run/render_check` as the
  `render-check-web-frames` artifact.
- The cloud environment (`game-bits`) needs `naga` and the new `spirv-opt`
  installed when it starts, which the owner approves. Until then, a session
  installs both itself, as above.
- CLAUDE.md's Web section says how to build render-check for the web and run
  its scripts, and Cloud sessions lists the new job.

### To confirm

Checked in experiments outside the repository: render-check's 14 shaders and
gb_imgui's 2, converted as in [Shaders](#shaders) with `glslc` and
`spirv-opt` from the Vulkan SDK 1.4.363.1 and `naga` 30.0.2, drawn by one page
built with Emscripten 7.0.1 (`-sJSPI --use-port=emdawnwebgpu`, with
`webgpu_cpp.h`) and run in headless Chromium 158 with WebGPU on SwiftShader
(Playwright, under `xvfb-run`, with the flags `run-page.js` uses). The page
drew, with layouts and bind groups made by hand as the backend will make
them: a world pass (4 samples, depth) of a turned quad (`pass.vert`,
`quad.frag`); its depth resolved by the shader above into a depth target and
its `r32float` copy; a mist pass (`pass_mist.frag`) tested against that depth
read-only while sampling the copy; and the frame (4 samples), with the world
composited (`pass_composite.frag`), the unset bindings quad (`unset.frag`:
texture, array, volume, and storage at WGSL bindings 0 to 6), an sRGB quad, a
storage buffer read in a vertex shader, and an indirect draw with a first
instance (`pool.vert`, `storage.frag`). It read each back and checked pixels.
- The binding numbering works through real pipelines: every binding type, in
  sets 0 to 2, and `world_depth` at the same numbers in `pass_mist.frag` and
  `pass_water.frag`. All 16 shaders converted and validated. (CL1, CL4, CL5)
- A storage buffer read in a vertex shader drew each instance from it. (CL5)
- An indirect draw with `firstInstance` 2 drew with the third color.
  SwiftShader has `indirect-first-instance` and `float32-filterable`, but not
  `multi-draw-indirect`. (CL6)
- Front faces: with `CCW`, every quad was culled; with `CW`, all drew, the
  right way up, turned the way Vulkan turns them. (CL4)
- Colors: with the frame resolved into an `rgba8unorm-srgb` texture and copied
  into an `rgba8unorm` canvas, the clear color (32, 32, 48) read back as
  (99, 99, 120), and a shader's (0.5, 0.25, 0) as (188, 137, 0): the sRGB
  encoding Vulkan's swap chain gives. Resolving into an sRGB view of the
  canvas's texture gave (32, 32, 48) and (128, 64, 0), the encoding lost.
  Drawing into that view without multisampling did encode. (CL3)
- Depth: the world's quad kept the mist off it, and the mist's alpha followed
  the sampled depth; the edges in the world's color were smooth. A
  `depth32float` texture in a `float` texture binding is refused. A pipeline
  that writes depth is refused in a read-only depth pass. A read-only depth
  attachment with `depthClearValue` unset (NaN) is a page error. (CL7)
- Reading back: every frame's canvas texture was new and transparent black,
  after a `MessageChannel` yield as after an animation frame, whether or not
  the frame before it was read. A frame's copy, submitted before the map
  wait, read back what was drawn. (CL3)
- A page whose program returned while holding the device, queue, surface,
  adapter, and instance never exited (no `onExit`); releasing them first, it
  exited with its code. (CL3)
- Validation error scopes, waited for with `WaitAny()`, report a bad shader
  and a bad pipeline at creation. A depth-only pipeline whose shader writes
  color is valid; a color pipeline whose shader writes no color is valid only
  with a write mask of none (a material with no color channels). Binding
  number 1000 is refused. (CL4, CL7)
- SwiftShader's limits: 4 bind groups, 8 dynamic uniform buffers per pipeline
  layout, 10 storage buffers and 16 sampled textures and samplers per stage,
  256-byte uniform and storage offset alignment, 64 KiB uniform bindings,
  1 GiB storage bindings and buffers, 8192 2D and 2048 3D textures, 256 array
  layers, 1000 bindings per group. A 257-layer array is refused. (CL5)
- The canvas's preferred format on SwiftShader is `rgba8unorm`. (CL3)

Settled by CL1, with Game Bits' own CMake and tools:
- `gb_embed_shader` in a web build converted render-check's 14 shaders, and
  Chromium's `createShaderModule()` accepted each with no errors in
  `getCompilationInfo()`. Each was byte for byte the WGSL the experiments
  above drew with.
- `glslc -MT` names the WGSL as the depfile's target, so a shader is
  converted again when a file it `#include`s changes, as it is compiled again
  in a native build.
- A native build's shaders are unchanged: the same `glslc` command, and the
  same SPIR-V bytes, as before CL1.
- `tools/install-naga.sh` and `tools/install-vulkan-sdk.sh` install `naga`
  and `spirv-opt` in an empty folder.

Settled by CL2:
- gb_imgui's font atlas clamped to its edge draws what clamping to a
  transparent border drew. render-check's seven scripts, run on Linux with
  each, saved 34 frames that are byte for byte the same, apart from the
  process memory figure in 16 of them, which differs as much between two runs
  of the same build.
- The SPIR-V pasted into gb_imgui and `glslc`'s output for
  `assets/shaders/imgui_shader.*` today differ only in a generator version, a
  struct's name, and the order of decorations, so the WGSL converted from the
  GLSL is the same shader. naga converts `texture()` of an array's layer by
  truncating it to an integer, where GLSL rounds it, which gb_imgui's whole
  layer numbers don't notice.

Not checked:
- A real GPU: Chrome on Windows (whose canvas prefers `bgra8unorm`) with the
  canvas configured as `rgba8unorm`, and whether its sRGB resolve differs
  from SwiftShader's. The copy is right either way. (The owner, by hand, after
  CL8.)
- `kRgba4` expanded, mip levels written with `WriteTexture()`, texture arrays
  beyond 256 layers where an adapter allows them. (CL5)
- `Destroy()` freeing memory promptly, so render-check's GPU bytes stay level
  in `instances.txt`. (CL3, CL8)
- The canvas resized by `SDL_SetWindowSize()` and the surface reconfigured.
  (CL3)
- Linking the bindings into a test program run under Node, for
  `gb_render_webgpu_test`. If it doesn't link, the pure functions' tests move
  into a library without the port. (CL3)
- How long render-check's scripts take on SwiftShader, against
  `run-program.sh`'s timeout. (CL8)
- Installing `naga` in CI, and its cache. (CL9)
- A web build with threads, which nothing checks.

## CLs

Each CL that draws is checked by a scratch page in the session's scratchpad
until render-check runs (CL8): a `Game` like `templates/sdl` with a
`RenderSystem` on `WebGpuBackend`, linked against the CL's build, drawing
render-check's pieces with its shaders, run with `tools/run-program.sh` and a
`--steps` script that saves frames, which the session reads. The CL's
verification comment says what the page drew and what the frames showed, and
compares them with the same pieces in render-check's Linux frames.

CL1 and CL3 can run at the same time, and CL2 any time after CL1. CL5 and CL6
each depend on CL4, and can run at the same time; CL7 follows CL5, whose
textures render targets are. CL8 needs every one before it, and CL9 needs
CL8.

### CL1 [x] build: shaders converted to WGSL in a web build

Depends on: *Web platform* CL1.

- `gb_embed_shader` (`CMake/GameBitsTargetCommands.cmake`): in a web build,
  the steps in [Shaders](#shaders), finding `spirv-opt` and `naga`, with a
  message naming the install scripts when one is missing. Its comment says
  what a web build embeds, and how bindings are numbered.
- `CMake/GameBitsWgslBindings.cmake`, as in [Shaders](#shaders).
- `tools/install-vulkan-sdk.sh` extracts `spirv-opt`; `tools/install-naga.sh`.
- CLAUDE.md: installing `naga`, and what `gb_embed_shader` makes in a web
  build. The `gb_embed_shader` line under Build system says so.
- Unused in a web build of Game Bits itself, which embeds no shaders.

**Verify**
- Standard checks (the CMake change touches every platform's configure).
- A ctest (`gb_wgsl_bindings_test`, registered on every platform, as it
  needs only CMake, running `cmake -P` on WGSL fixtures and comparing the
  output) covers a texture, an array, a volume, constants, a storage buffer,
  several sets, and a `var` split over two lines as `naga` writes it.
- A scratch CMake project embedding render-check's 14 shaders in a web build
  builds, and each WGSL is accepted by Chromium's `createShaderModule()` with
  no errors in `getCompilationInfo()`, checked with a Playwright snippet.
- `tools/install-naga.sh` and the new `install-vulkan-sdk.sh` run in a clean
  folder in a cloud session.

### CL2 [x] imgui: font atlas clamped to its edge, and WGSL shaders

Depends on: CL1.

- `imgui_instance.cc`: the font atlas uses `kClampEdge` (see
  [The gaps](#the-gaps)). A cross-platform change, which draws the same pixels.
- `imgui_shaders.h`, `gen_imgui_shaders.cc` (the pasted SPIR-V, moved),
  `web_imgui_shaders.cc` (WGSL converted from `assets/shaders/imgui_shader.*`
  as in [Shaders](#shaders), with the commands in a comment), picked in
  `src/gb/imgui/CMakeLists.txt`.

**Verify**
- Standard checks. `gb_imgui_test` passes on Linux, Windows (CI), and under
  Node.
- Linux: render-check's `gui.txt`, run before and after, saves frames that
  are byte for byte the same (`cmp`).
- The WGSL is what CL1's steps give for the GLSL now (the CL says the
  commands), and Chromium accepts it, as in CL1.

### CL3 [ ] render: the WebGPU backend clears a frame

Depends on: *Programs in the browser* CL1 and CL4, *Web platform* CL6.

- `src/gb/render/webgpu`: `gb_render_webgpu`, built only in a web build.
  `WebGpuBackend` with creation, destruction, the frame (color, canvas, copy,
  resize), `EndFrameAndReadImage()`, the clear color, and
  `GetAllocatedBytes()`, as in [WebGpuBackend](#webgpubackend) and
  [Frames](#frames).
- `WebGpuRenderBuffer` for every volatility, and textures, texture arrays,
  and volume textures created and written (without mipmaps, and failing
  `kRgba4` and `kClampBorder` for now), as in
  [Resources and updates](#resources-and-updates), enough for `RenderSystem`
  to make its default bindings. The other factory methods log that they
  aren't supported yet and return null.
- `webgpu_format.h`/`.cc` with the backend's pure functions (formats,
  `GetWgslBinding()`, `kRgba4` expansion), and `webgpu_format_test.cc`, run
  under Node.
- CLAUDE.md's Web section: what the backend is, and that it needs a page.

**Verify**
- Standard checks; Windows and Linux are unchanged (the library isn't built).
- Web: `gb_render_webgpu_test` passes under Node.
- A scratch page with a `RenderSystem` on `WebGpuBackend` and clear color
  (32, 32, 48), run with `--steps 'wait 2; save clear; resize 640 360; wait 2;
  save resized'`: both frames are (99, 99, 120) everywhere, the second 640 by
  360, and the run exits with code 0.
- The same page creating and destroying a hundred 64 KiB buffers and
  textures a frame for 300 frames: `GetAllocatedBytes()` returns to where it
  started, with no WebGPU errors. Whether the browser frees the memory at once
  isn't visible to the page; CL8's `instances.txt` run is the check that it
  doesn't grow.

### CL4 [ ] render: shaders, bindings, and pipelines draw quads

Depends on: CL1, CL3.

- `WebGpuShaderCode`, `WebGpuBindingLayout`, `WebGpuBindingData` and its
  factory (constants in slots, bind groups made again as bindings change),
  `WebGpuView`, and `WebGpuRenderPipeline` (pipelines per kind of pass,
  error scopes), as in [Bindings](#bindings) and [Pipelines](#pipelines).
- `Draw()` of meshes, gathered and encoded in the frame as in
  [Frames](#frames). Draw lists' commands, passes, and render targets still
  fail (CL6, CL7).
- `WebGpuBackend`'s class comment: how a hand-written WGSL shader numbers its
  bindings.

**Verify**
- Standard checks; `gb_render_webgpu_test` covers `GetWgslBinding()` and
  vertex formats, including the 3-channel ones that fail.
- A scratch page drawing render-check's quads section (its `quad.vert` and
  `quad.frag` and the same constants, 10 quads with per-instance constants,
  changed each frame): its saved frames show the quads where render-check's
  Linux `instances.txt` frames show them, the same colors, and none culled.
- A material with `RasterMode::kLine`, and a shader with an error, each fail
  to create with an error and no uncaptured WebGPU error.

### CL5 [ ] render: textures, samplers, and storage buffers

Depends on: CL4.

- Texture binding and sampling for textures, texture arrays, and volume
  textures; samplers; mip levels; `kRgba4`; `Get()` and `Edit()`; storage
  buffers bound, in either stage; default bindings for unset ones;
  `kClampBorder` failing, as in [Resources and updates](#resources-and-updates)
  and [The gaps](#the-gaps).

**Verify**
- Standard checks; `gb_render_webgpu_test` covers `kRgba4` expansion.
- A scratch page drawing render-check's textures, texture arrays, blend
  modes, texture formats, volume textures, storage buffers, and unset
  bindings sections, with their shaders: its frames match the same sections
  in render-check's Linux frames (`textures.txt`, `texture_arrays.txt`).
- A texture array of 257 layers fails with an error naming the limit.

### CL6 [ ] render: draw lists, mesh pools, and indirect draws

Depends on: CL4.

- `Draw()` of draw lists (pipelines, buffers, scissors, resets),
  `WebGpuMeshPoolBuffers` (writes, growth, reuse tickets), and indirect draws,
  as in [Mesh pools and indirect draws](#mesh-pools-and-indirect-draws).

**Verify**
- Standard checks.
- A scratch page drawing render-check's mesh pools section with
  `pool.vert` and `storage.frag`, replacing a shape each frame: its frames
  show both rows matching, as in render-check's Linux `mesh_pools.txt`
  frames, while the pool grows.
- The same page with an ImGui window (`ImGuiInstance`), clipped by a scissor:
  the window draws as render-check's does on Linux (the font atlas, colors,
  and clipping).

### CL7 [ ] render: passes, render targets, and depth

Depends on: CL5.

- `WebGpuPass`, `WebGpuRenderTarget` (color and depth, with sampled copies),
  multisampled passes and the depth resolve, read-only depth, and pipelines
  for each kind of pass, as in [Depth targets](#depth-targets) and
  [Passes and render targets](#passes-and-render-targets).

**Verify**
- Standard checks.
- A scratch page drawing render-check's passes section (world, mist, water,
  depth views, depth-only pass) with its shaders, toggling multisampling and
  resizing, as `passes.txt` does: its frames match render-check's Linux
  `passes.txt` frames, apart from the shade of the multisampled edges.

### CL8 [ ] examples: render-check in the browser

Depends on: CL2, CL6, CL7, *Programs in the browser* CL5.

- `RenderCheckBackend` and its two files, as in
  [render-check on the web](#render-check-on-the-web); `render_check.cc`
  and `main.cc` lose their Vulkan names.
- render-check's `CMakeLists.txt` picks the file and backend library.
- README.md: building render-check for the web, and opening it.
- CLAUDE.md's Web section: running render-check's scripts in a page.

**Verify**
- Standard checks. Linux: render-check's seven scripts pass, and their frames
  are byte for byte as before this CL. Windows: `build-render-check-windows`
  in a `workflow_dispatch` run.
- Web: render-check builds in Release, and each of its seven scripts passes
  with `tools/run-program.sh`, within the default timeout. Every saved frame
  is compared with the Linux frame of the same name: the same things in the
  same places and colors, differing only in multisampled edges, the
  multi-draw line, and the memory figures. The verification comment lists any
  other difference and why.
- Web: `instances.txt`'s GPU bytes stay level as it recreates resources.

### CL9 [ ] CI: render-check in a headless browser

Depends on: CL8, *Programs in the browser* CL6.

- `.github/actions/naga`, `NAGA_VERSION` in `cmake.yml`, and the
  `build-render-check-web` job, as in [Tools and CI](#tools-and-ci).
- CLAUDE.md's Cloud sessions lists the job.

**Verify**
- A `workflow_dispatch` run of the CMake workflow on the PR's branch passes,
  with the new job's `render-check-web-frames` artifact holding every
  script's frames, and a second run takes `naga` from the cache.
