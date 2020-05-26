#include "gbits/test/test_util.h"

#include <cstdlib>

namespace gb {

std::string GenerateTestString(int64_t size) {
  // True randomness is unimportant here, but we make sure that no two adjacent
  // characters are equal to better detect off-by-one errors.
  std::string text(size, ' ' + static_cast<char>(std::rand() % 94));
  for (std::string::size_type i = 1; i < text.size(); ++i) {
    text[i] = ' ' + static_cast<char>(std::rand() % 93);
    if (text[i] == text[i - 1]) {
      ++text[i];
    }
  }
  return text;
}

std::string GenerateAlphaTestString(int64_t size) {
  // True randomness is unimportant here, but we make sure that no two adjacent
  // characters are equal to better detect off-by-one errors.
  std::string text(size, 'a' + static_cast<char>(std::rand() % 26));
  for (std::string::size_type i = 1; i < text.size(); ++i) {
    text[i] = 'a' + static_cast<char>(std::rand() % 25);
    if (text[i] == text[i - 1]) {
      ++text[i];
    }
  }
  return text;
}

}  // namespace gb
