#include "gbits/resource/resource_entry.h"

#include "gbits/resource/resource_system.h"

namespace gb {

void ResourceEntry::Free() { system_->RemoveResource({}, type_, id_); }

std::string_view ResourceEntry::GetName() const {
  return system_->GetResourceName({}, type_, id_);
}

}  // namespace gb
