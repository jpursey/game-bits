// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_RENDER_PIPELINE_H_
#define GB_RENDER_RENDER_PIPELINE_H_

#include "gb/render/binding.h"

namespace gb {

// This class defines a a prebuilt render program that is used during
// rendering. This is the API-specific implementation of a MaterialType
// resource.
//
// This is an internal class called by other render classes to access the
// underlying graphics API and GPU.
//
// Derived classes should assume that all method arguments are already valid. No
// additional checking is required, outside of limits that are specific to the
// derived class implementation or underlying graphics API or GPU.
//
// This class and all derived classes must be thread-safe.
class RenderPipeline {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  RenderPipeline(const RenderPipeline&) = delete;
  RenderPipeline(RenderPipeline&&) = delete;
  RenderPipeline& operator=(const RenderPipeline&) = delete;
  RenderPipeline& operator=(RenderPipeline&&) = delete;
  virtual ~RenderPipeline() = default;

  //----------------------------------------------------------------------------
  // Derived class interface
  //----------------------------------------------------------------------------

  // Creates binding data for the specified binding set.
  //
  // The data created here allows the game to change data for this pipeline. It
  // is passed back in RenderBackend::Draw.
  //
  // The binding set will be either kMaterial or kInstance.
  virtual std::unique_ptr<BindingData> CreateMaterialBindingData() = 0;
  virtual std::unique_ptr<BindingData> CreateInstanceBindingData() = 0;

  // Validates binding data for the specified binding set.
  virtual bool ValidateInstanceBindingData(BindingData* binding_data) = 0;

 protected:
  RenderPipeline() = default;
};

}  // namespace gb

#endif  // GB_RENDER_RENDER_PIPELINE_H_
