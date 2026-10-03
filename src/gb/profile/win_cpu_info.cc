// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <vector>

#include "absl/base/no_destructor.h"
#include "gb/profile/cpu_info.h"

// MUST be last, as windows pollutes the global namespace with many macros.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gb::internal {

namespace {

// Windows numbers processors within groups of up to 64.
constexpr int kGroupSize = 64;

// Returns the class of every processor, indexed by its group times kGroupSize
// plus its number in the group.
std::vector<int> ReadCoreClasses() {
  std::vector<int> classes(GetActiveProcessorGroupCount() * kGroupSize, 0);
  DWORD size = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
  std::vector<char> buffer(size);
  if (!GetLogicalProcessorInformationEx(
          RelationProcessorCore,
          reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
              buffer.data()),
          &size)) {
    return classes;
  }
  for (DWORD offset = 0; offset < size;) {
    const auto* info =
        reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(
            buffer.data() + offset);
    for (int i = 0; i < info->Processor.GroupCount; ++i) {
      const GROUP_AFFINITY& affinity = info->Processor.GroupMask[i];
      for (int bit = 0; bit < kGroupSize; ++bit) {
        const int index = affinity.Group * kGroupSize + bit;
        if ((affinity.Mask & (KAFFINITY{1} << bit)) != 0 &&
            index < static_cast<int>(classes.size())) {
          classes[index] = info->Processor.EfficiencyClass;
        }
      }
    }
    offset += info->Size;
  }
  return classes;
}

}  // namespace

int GetCoreClass() {
  static const absl::NoDestructor<std::vector<int>> s_classes(
      ReadCoreClasses());
  PROCESSOR_NUMBER processor;
  GetCurrentProcessorNumberEx(&processor);
  const int index = processor.Group * kGroupSize + processor.Number;
  return index < static_cast<int>(s_classes->size()) ? (*s_classes)[index] : 0;
}

}  // namespace gb::internal
