// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RENDER_LOCAL_BINDING_DATA_H_
#define GB_RENDER_LOCAL_BINDING_DATA_H_

#include <stdint.h>

#include <vector>

#include "absl/types/span.h"
#include "gb/base/type_info.h"
#include "gb/resource/resource.h"
#include "gb/render/binding.h"
#include "gb/render/binding_data.h"

namespace gb {

// This class is a CPU-only implementation for BindingData used for specifying
// defaults.
//
// This class is thread-compatible.
class LocalBindingData : public BindingData {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ~LocalBindingData() override;

  //----------------------------------------------------------------------------
  // Operations
  //----------------------------------------------------------------------------

  // Copies over the binding data with the binding data in this local binding
  // data. "binding_data" must be for the same binding types or a strict
  // superset of this binding data. Anything else is undefined behavior, and
  // likely will result in a crash.
  void CopyTo(BindingData* binding_data) const;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  LocalBindingData(RenderInternal, BindingSet set,
                   absl::Span<const Binding> bindings);
  LocalBindingData(RenderInternal, const LocalBindingData& other);

 protected:
  bool Validate(int index, TypeKey* type) const override;
  void DoSet(int index, const void* value) override;
  void DoGet(int index, void* value) const override;
  void DoGetDependencies(
      ResourceDependencyList* dependencies) const override;

 private:
  static const RenderDataType* GetTextureDataType();
  static const RenderDataType* GetTextureArrayDataType();

  std::vector<std::tuple<const RenderDataType*, void*>> data_;
  size_t backing_buffer_size_ = 0;
  uint8_t* backing_buffer_ = nullptr;
};

}  // namespace gb

#endif  // GB_RENDER_LOCAL_BINDING_DATA_H_
