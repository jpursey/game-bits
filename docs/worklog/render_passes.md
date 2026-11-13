# Render passes and targets

Everything is drawn today in one pass, to the window, with scenes in
`scene_order`. Effects that read what was already drawn (refraction and
reflections on water, fog by depth, soft particles) or draw from somewhere
else (shadow maps, mirrors, maps) need more than one. A program can now create
render targets, which are textures that passes draw into, and passes, which
draw views into targets before the frame is drawn. A later pass, or the frame,
samples an earlier pass's targets like any other texture.

First, scenes are renamed for what they are. A `RenderScene` is a view: the
constants a group of draws share (such as a camera) and their order. A
`RenderSceneType` is a binding layout: the bindings every material type,
shader, and material built on it share. This breaks every program that draws,
for the better, while Terra Diem is the only one that does much.

Beyond the rename, existing code is unchanged: views created as scenes are
today are drawn in the frame, as they are now. `RenderBackend` gains pure
virtuals, which no project implements, and `TextureFormat` gains `kDepth`.

## What Terra Diem needs first

Terra Diem's *Water refraction and reflections* is the first user. It draws
one world scene today (opaque, water, and transparent faces), with the sky as
the clear color (changing with the time of day), and a debug scene over it,
into the frame with multisampling. With this feature, it would draw:
- **The world pass**: the opaque world into a color and a depth target that
  follow the frame, multisampled as the frame is, cleared to the sky color.
- **The frame**: a view that draws the world's color over the whole frame,
  writing the world's depth (`gl_FragDepth`, with `DepthMode::kWrite`), so what
  follows depth tests against the world. Then a water view, sampling the
  world's color (offset by moving normals, for refraction and reflections in
  screen space) and depth (for color by depth, and foam at the shore), with its
  depth pre-pass as today. Then the debug and UI views.
- Its render test checks, through the test backend, that the water reads the
  world pass's targets, and the world pass reads none.

Every piece of that is in the CLs below: multisampled passes (CL5) included,
as without them the world would lose its antialiasing as soon as it moved into
a target. render-check's passes section draws the same way, so it is checked
before Terra Diem starts. Shadow maps, which are optional for Terra Diem, are
depth-only passes, which CL4 covers too.

## Behavior

### Views and binding layouts

| Was | Is |
| --- | --- |
| `RenderScene` | `RenderView` |
| `RenderSceneType` | `BindingLayout` |
| `RenderSystem::RegisterSceneType()`, `GetSceneType()` | `RegisterBindingLayout()`, `GetBindingLayout()` |
| `RenderSystem::CreateScene(scene_type, scene_order)` | `CreateView(binding_layout, view_order)` |
| `RenderScene::GetType()`, `GetOrder()`, `GetSceneBindingData()` | `RenderView::GetBindingLayout()`, `GetOrder()`, `GetViewBindingData()` |
| `RenderSceneType::GetDefaultSceneBindingData()` | `BindingLayout::GetDefaultViewBindingData()` |
| `MaterialType::GetSceneType()` | `GetBindingLayout()` |
| `BindingSet::kScene` | `BindingSet::kView` |

- Everything else named for scenes follows: parameters, members, the test and
  Vulkan classes (`TestRenderView`, `VulkanView`, `VulkanBindingLayout`), and
  files (`render_view.h`, `binding_layout.h`).
- Saved material types and binding data load as before: they hold the binding
  layout's registered name, and `BindingSet`'s values are unchanged.
- `RenderView`'s comments say that it is a view of draws, not a window onto
  data as `ImageView` and `MeshView` are.

### Render targets

```
// The size of a render target: fixed in pixels, or a scale of the frame's size
// that follows the window as it resizes.
struct RenderTargetSize {
  // Exactly width x height pixels, each positive.
  static RenderTargetSize Pixels(int width, int height);

  // The frame's size times scale (in (0, 1]), rounded, and at least one pixel.
  static RenderTargetSize Frame(float scale = 1.0f);

  bool operator==(const RenderTargetSize&) const = default;

  bool follows_frame = true;
  int width = 0;       // Pixels() only.
  int height = 0;      // Pixels() only.
  float scale = 1.0f;  // Frame() only.
};

// A texture that passes draw into (see RenderPass), sampled in later passes
// and the frame as any other texture: bind it with BindingData::SetTexture().
//
// Its contents start cleared (transparent black, or the far plane for depth),
// and are cleared again whenever it is resized. They can't be read or written
// from the CPU: it has kStaticWrite volatility, and Clear(), Set(), Edit(),
// Get(), and saving it all fail.
//
// GetWidth() and GetHeight() are its current size. A target that follows the
// frame changes size only in RenderSystem::BeginFrame().
class RenderTarget : public Texture {
 public:
  const RenderTargetSize& GetSize() const;
};

// RenderSystem
ResourcePtr<RenderTarget> CreateRenderTarget(
    TextureFormat format, const RenderTargetSize& size,
    const SamplerOptions& options = kRenderTargetSamplerOptions);
RenderTarget* CreateRenderTarget(ResourceSet* resource_set, ...);
```

- `TextureFormat::kDepth` is depth, at the precision the backend has (at
  least 24 bits), which a shader samples as one channel in [0, 1]. Only render
  targets have it: creating any other texture in it fails. A depth target is
  `kDepth`.
- A color target is `kRgba8Srgb` (as the frame is), `kRgba8`, `kRg8`, or
  `kR8`, the formats every device can draw into. `kRgba4` fails.
- The sampler options default to filtering, clamped to the edge, without
  mipmaps. Mipmaps fail, and filtering a format `CanFilter()` rejects fails as
  it does for textures.
- `Texture::IsRenderTarget()` says whether a texture is one.

### Passes

```
// How a pass draws: into which targets, and whether it clears them first.
struct RenderPassConfig {
  RenderPassConfig& SetColor(RenderTarget* target, bool keep = false);
  RenderPassConfig& SetDepth(RenderTarget* target, bool keep = false);
  RenderPassConfig& SetClearColor(Pixel color);
  RenderPassConfig& SetMultisample(bool multisample);

  RenderTarget* color = nullptr;  // Null for a depth-only pass.
  RenderTarget* depth = nullptr;  // Null for a pass without depth.
  bool keep_color = false;        // False clears to clear_color.
  bool keep_depth = false;        // False clears to the far plane.
  Pixel clear_color = {0, 0, 0, 0};

  // Draws with as many samples as the frame, resolved into the targets as the
  // pass ends. A multisampled pass clears its targets: keeping either fails.
  bool multisample = false;
};

// A pass draws the views created in it into its targets, each frame, before
// the frame (the window) is drawn. Passes are drawn in pass order, and those
// with the same order in the order they were created.
//
// A pass must outlive its views, and the targets it draws into are kept alive
// while it exists. It can't be destroyed between BeginFrame() and EndFrame().
class RenderPass {
 public:
  int GetOrder() const;
  RenderTarget* GetColorTarget() const;
  RenderTarget* GetDepthTarget() const;
  bool KeepsColor() const;
  bool KeepsDepth() const;
  bool IsMultisampled() const;
  Pixel GetClearColor() const;
  void SetClearColor(Pixel color);  // From the next frame drawn.
};

// RenderSystem
std::unique_ptr<RenderPass> CreatePass(const RenderPassConfig& config,
                                       int pass_order = 0);
std::unique_ptr<RenderView> CreateView(RenderPass* pass,
                                       BindingLayout* binding_layout,
                                       int view_order = 0);

// RenderView
RenderPass* GetPass() const;  // Null for a view drawn in the frame.
```

- A pass has a color target, a depth target, or both, of the same
  `RenderTargetSize`. Its viewport and scissor cover its targets.
- A pass runs every frame, whether or not anything is drawn in it, so a target
  it clears is cleared. One created during a frame runs from the next, and its
  views can't be drawn before then.
- Each view carries its own view binding data, so a pass with its own camera
  is a view of its own, drawing the same meshes and materials as another.
- Any material type draws in any pass. Its color channels, blend mode, and
  depth mode apply to the pass's targets: a depth-only pass writes no color,
  and a pass without depth does no depth test.
- A depth-only pass is best drawn with material types that write no color: no
  color channels, and a fragment shader that writes none (an empty `main()`,
  or one that only discards), as engines draw shadow maps with a depth-only
  variant of each material. One whose shader writes color draws correctly,
  but does work the driver may not remove, and Vulkan's validation layers
  warn about it as each pipeline is created. They warn too about a shader
  that writes no color to channels that are written, which the frame's
  pipeline, created with the material type, would be without the channels
  off.
- A pass draws without multisampling unless it is multisampled, while the
  frame keeps its multisampling. A multisampled pass resolves its color by
  averaging each pixel's samples, and its depth by taking the first sample.
- A pass, or the frame, samples the targets of earlier passes as drawn this
  frame. A target sampled before the pass that draws it holds last frame's
  contents. A pass must not sample its own targets: Vulkan's validation
  layers (on in Debug) log that as an error.

## Design

### Names

- **RenderView** and **BindingLayout**, chosen with the user over
  *DrawLayer* and *DrawGroup*, and *RenderViewType*. "View" is the term most
  engines use for this (bgfx, Unreal, Filament, Bevy). It differs from Game
  Bits' other views (`ImageView`, `MeshView`, `RenderBufferView`,
  `BufferView`), which are windows onto data; `RenderView`'s comments say so.
  A binding layout is the part of each material type's pipeline layout (in
  Vulkan's terms) that every pipeline built on it shares; only its view set
  becomes a descriptor set layout of its own. The name fits `Binding`,
  `BindingSet`, and `BindingData`.
- **RenderTarget** (recommended), *TargetTexture*, *DrawTarget*. No existing
  name is close. "Render target" is the common term across graphics APIs.
- **RenderPass** (recommended), *DrawPass*, *RenderStage*. *RenderStage* is
  taken by `VulkanBackend::RenderStage`. *RenderPass* shares its name with
  `vk::RenderPass`, which Vulkan code always writes qualified;
  `VulkanBackend::GetRenderPass()`, which nothing calls, is removed.
- **RenderPassConfig**, as `MaterialConfig` is for a material type.
- **RenderTargetSize**, beside `FrameDimensions`, which is the frame's size in
  pixels.
- "The frame" keeps meaning the window's image (`BeginFrame()`,
  `FrameDimensions`), and the frame is not a `RenderPass`.

### gb_render: render targets

- `RenderTarget` derives from `Texture`, so it binds anywhere a texture does
  (bindings, material defaults, gb_imgui images) with nothing new in binding
  data. It implements Texture's `Do` virtuals as failures (`final`), and adds a
  protected virtual `DoResize(width, height)` for the backend, which returns
  false if it can't. It is a resource type of its own, as `MeshPool` is,
  registered with the resource system, so `ResourcePtr<RenderTarget>` converts
  to `ResourcePtr<Texture>`.
- `Texture`'s width and height lose their `const`, with a private setter that
  only `RenderTarget` reaches, through `RenderTarget::Resize()`.
- The backend creates it: `RenderBackend::CreateRenderTarget(entry, format,
  size, width, height, options)`. `RenderSystem` validates the format, size,
  and options first, and computes the size of one that follows the frame.
- `RenderSystem` keeps the targets that follow the frame, in a set under a
  mutex (creation is thread-safe). A target leaves the set in `RenderSystem`'s
  release handler for render targets, under the same mutex, before it is
  deleted, so `BeginFrame()` never resizes one being destroyed on another
  thread. `BeginFrame()` resizes each to the frame's size, which does nothing
  for one already that size, so a target created at an older size catches up
  before it is drawn.
- `SaveTexture()` fails for a render target, saying why.
- `TestRenderTarget` records its resizes.

**Brittleness:** the targets that follow the frame are added and removed by
`RenderSystem` itself, never by a caller.

### gb_render: passes

- `RenderPass` is created by the backend (`RenderBackend::CreatePass(config,
  pass_order)`), as views are, after `RenderSystem` validates the config. It
  holds `ResourcePtr`s to its targets.
- `RenderSystem` keeps the passes in draw order, under a mutex. It adds each
  pass as it creates it, and the pass removes itself as it is destroyed,
  through a pointer it holds, as `MeshPoolBuffers` does with the Vulkan
  backend. Destroying the `RenderSystem` with passes left is a `CHECK` failure.
- `RenderBackend::CreateView()` takes the pass (null for the frame).
  `RenderView` holds it, and a pass counts its views: destroying a pass with
  views left is a `CHECK` failure. `RenderSystem::CreateView()` without a pass
  draws in the frame, as before.
- `RenderBackend::BeginFrame()` is given the frame's passes in draw order,
  valid until `EndFrame()`, which draws each, then the frame. `RenderSystem`
  copies them under the mutex as the frame begins. Passing them to
  `BeginFrame()`, rather than a separate `SetPasses()`, leaves no call order to
  get right. In Debug, drawing a view whose pass isn't in the frame is a
  `RENDER_ASSERT` failure, checked by the same flag that catches a pass
  destroyed during a frame (see Brittleness).
- `TestRenderBackend` records each frame's passes in draw order, as
  `State::passes`, each with its pass (null for the frame), the draws and
  draw commands drawn in it, and the textures it reads: every texture bound in
  the view, material, and instance data of its draws. Tests check the order
  and what each pass reads from that.

**Brittleness:** a pass must outlive its views, the `RenderSystem` must outlive
its passes, and a pass must not be destroyed during a frame it is drawn in,
each enforced by a `CHECK`. For the last, `BeginFrame()` marks each pass it
copies as in the frame, under the passes' mutex, and `EndFrame()` (or a failed
`BeginFrame()`) clears it, so a pass destroyed on any thread is caught. A pass
sampling its own targets is left to the validation layers, as finding it in
`RenderSystem` would mean looking through every draw's bindings.

### gb_render_vulkan

- **Pipelines per pass layout.** A `vk::Pipeline` can only draw in render
  passes compatible with the one it was created for: the same attachment
  formats and sample counts. `VulkanRenderPipeline` keeps one per layout
  (`VulkanPassLayout`: the color and depth formats, either of which may be
  none, and the sample count), creating each the first time it is drawn in a
  pass of that layout. It is given a `VulkanRenderPassInfo`, a render pass and
  its layout, which are put together only where the render pass is created,
  so they can't disagree. A pipeline that fails for a layout draws nothing in
  it, and the failure is logged once. The frame's is created with the material type, as now,
  so a material type that can't draw fails to be created. Its blend state has
  one attachment, or none for a depth-only pass, and it does no depth test in
  a pass without depth.
- **Targets.** `VulkanRenderTarget` holds a `VulkanImage` usable as an
  attachment and sampled, with a version that changes as it is resized, so
  binding data rewrites its descriptors as it does for a texture that is
  replaced (`VulkanTexels::ImageHandle`). A resized image goes to the garbage
  collector. A new or resized image is cleared, into
  `eShaderReadOnlyOptimal`, at the start of the first frame that draws into it
  or samples it. `VulkanBindingData` tells a target from a texture by
  `IsRenderTarget()`.
- **Depth format.** Depth targets need a format that is both a depth
  attachment and sampled, and has no stencil, so a view of its depth is the
  whole attachment: `D32_SFLOAT` first, as for the frame, then
  `X8_D24_UNORM_PACK32`. On a device with neither, creating a depth target
  fails.
- **Passes.** `VulkanPass` holds a `vk::RenderPass` (clearing or loading each
  attachment as configured) and a framebuffer, recreated when a target's
  version changes. Each attachment's final layout is
  `eShaderReadOnlyOptimal`, so every target is ready to sample after its pass,
  and the render pass's external dependencies order its attachment writes
  after earlier fragment shader reads, and before later ones. All frames are
  drawn on one queue, so one image per target is enough: frames in flight
  are ordered by the same dependencies.
- **Multisampled passes.** The pass holds multisampled color and depth
  images of its targets' size, recreated with them, and resolves them
  into its targets: color with a resolve attachment, as the frame does, and
  depth with a depth resolve (sample zero), which needs `vkCreateRenderPass2`.
  Both are core in Vulkan 1.2, which the backend now requires of its device,
  and which guarantees resolving depth from sample zero. Every pass is created
  with `vkCreateRenderPass2`, so passes have one path. The frame's render pass
  is unchanged. Multisampled color is never stored, so it is transient.
  Multisampled depth is stored, though nothing reads it after its resolve:
  without that, NVIDIA's driver (610.88, RTX 4070 Ti SUPER) leaves parts of
  the resolved depth undefined at some sizes (see CL5). Keeping a target would
  need its samples loaded too, which is why a multisampled pass can't. On a
  device that can't multisample, as the frame doesn't, a multisampled pass
  draws with one sample.
- **Drawing.** `VulkanRenderState::draw` is kept per pass, and
  `EndFrameRenderPass()` draws each pass in order, then the frame, setting the
  viewport and scissor (and `DrawCommand::kReset`'s scissor) to the pass's
  size.

**Brittleness:** none new for callers. Within the backend, a target's
version is the only signal that its framebuffers and descriptors are stale.

### Performance

- Each pass is a render pass of its own, with its barriers, which costs
  little next to what it draws.
- The first draw of a material type in a pass of a new layout creates a
  pipeline, a stall of a few milliseconds, once.
- A multisampled pass costs what the frame's multisampling does, plus its
  resolves, and storing its multisampled depth: at 8x and 1080p, about 66 MB
  written per frame.

### Left out

These would go to the backlog once the feature is done:
- Multisampled passes that keep their targets (storing the samples).
- More than one color target per pass.
- Comparison samplers (`sampler2DShadow`), for filtered shadow maps.
- Floating point color formats, for HDR.
- Testing against a depth target while sampling it (a read-only depth
  attachment), so water could test against the world's depth directly, rather
  than the frame's copy of it.
- Creating a pass's pipelines ahead of their first draw.

### To confirm

- Every device in use has a depth format that is both an attachment and
  sampled, and can filter it. CL4: the RTX 4070 Ti SUPER has `D32_SFLOAT`,
  and filters it.
- Validation layers accept a fragment shader writing color in a depth-only
  pass, and report a pass sampling its own target. CL4: writing color with no
  attachment is a warning (`Undefined-Value-ShaderOutputNotConsumed`), logged
  once as each pipeline is created. A pass sampling its own targets is an
  error (`VUID-VkDescriptorImageInfo-imageLayout-00344`), each frame.
- The devices in use support `VK_KHR_depth_stencil_resolve` with sample zero
  (or Vulkan 1.2), and whether to require it or let a multisampled pass with
  depth fail without it. CL5: the RTX 4070 Ti SUPER has Vulkan 1.4, and
  resolves depth from sample zero (also average, min, and max). The backend
  requires Vulkan 1.2, current on every desktop driver since 2020, rather
  than keeping a second render pass path for older devices.

## CLs

### CL0 [x] render, render_vulkan, imgui, examples: Views and binding layouts

Depends on: nothing.

- Renames scenes to views and scene types to binding layouts, as in the table
  above, across gb_render, gb_render_vulkan, gb_imgui, the examples, and the
  docs that name them.
- No change in behavior. Breaks every program that draws: Terra Diem and OZ-3
  each need a follow-up item.

**Verify**
- Standard checks (see CLAUDE.md).
- Every existing render test passes, renamed.
- Every render-check script runs, and block-world runs, as before.
- Nothing named for scenes is left (`git grep -i scene` finds only prose
  about scenes in the general sense).

### CL1 [x] render: Render targets

Depends on: CL0.

- `render_types.h`: `TextureFormat::kDepth`, rejected by every other texture.
  `render_resource.fbs` keeps its enum in step.
- `render_target.h/.cc`: `RenderTargetSize`, `RenderTarget`.
- `texture.h`: `IsRenderTarget()`, and size set only by `RenderTarget`.
- `render_system`: `CreateRenderTarget()`, and resizing in `BeginFrame()`.
- `render_backend.h`: `CreateRenderTarget()`.
- `test_render_target.h/.cc`, and the test backend creating them.
- `VulkanBackend::CreateRenderTarget()` logs that it isn't supported yet and
  returns null.

**Verify**
- Standard checks.
- Unit tests: creating each format, size, and sampler options, valid and not;
  sizes that follow the frame, scaled and rounded, resized only when the frame
  changes, and not when it doesn't; writes, reads, and saving failing; binding
  a target as a texture.

### CL2 [x] render: Passes

Depends on: CL1.

- `render_pass.h/.cc`: `RenderPassConfig` (multisampling included),
  `RenderPass`.
- `render_view.h`: `GetPass()`.
- `render_system`: `CreatePass()`, `CreateView()` in a pass, and the passes in
  draw order given to the backend.
- `render_backend.h`: `CreatePass()`, `CreateView()` taking the pass, and
  `BeginFrame()` taking the frame's passes.
- The test backend's passes, and `State::passes`.
- `VulkanBackend::CreatePass()` logs that it isn't supported yet and returns
  null.

**Verify**
- Standard checks.
- Unit tests: configs valid and not (no target, wrong formats, mismatched
  sizes, keeping in a multisampled pass); pass order and ties; views in passes
  and in the frame; each frame's passes, their draws, and what each reads; a
  pass with nothing drawn still recorded; destroying a pass with views left.

### CL3 [x] render_vulkan: Pipelines and drawing per pass

Depends on: CL2.

- `VulkanRenderPipeline` keeps a pipeline per layout, with the frame's as the
  only one used.
- `VulkanRenderState::draw` per pass, and `EndFrameRenderPass()` drawing each,
  with the frame the only one, and the scissor from its size.
- `VulkanBackend::GetRenderPass()` and `GetMsaaSampleCount()`, which nothing
  else calls, are removed.
- No visible change.

**Verify**
- Standard checks.
- Every render-check script runs, and its frames look as before.
- block-world runs, and looks as before.

### CL4 [x] render_vulkan: Targets and passes in Vulkan

Depends on: CL3.

- `VulkanRenderTarget`, `VulkanPass`, and their place in `EndFrame()`, without
  multisampling (a multisampled pass fails to be created until CL5).
- render-check: a passes section drawn as Terra Diem would draw its world and
  water. A pass draws turning, overlapping quads into a color and a depth
  target. The frame then draws the target's color over the section, writing
  its depth, then a "water" quad through the middle, depth tested against it,
  sampling the color offset by a moving wave and tinted by the depth below it.
  Beside it, the depth target is shown as gray. A depth-only pass draws the
  same quads into a fixed-size depth target, with a material type that writes
  no color (no color channels, and an empty fragment shader), shown as gray
  too.
  `scripts/passes.txt` saves it. The window has no room left, so the section
  is drawn over the grid while shown (the S key), and `gui.txt` collapses its
  header.

**Verify**
- Standard checks.
- render-check's `passes.txt`: the water distorts and tints what is behind
  it, and is hidden where a quad is in front of it; both depths show the quads
  as gray; no validation errors.
- Every other render-check script, unchanged apart from the panel's new
  header and the memory it reports.
- Checked once by hand-editing render-check: a pass that keeps its depth
  target, without validation errors, and a pass sampling its own targets,
  with them. The depth-only pass drawn with the world's color material, which
  draws the same, with one validation warning.
- By hand: resizing render-check's window resizes the targets that follow the
  frame, and the passes section keeps drawing.

### CL5 [x] render_vulkan: Multisampled passes

Depends on: CL4.

- Multisampled passes, resolving color and depth into their targets. Every
  pass is created with `vkCreateRenderPass2`, and the backend requires a
  Vulkan 1.2 device.
- render-check's passes section multisamples its world pass, with a checkbox
  to turn it off, which replaces the pass and its view before the frame
  begins. A `resize_window` pilot command resizes the window, so
  `passes.txt` checks the passes in a smaller one, until the pilot has a
  step for it (backlog: *Pilot window resizing and polled mouse*).
- The frame's render pass waits for the last frame's depth tests, and its
  writes, before clearing its attachments. Found by the validation layers'
  synchronization checks while looking into the depth resolve.

The depth resolve first left tile-shaped garbage in the world's depth after a
resize, changing each frame, at some sizes (1280x400, 1085x399, 1084x400) and
not others (1280x800, 1281x800, 1100x700). Ruled out: synchronization (a full
barrier after the pass, and the synchronization checks, found nothing),
memory placement (dedicated allocations), the resolve mode (min), the target's
load op and layouts, the transient flag, the depth format (D24), and the VMA
API version. With 4 samples, the broken part was one triangle of the target,
as if the driver resolves with a draw. Storing the multisampled depth fixed it
at every size.

**Verify**
- Standard checks.
- render-check's `passes.txt`, saving the section with and without
  multisampling: quad edges in the target are smooth only with it, and the
  water still hides behind quads in both. Then in a smaller window: the
  world's depth is still the quads'.
- No hazards for the passes from the validation layers' synchronization
  checks.
