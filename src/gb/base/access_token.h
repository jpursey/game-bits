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
// GB_DEFINE_ACCESS_TOKEN(ModuleInternal, class Foo, void GlobalFunc());
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

#define GB_CALL(Name, Args) Name Args
#define GB_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, \
                   _14, _15, N, ...)                                       \
  N

#define GB_DEFINE_FRIEND_LIST1(Friend) friend Friend;
#define GB_DEFINE_FRIEND_LIST2(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST1, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST3(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST2, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST4(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST3, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST5(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST4, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST6(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST5, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST7(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST6, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST8(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST7, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST9(Friend, ...) \
  friend Friend;                            \
  GB_CALL(GB_DEFINE_FRIEND_LIST8, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST10(Friend, ...) \
  friend Friend;                             \
  GB_CALL(GB_DEFINE_FRIEND_LIST9, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST11(Friend, ...) \
  friend Friend;                             \
  GB_CALL(GB_DEFINE_FRIEND_LIST10, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST12(Friend, ...) \
  friend Friend;                             \
  GB_CALL(GB_DEFINE_FRIEND_LIST11, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST13(Friend, ...) \
  friend Friend;                             \
  GB_CALL(GB_DEFINE_FRIEND_LIST12, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST14(Friend, ...) \
  friend Friend;                             \
  GB_CALL(GB_DEFINE_FRIEND_LIST13, (__VA_ARGS__))
#define GB_DEFINE_FRIEND_LIST15(Friend, ...) \
  friend Friend;                             \
  GB_CALL(GB_DEFINE_FRIEND_LIST14, (__VA_ARGS__))

#define GB_DEFINE_FRIEND_LIST(...)                                           \
  GB_CALL(                                                                   \
      GB_CALL(GB_NTH_ARG, (__VA_ARGS__, GB_DEFINE_FRIEND_LIST15,             \
                           GB_DEFINE_FRIEND_LIST14, GB_DEFINE_FRIEND_LIST13, \
                           GB_DEFINE_FRIEND_LIST12, GB_DEFINE_FRIEND_LIST11, \
                           GB_DEFINE_FRIEND_LIST10, GB_DEFINE_FRIEND_LIST9,  \
                           GB_DEFINE_FRIEND_LIST8, GB_DEFINE_FRIEND_LIST7,   \
                           GB_DEFINE_FRIEND_LIST6, GB_DEFINE_FRIEND_LIST5,   \
                           GB_DEFINE_FRIEND_LIST4, GB_DEFINE_FRIEND_LIST3,   \
                           GB_DEFINE_FRIEND_LIST2, GB_DEFINE_FRIEND_LIST1)), \
      (__VA_ARGS__))

#define GB_DEFINE_ACCESS_TOKEN(Name, ...)         \
  class Name {                                    \
   public:                                        \
    Name(const Name&) = default;                  \
    Name(Name&&) = default;                       \
    Name& operator=(const Name&) = default;       \
    Name& operator=(Name&&) = default;            \
                                                  \
   private:                                       \
    Name() = default;                             \
    gb::AccessToken token_;                       \
    GB_CALL(GB_DEFINE_FRIEND_LIST, (__VA_ARGS__)) \
  }

namespace gb {
// This class is required so the named access token defined above is not
// trivially constructible. Without this workaround, any class can construct the
// access token with just {} (at least in VS 2017).
class AccessToken {};
}  // namespace gb

#endif  // GB_BASE_ACCESS_TOKEN_H_
