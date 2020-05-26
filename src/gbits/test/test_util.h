#ifndef GBITS_TEST_TEST_UTIL_H_
#define GBITS_TEST_TEST_UTIL_H_

#include <string>

namespace gb {

// Generates a pseudo-random ASCII string of the specified size which further
// guarantees that no two adjacent characters in the string are the same (to
// help detect off-by-one errors).
std::string GenerateTestString(int64_t size);

// Generates a pseudo-random ASCII string of the specified size containing only
// lowercase characters. It further guarantees that no two adjacent characters
// in the string are the same (to help detect off-by-one errors).
std::string GenerateAlphaTestString(int64_t size);

}  // namespace gb

#endif  // GBITS_TEST_TEST_UTIL_H_
