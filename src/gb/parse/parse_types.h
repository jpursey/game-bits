// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/base/access_token.h"

namespace gb {

// Forward declarations
class Lexer;
class LexerContent;
class Token;

GB_BEGIN_ACCESS_TOKEN(LexerInternal)
friend class Lexer;
friend class LexterContent;
GB_END_ACCESS_TOKEN()

}  // namespace gb
