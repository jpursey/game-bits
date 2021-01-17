// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_ACCESS_TOKEN_H_
#define GB_BASE_ACCESS_TOKEN_H_

// This header defines a macro for creating package-level access tokens.
//
// This works by defining a token class with a predefined set of friends which
// are allowed to construct the token. This token class can then be passed in as
// a parameter to any function (by convention, the first parameter), making it
// only callable by code that either already has an access token or is in the
// access token's friend list.
//
// Example:
//
// GB_BEGIN_ACCESS_TOKEN(ModuleInternal)
// friend class Foo;
// friend void GlobalFunc();
// GB_END_ACCESS_TOKEN()
//
// class Bar {
//  public:
//   // Only callable by Foo, and GlobalFunc.
//   void FuncA(ModuleInternal, int x, int y) { ... }
//   void FuncB(ModuleInternal internal) {
//     // Ok, because it can copy the token. This is an internal chain of calls.
//     FuncA(internal, 1, 2);
//     // Compile Error: B cannot mint its own token!
//     FuncA({}, 1, 2);
//   }
// };
//
// class Foo {
//  public:
//   Foo() {
//     Bar bar;
//     // Ok, because class Foo can construct the ModuleInternal token.
//     bar.FuncB({});
//   }
// };
//
// void GlobalFunc() {
//   Bar bar;
//   // Also ok, because GlobalFunc can construct the ModuleInternal token.
//   bar.FuncA({}, 100, 200);
// }

#define GB_BEGIN_ACCESS_TOKEN(Name)         \
  class Name {                              \
   public:                                  \
    Name(const Name&) = default;            \
    Name(Name&&) = default;                 \
    Name& operator=(const Name&) = default; \
    Name& operator=(Name&&) = default;      \
                                            \
   private:                                 \
    Name() = default;                       \
    gb::AccessToken token_;

#define GB_END_ACCESS_TOKEN() \
  }                           \
  ;

namespace gb {
// This class is required so the named access token defined above is not
// trivially constructible. Without this workaround, any class can construct the
// access token with just {} (at least in VS 2017).
class AccessToken {};
}  // namespace gb

#endif  // GB_BASE_ACCESS_TOKEN_H_
