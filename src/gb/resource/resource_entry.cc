// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_entry.h"

#include "gb/resource/resource_system.h"

namespace gb {

void ResourceEntry::Free() { system_->RemoveResource({}, type_, id_); }

std::string_view ResourceEntry::GetName() const {
  return system_->GetResourceName({}, type_, id_);
}

}  // namespace gb
