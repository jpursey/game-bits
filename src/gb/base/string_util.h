// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_STRING_UTIL_H_
#define GB_BASE_STRING_UTIL_H_

#include <string_view>

namespace gb {

// This type can be used in maps and sets to prevent unnecessary construction of
// std::string when looping up keys.
struct StringKeyCompare {
  using is_transparent = void;
  bool operator()(std::string_view a, std::string_view b) const {
    return a < b;
  }
};  

}  // namespace gb

#endif  // GB_BASE_STRING_UTIL_H_
