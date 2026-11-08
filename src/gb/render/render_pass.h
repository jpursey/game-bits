// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_PASS_H_
#define GB_RENDER_RENDER_PASS_H_

#include <atomic>

#include "gb/image/pixel.h"
#include "gb/render/render_target.h"
#include "gb/render/render_types.h"
#include "gb/resource/resource_ptr.h"

namespace gb {

// RenderSystem includes this header, and RenderPass removes itself from the
// RenderSystem that created it.
class RenderSystem;

//==============================================================================
// RenderPassConfig
//==============================================================================

// How a pass draws: into which targets, and whether it clears them first.
//
// A pass has a color target, a depth target, or both, of the same
// RenderTargetSize. Its color target is in a color format, and its depth target
// is kDepth (see RenderTarget).
struct RenderPassConfig {
  RenderPassConfig& SetColor(RenderTarget* target, bool keep = false) {
    color = target;
    keep_color = keep;
    return *this;
  }
  RenderPassConfig& SetDepth(RenderTarget* target, bool keep = false) {
    depth = target;
    keep_depth = keep;
    return *this;
  }
  RenderPassConfig& SetClearColor(Pixel color) {
    clear_color = color;
    return *this;
  }
  RenderPassConfig& SetMultisample(bool in_multisample) {
    multisample = in_multisample;
    return *this;
  }

  RenderTarget* color = nullptr;  // Null for a depth-only pass.
  RenderTarget* depth = nullptr;  // Null for a pass without depth.
  bool keep_color = false;        // False clears to clear_color.
  bool keep_depth = false;        // False clears to the far plane.
  Pixel clear_color = {0, 0, 0, 0};

  // Draws with as many samples as the frame, resolved into the targets as the
  // pass ends. A multisampled pass clears its targets: keeping either fails.
  bool multisample = false;
};

//==============================================================================
// RenderPass
//==============================================================================

// A pass draws the views created in it into its targets, each frame, before the
// frame (the window) is drawn. Passes are drawn in pass order, and those with
// the same order in the order they were created. A pass is drawn every frame,
// whether or not anything is drawn in it, so a target it clears is cleared.
//
// A pass, or the frame, samples the targets of earlier passes as drawn this
// frame. A target sampled before the pass that draws it holds last frame's
// contents. A pass must not sample its own targets.
//
// A pass must outlive the views created in it: destroying one with views left
// is a CHECK failure. It keeps its targets alive while it exists. It must be
// destroyed before the RenderSystem that created it, and not between the
// RenderSystem::BeginFrame() that draws it and EndFrame(): each is a CHECK
// failure too.
//
// This class and all derived classes must be thread-compatible.
class RenderPass {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderPass(const RenderPass&) = delete;
  RenderPass(RenderPass&&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;
  RenderPass& operator=(RenderPass&&) = delete;
  virtual ~RenderPass();

  //----------------------------------------------------------------------------
  // Properties
  //----------------------------------------------------------------------------

  // Returns the pass order, which is the order passes are drawn in.
  int GetOrder() const { return order_; }

  // Returns the targets the pass draws into, either of which may be null.
  RenderTarget* GetColorTarget() const { return color_.Get(); }
  RenderTarget* GetDepthTarget() const { return depth_.Get(); }

  // Returns true if the pass keeps what its targets held, rather than clearing
  // them before it draws.
  bool KeepsColor() const { return keep_color_; }
  bool KeepsDepth() const { return keep_depth_; }

  // Returns true if the pass draws with as many samples as the frame.
  bool IsMultisampled() const { return multisample_; }

  // The color the pass clears its color target to, unless it keeps it. Setting
  // it takes effect the next time the pass is drawn, in EndFrame().
  Pixel GetClearColor() const { return clear_color_; }
  void SetClearColor(Pixel color) { clear_color_ = color; }

 protected:
  // The config is valid (see RenderSystem::CreatePass).
  RenderPass(const RenderPassConfig& config, int pass_order);

 private:
  friend class RenderSystem;
  friend class RenderView;

  // Called by RenderView as each view in the pass is created and destroyed.
  void AddView() { view_count_.fetch_add(1, std::memory_order_relaxed); }
  void RemoveView() { view_count_.fetch_sub(1, std::memory_order_relaxed); }

  const int order_;
  const ResourcePtr<RenderTarget> color_;
  const ResourcePtr<RenderTarget> depth_;
  const bool keep_color_;
  const bool keep_depth_;
  const bool multisample_;
  Pixel clear_color_;

  // Set by RenderSystem once the backend creates the pass.
  RenderSystem* system_ = nullptr;

  // True from the RenderSystem::BeginFrame() that the pass is drawn in to its
  // EndFrame(). Set by RenderSystem on the rendering thread, under the lock it
  // holds as a pass is removed.
  bool in_frame_ = false;

  // Views are created on any thread.
  std::atomic<int> view_count_ = 0;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_PASS_H_
