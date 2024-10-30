// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/parser_rules.h"

#include "absl/log/log.h"
#include "gb/parse/parser.h"

namespace gb {

ParseMatch ParserToken::Match(ParserInternal internal, Parser& parser) const {
  return parser.MatchTokenItem(internal, *this);
}

ParseMatch ParserRuleName::Match(ParserInternal internal,
                                 Parser& parser) const {
  return parser.MatchRuleItem(internal, *this);
}

ParseMatch ParserGroup::Match(ParserInternal internal, Parser& parser) const {
  switch (GetType()) {
    case Type::kSequence:
      return parser.MatchSequence(internal, *this);
    case Type::kAlternatives:
      return parser.MatchAlternatives(internal, *this);
  }
  LOG(DFATAL) << "Unimplemented group type";
  return ParseMatch::Abort(ParseError("Unimplemented group type"));
}

}  // namespace gb