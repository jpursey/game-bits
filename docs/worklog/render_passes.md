# Render passes and targets

A program can draw in more than one pass. Render targets are textures that
passes draw into, and passes draw views into targets before the frame (the
window) is drawn. A later pass, or the frame, samples an earlier pass's targets
like any other texture. This is what effects that read what was already drawn
(refraction and reflections on water, fog by depth, soft particles) or draw
from somewhere else (shadow maps, mirrors, maps) need.

Scenes were renamed for what they are first. A `RenderScene` was a view: the
constants a group of draws share (such as a camera) and their order. A
`RenderSceneType` was a binding layout: the bindings every material type,
shader, and material built on it share. The rename broke every program that
draws, for the better, so Terra Diem and OZ-3 each need a follow-up item. Beyond
it, existing code is unchanged: a view created without a pass is drawn in the
frame, as scenes were.

Terra Diem's *Water refraction and reflections* is the first user, and
render-check's passes section draws as it would:
- **The world pass**: the opaque world into a color and a depth target that
  follow the frame, multisampled as the frame is, cleared to the sky color.
- **The frame**: a view that draws the world's color over the whole frame,
  writing the world's depth (`gl_FragDepth`, with `DepthMode::kWrite`), so what
  follows depth tests against the world. Then a water view, sampling the
  world's color (offset for refraction) and depth (for color by depth), then
  the debug and UI views.
- Its render test can check, through the test backend, that the water reads
  the world pass's targets, and the world pass reads none.

Shadow maps are depth-only passes.

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

- Everything else named for scenes followed: parameters, members, the test and
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
  static RenderTargetSize Pixels(int width, int height);  // Each positive.
  static RenderTargetSize Frame(float scale = 1.0f);  // In (0, 1], rounded.
  bool follows_frame = true;
  int width = 0;       // Pixels() only.
  int height = 0;      // Pixels() only.
  float scale = 1.0f;  // Frame() only.
};

// A texture that passes draw into, sampled in later passes and the frame as
// any other texture: bind it with BindingData::SetTexture().
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

- A target's contents start cleared (transparent black, or the far plane for
  depth), and are cleared again whenever it is resized. They can't be read or
  written from the CPU: it has `kStaticWrite` volatility, and `Clear()`,
  `Set()`, `Edit()`, `Get()`, and saving it all fail.
- `GetWidth()` and `GetHeight()` are its current size. A target that follows
  the frame changes size only in `RenderSystem::BeginFrame()`.
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
  bool multisample = false;
};

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

- A pass draws the views created in it into its targets, each frame, before
  the frame. Passes are drawn in pass order, and those with the same order in
  the order they were created.
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
  frame keeps its multisampling. A multisampled pass draws with as many
  samples as the frame, and resolves its color by averaging each pixel's
  samples, and its depth by taking the first sample. It clears its targets:
  keeping either fails.
- A pass, or the frame, samples the targets of earlier passes as drawn this
  frame. A target sampled before the pass that draws it holds last frame's
  contents. A pass must not sample its own targets: Vulkan's validation
  layers (on in Debug) log that as an error.
- A pass must outlive the views created in it, and keeps its targets alive
  while it exists. It must be destroyed before the `RenderSystem` that
  created it, and not between the `BeginFrame()` that draws it and
  `EndFrame()`. Each is a `CHECK` failure.

## Design

### Names

- **RenderView** and **BindingLayout**, over *DrawLayer* and *DrawGroup*, and
  *RenderViewType*. "View" is the term most engines use for this (bgfx,
  Unreal, Filament, Bevy). It differs from Game Bits' other views
  (`ImageView`, `MeshView`, `RenderBufferView`, `BufferView`), which are
  windows onto data. A binding layout is the part of each material type's
  pipeline layout (in Vulkan's terms) that every pipeline built on it shares.
  The name fits `Binding`, `BindingSet`, and `BindingData`.
- **RenderTarget**, over *TargetTexture* and *DrawTarget*: the common term
  across graphics APIs.
- **RenderPass**, over *DrawPass* and *RenderStage* (taken by
  `VulkanBackend::RenderStage`). It shares its name with `vk::RenderPass`,
  which Vulkan code always writes qualified.
- **RenderPassConfig**, as `MaterialConfig` is for a material type, and
  **RenderTargetSize**, beside `FrameDimensions`.
- "The frame" keeps meaning the window's image (`BeginFrame()`,
  `FrameDimensions`), and the frame is not a `RenderPass`.

### gb_render: render targets

- `RenderTarget` derives from `Texture`, so it binds anywhere a texture does
  (bindings, material defaults, gb_imgui images) with nothing new in binding
  data. It implements Texture's `Do` virtuals as failures (`final`), and adds a
  protected virtual `DoResize(width, height)` for the backend, which returns
  false if it can't. It is a resource type of its own, as `MeshPool` is, so
  `ResourcePtr<RenderTarget>` converts to `ResourcePtr<Texture>`.
- `Texture`'s width and height aren't `const`, with a private setter that only
  `RenderTarget` reaches, through `RenderTarget::Resize()`.
- The backend creates it (`RenderBackend::CreateRenderTarget()`), after
  `RenderSystem` validates the format, size, and options, and computes the
  size of one that follows the frame.
- `RenderSystem` keeps the targets that follow the frame, in a set under a
  mutex. A target leaves the set in `RenderSystem`'s release handler, under
  the same mutex, before it is deleted, so `BeginFrame()` never resizes one
  being destroyed on another thread. `BeginFrame()` resizes each to the
  frame's size, which does nothing for one already that size.

### gb_render: passes

- The backend creates a pass (`RenderBackend::CreatePass()`), as it does
  views, after `RenderSystem` validates the config. It holds `ResourcePtr`s to
  its targets.
- `RenderSystem` keeps the passes in draw order, under a mutex. It adds each
  pass as it creates it, and the pass removes itself as it is destroyed,
  through a pointer it holds.
- `RenderBackend::CreateView()` takes the pass (null for the frame), and a
  pass counts its views.
- `RenderBackend::BeginFrame()` is given the frame's passes in draw order,
  valid until `EndFrame()`, which draws each, then the frame. Passing them to
  `BeginFrame()`, rather than a separate `SetPasses()`, leaves no call order to
  get right. `BeginFrame()` marks each pass as in the frame, under the passes'
  mutex, and `EndFrame()` (or a failed `BeginFrame()`) clears it, so a pass
  destroyed during a frame on any thread is caught. In Debug, drawing a view
  whose pass isn't in the frame is a `RENDER_ASSERT` failure.
- `TestRenderBackend` records each frame's passes in draw order, as
  `State::passes`, each with its pass (null for the frame), the draws and
  draw commands drawn in it, and the textures it reads: every texture bound in
  the view, material, and instance data of its draws.

**Brittleness:** the targets that follow the frame are added and removed by
`RenderSystem` itself. A pass must outlive its views, the `RenderSystem` must
outlive its passes, and a pass must not be destroyed during a frame it is
drawn in, each enforced by a `CHECK`. A pass sampling its own targets is left
to the validation layers, as finding it in `RenderSystem` would mean looking
through every draw's bindings.

### gb_render_vulkan

- **Vulkan 1.2.** Passes are created with `vkCreateRenderPass2`, for the depth
  resolve, which are core in Vulkan 1.2. The backend requires a 1.2 device,
  logging each device it skips, rather than keeping a second render pass path
  for older ones. 1.2 guarantees resolving depth from sample zero.
- **Pipelines per pass layout.** A `vk::Pipeline` can only draw in render
  passes compatible with the one it was created for: the same attachment
  formats and sample counts. `VulkanRenderPipeline` keeps one per
  `VulkanPassLayout` (the color and depth formats, either of which may be
  none, and the sample count), creating each the first time it is drawn in a
  pass of that layout. It is given a `VulkanRenderPassInfo`, a render pass and
  its layout, put together only where the render pass is created, so they
  can't disagree. A pipeline that fails for a layout draws nothing in it, and
  the failure is logged once. The frame's is created with the material type,
  so a material type that can't draw fails to be created.
- **Targets.** `VulkanRenderTarget` holds a `VulkanImage` usable as an
  attachment and sampled, with a version that changes as it is resized, so
  binding data rewrites its descriptors as it does for a texture that is
  replaced (`VulkanTexels::ImageHandle`). A resized image goes to the garbage
  collector. A new image is cleared, into `eShaderReadOnlyOptimal`, at the
  start of the first frame that draws into it or samples it, and stays in
  that layout outside passes. `VulkanBindingData` tells a target from a
  texture by `IsRenderTarget()`.
- **Depth format.** Depth targets need a format that is both a depth
  attachment and sampled, and has no stencil, so a view of its depth is the
  whole attachment: `D32_SFLOAT` first, then `X8_D24_UNORM_PACK32`. On a
  device with neither, creating a depth target fails.
- **Passes.** `VulkanPass` holds a render pass (clearing or loading each
  attachment as configured) and a framebuffer, recreated when a target's
  version changes. Each attachment's final layout is
  `eShaderReadOnlyOptimal`, and the render pass's external dependencies order
  its writes after earlier reads and writes of the targets, and before later
  shader reads. All frames are drawn on one queue, so one image per target is
  enough: frames in flight are ordered by the same dependencies.
- **Multisampled passes.** The pass draws into multisampled color and depth
  images of its own, its targets' size, recreated with its framebuffer, and
  resolves them into the targets: color with a resolve attachment, and depth
  with a depth resolve (sample zero). Multisampled color is transient and
  never stored. Multisampled depth is stored, though nothing reads it after
  its resolve: without that, NVIDIA's driver (610.88, RTX 4070 Ti SUPER) left
  tile-shaped garbage in the resolved depth after resizing to some sizes, such
  as 1085x399. Synchronization, memory placement, the resolve mode, the
  target's load op and layouts, the transient flag, and the depth format were
  all ruled out. On a device that can't multisample, a multisampled pass
  draws with one sample, as the frame does.
- **Drawing.** `VulkanRenderState::draw` is kept per pass, and
  `EndFrameRenderPass()` draws each pass in order, then the frame, setting the
  viewport and scissor (and `DrawCommand::kReset`'s scissor) to the pass's
  size.
- **The frame's render pass** waits for the last frame's depth tests and
  attachment writes before clearing its attachments, which the validation
  layers' synchronization checks found missing.

**Brittleness:** none for callers. Within the backend, a target's version is
the only signal that its framebuffers and descriptors are stale, and
`VulkanPass::Init()` and `GetFramebuffer()` must agree on the attachment
order (each multisampled image, then each target it resolves into).

### Performance

- Each pass is a render pass of its own, with its barriers, which costs
  little next to what it draws.
- The first draw of a material type in a pass of a new layout creates a
  pipeline, a stall of a few milliseconds, once.
- A multisampled pass costs what the frame's multisampling does, plus its
  resolves, and storing its multisampled depth: at 8x and 1080p, about 66 MB
  written per frame.

### Confirmed on a device

On an NVIDIA GeForce RTX 4070 Ti SUPER (Vulkan 1.4, driver 610.88):
- `D32_SFLOAT` is a depth attachment, sampled, and filtered.
- Depth resolves from sample zero (also average, min, and max).
- A fragment shader writing color in a depth-only pass is a validation warning
  (`Undefined-Value-ShaderOutputNotConsumed`), once per pipeline. A pass
  sampling its own targets is a validation error
  (`VUID-VkDescriptorImageInfo-imageLayout-00344`), each frame.
- A pass that keeps its depth target draws without validation errors.

### Building blocks

- Render targets in any pass or the frame, and depth-only passes for shadow
  maps.
- `TestRenderBackend::State::passes`, for a program's tests to check what each
  pass draws and reads.
- `VulkanPassLayout` and `VulkanRenderPassInfo`, for anything else in the
  backend that draws in more than one kind of render pass.
- render-check's `resize_window` pilot command, until the pilot has a step of
  its own.

### Left out

Each is in the backlog: *Multisampled targets that are kept*, *Several color
targets per pass*, *Shadow map comparison samplers*, *Floating point render
targets*, *Read-only depth attachments*, *Pipelines created before their first
draw*, and *Multisampled depth without storing it*.

## Structure

- `gb/render/binding_layout.h` / `.cc`, `render_view.h`: the renamed scene
  types.
- `gb/render/render_target.h` / `.cc`: `RenderTargetSize` and `RenderTarget`.
- `gb/render/render_pass.h` / `.cc`: `RenderPassConfig` and `RenderPass`.
- `gb/render/render_system.h` / `.cc`: `CreateRenderTarget()`, `CreatePass()`,
  `CreateView()` in a pass, resizing targets and handing the frame's passes to
  the backend in `BeginFrame()`.
- `gb/render/test_render_target.h` / `.cc`, `test_render_pass.h`, and
  `test_render_backend.h` / `.cc`: the test backend's targets, passes, and
  `State::passes`.
- `gb/render/vulkan/vulkan_render_target.h` / `.cc`: `VulkanRenderTarget`.
- `gb/render/vulkan/vulkan_pass.h` / `.cc`: `VulkanPass`, multisampling
  included.
- `gb/render/vulkan/vulkan_render_pipeline.h` / `.cc`: pipelines per layout.
- `gb/render/vulkan/vulkan_backend.h` / `.cc`: creating targets and passes,
  clearing new targets, and drawing each pass then the frame.
- `examples/render-check`: the passes section (`shaders/pass*.vert/.frag`),
  and `scripts/passes.txt`.

## Checks

- `gb_render_test`:
  - `render_target_test.cc`: each format, size, and sampler options, valid and
    not; sizes that follow the frame, scaled and rounded, resized only when the
    frame changes; writes, reads, and saving failing; binding a target as a
    texture.
  - `render_pass_test.cc`: configs valid and not (no target, wrong formats,
    mismatched sizes, keeping in a multisampled pass); pass order and ties;
    views in passes and in the frame; each frame's passes, their draws, and
    what each reads; a pass with nothing drawn still recorded; destroying a
    pass with views left, or during a frame.
- `examples/render-check`, `scripts/passes.txt`, shown over the grid (S):
  turning, overlapping quads from a multisampled world pass over a blue sky;
  the frame draws its color and depth, then water through the middle that
  ripples and tints what is behind it and hides behind the quads in front of
  it. Beside it, the world's depth, and a depth-only pass's depth of the same
  quads (a material type with no color channels and an empty fragment
  shader), both as gray. It is saved with and without multisampling (quad
  edges in the color are smooth only with it), and in a 1085x399 window. No
  validation messages, and no synchronization hazards for the passes with
  `VK_KHRONOS_VALIDATION_VALIDATE_SYNC=true`.
- Every other render-check script, and block-world's, run as before.
- Checked once by hand-editing render-check: a pass that keeps its depth
  target, and a pass sampling its own targets (see Confirmed on a device).
