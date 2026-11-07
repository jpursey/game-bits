// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/render/render_target.h"

#include <utility>

#include "absl/log/log.h"

namespace gb {

RenderTarget::RenderTarget(ResourceEntry entry, TextureFormat format,
                           const RenderTargetSize& size, int width, int height,
                           const SamplerOptions& options)
    : Texture(std::move(entry), DataVolatility::kStaticWrite, width, height,
              options, format),
      size_(size) {}

void RenderTarget::Resize(RenderInternal, int width, int height) {
  if (width == GetWidth() && height == GetHeight()) {
    return;
  }
  if (!DoResize(width, height)) {
    LOG(ERROR) << "Failed to resize render target from " << GetWidth() << "x"
               << GetHeight() << " to " << width << "x" << height;
    return;
  }
  SetSize(width, height);
}

bool RenderTarget::DoClear(int x, int y, int width, int height,
                           const void* texel) {
  LOG(ERROR) << "Render targets cannot be cleared by the CPU";
  return false;
}

bool RenderTarget::DoSet(int x, int y, int width, int height,
                         const void* texels, int stride) {
  LOG(ERROR) << "Render targets cannot be written by the CPU";
  return false;
}

bool RenderTarget::DoGet(void* out_texels) { return false; }

void* RenderTarget::DoEditBegin() { return nullptr; }

void RenderTarget::OnEditEnd(bool modified) {}

}  // namespace gb
