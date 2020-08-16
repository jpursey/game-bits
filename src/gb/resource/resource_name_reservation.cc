// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_name_reservation.h"

#include "gb/resource/resource_system.h"

namespace gb {

void ResourceNameReservation::Free() {
  system_->ReleaseResourceName({}, type_, id_, name_);
}

void ResourceNameReservation::Apply() {
  if (system_ == nullptr) {
    return;
  }
  system_->ApplyResourceName({}, type_, id_, name_);
  system_ = nullptr;
  type_ = nullptr;
  id_ = 0;
  name_.clear();
}

}  // namespace gb
