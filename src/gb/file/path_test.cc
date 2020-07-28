#include "gb/file/path.h"

#include <cctype>

#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(PathTest, IsValidProtocolName) {
  EXPECT_FALSE(IsValidProtocolName({}));
  for (unsigned char i = 0; i < 127; ++i) {
    char ch = static_cast<char>(i);
    EXPECT_EQ(IsValidProtocolName(std::string_view(&ch, 1)),
              (absl::ascii_islower(i) || absl::ascii_isdigit(i)))
        << "Character " << i << " failed test";
  }
  EXPECT_TRUE(IsValidProtocolName("abcdefghijklmnopqrstuvwxyz0123456789"));
}

struct PathExpectation {
  std::string_view path;
  PathFlags flags;
  std::string_view result;
  std::string_view out_result;
};

#define TEST_PATH_EXPECT(a, b) \
  {                            \
    EXPECT_EQ(a, b);           \
    if ((a) != (b)) {          \
      return false;            \
    }                          \
  }

#define PATH_TEST_FUNCTIONS(Name)                                              \
  struct Name##Functions {                                                     \
    static inline std::string_view Call(std::string_view path,                 \
                                        PathFlags flags) {                     \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, flags)), std::string_view>);      \
      return Name(path, flags);                                                \
    }                                                                          \
    static inline std::string_view Call(std::string_view path,                 \
                                        PathFlags flags,                       \
                                        std::string_view* out) {               \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, flags, out)), std::string_view>); \
      return Name(path, flags, out);                                           \
    }                                                                          \
    static inline std::string_view Call(const char* path, PathFlags flags) {   \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, flags)), std::string_view>);      \
      return Name(path, flags);                                                \
    }                                                                          \
    static inline std::string_view Call(const char* path, PathFlags flags,     \
                                        std::string_view* out) {               \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, flags, out)), std::string_view>); \
      return Name(path, flags, out);                                           \
    }                                                                          \
    static inline std::string Call(const std::string& path, PathFlags flags) { \
      static_assert(std::is_same_v<decltype(Name(path, flags)), std::string>); \
      return Name(path, flags);                                                \
    }                                                                          \
    static inline std::string Call(const std::string& path, PathFlags flags,   \
                                   std::string* out) {                         \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, flags, out)), std::string>);      \
      return Name(path, flags, out);                                           \
    }                                                                          \
    static inline std::string_view Call(std::string_view path) {               \
      static_assert(std::is_same_v<decltype(Name(path)), std::string_view>);   \
      return Name(path);                                                       \
    }                                                                          \
    static inline std::string_view Call(std::string_view path,                 \
                                        std::string_view* out) {               \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, out)), std::string_view>);        \
      return Name(path, out);                                                  \
    }                                                                          \
    static inline std::string_view Call(const char* path) {                    \
      static_assert(std::is_same_v<decltype(Name(path)), std::string_view>);   \
      return Name(path);                                                       \
    }                                                                          \
    static inline std::string_view Call(const char* path,                      \
                                        std::string_view* out) {               \
      static_assert(                                                           \
          std::is_same_v<decltype(Name(path, out)), std::string_view>);        \
      return Name(path, out);                                                  \
    }                                                                          \
    static inline std::string Call(const std::string& path) {                  \
      static_assert(std::is_same_v<decltype(Name(path)), std::string>);        \
      return Name(path);                                                       \
    }                                                                          \
    static inline std::string Call(const std::string& path,                    \
                                   std::string* out) {                         \
      static_assert(std::is_same_v<decltype(Name(path, out)), std::string>);   \
      return Name(path, out);                                                  \
    }                                                                          \
  }

template <typename Functions>
bool TestPathFunction(const char* path, PathFlags flags,
                      const std::string& result,
                      const std::string& out_result) {
  std::string_view path_view(path);
  std::string_view result_view;
  std::string_view out_view;

  std::string path_string(path);
  std::string result_string;
  std::string out_string;

  result_view = Functions::Call(path_view, flags);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  out_view = "*****";
  result_view = Functions::Call(path_view, flags, &out_view);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);
  TEST_PATH_EXPECT(std::string(out_view.data(), out_view.size()), out_result);

  result_view = Functions::Call(path, flags);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  out_view = "*****";
  result_view = Functions::Call(path, flags, &out_view);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);
  TEST_PATH_EXPECT(std::string(out_view.data(), out_view.size()), out_result);

  result_string = Functions::Call(path_string, flags);
  TEST_PATH_EXPECT(result_string, result);

  out_string = "*****";
  result_string = Functions::Call(path_string, flags, &out_string);
  TEST_PATH_EXPECT(result_string, result);
  TEST_PATH_EXPECT(out_string, out_result);

  if (flags != kGenericPathFlags) {
    return true;
  }

  result_view = Functions::Call(path_view);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  out_view = "*****";
  result_view = Functions::Call(path_view, &out_view);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);
  TEST_PATH_EXPECT(std::string(out_view.data(), out_view.size()), out_result);

  result_view = Functions::Call(path);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  out_view = "*****";
  result_view = Functions::Call(path, &out_view);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);
  TEST_PATH_EXPECT(std::string(out_view.data(), out_view.size()), out_result);

  result_string = Functions::Call(path_string);
  TEST_PATH_EXPECT(result_string, result);

  out_string = "*****";
  result_string = Functions::Call(path_string, &out_string);
  TEST_PATH_EXPECT(result_string, result);
  TEST_PATH_EXPECT(out_string, out_result);

  return true;
}

#define EXPECT_TEST_FUNCTIONS(Name, path, flags, result, out_result) \
  EXPECT_TRUE(                                                       \
      TestPathFunction<Name##Functions>(path, flags, result, out_result))

TEST(PathTest, RemoveProtocol) {
  PATH_TEST_FUNCTIONS(RemoveProtocol);
#define REMOVE_PROTOCOL_TEST(path, flags, result, out_result) \
  EXPECT_TEST_FUNCTIONS(RemoveProtocol, path, flags, result, out_result)

  REMOVE_PROTOCOL_TEST("", kGenericPathFlags, "", "");
  REMOVE_PROTOCOL_TEST("", PathFlag::kAllowProtocol, "", "");
  REMOVE_PROTOCOL_TEST("", PathFlag::kRequireProtocol, "", "");
  REMOVE_PROTOCOL_TEST("", kLocalPathFlags, "", "");
  REMOVE_PROTOCOL_TEST(":", kGenericPathFlags, ":", "");
  REMOVE_PROTOCOL_TEST(":", PathFlag::kAllowProtocol, ":", "");
  REMOVE_PROTOCOL_TEST(":", PathFlag::kRequireProtocol, ":", "");
  REMOVE_PROTOCOL_TEST(":", kLocalPathFlags, ":", "");
  REMOVE_PROTOCOL_TEST("abc", kGenericPathFlags, "abc", "");
  REMOVE_PROTOCOL_TEST("abc", PathFlag::kAllowProtocol, "abc", "");
  REMOVE_PROTOCOL_TEST("abc", PathFlag::kRequireProtocol, "abc", "");
  REMOVE_PROTOCOL_TEST("abc", kLocalPathFlags, "abc", "");
  REMOVE_PROTOCOL_TEST("abc:", kGenericPathFlags, "", "abc");
  REMOVE_PROTOCOL_TEST("abc:", PathFlag::kAllowProtocol, "", "abc");
  REMOVE_PROTOCOL_TEST("abc:", PathFlag::kRequireProtocol, "", "abc");
  REMOVE_PROTOCOL_TEST("abc:", kLocalPathFlags, "abc:", "");
  REMOVE_PROTOCOL_TEST("abc:xyz", kGenericPathFlags, "xyz", "abc");
  REMOVE_PROTOCOL_TEST("abc:xyz", PathFlag::kAllowProtocol, "xyz", "abc");
  REMOVE_PROTOCOL_TEST("abc:xyz", PathFlag::kRequireProtocol, "xyz", "abc");
  REMOVE_PROTOCOL_TEST("abc:xyz", kLocalPathFlags, "abc:xyz", "");
  REMOVE_PROTOCOL_TEST("abc:/", kGenericPathFlags, "/", "abc");
  REMOVE_PROTOCOL_TEST("abc:/", PathFlag::kAllowProtocol, "/", "abc");
  REMOVE_PROTOCOL_TEST("abc:/", PathFlag::kRequireProtocol, "/", "abc");
  REMOVE_PROTOCOL_TEST("abc:/", kLocalPathFlags, "abc:/", "");
  REMOVE_PROTOCOL_TEST("abc:/xyz", kGenericPathFlags, "/xyz", "abc");
  REMOVE_PROTOCOL_TEST("abc:/xyz", PathFlag::kAllowProtocol, "/xyz", "abc");
  REMOVE_PROTOCOL_TEST("abc:/xyz", PathFlag::kRequireProtocol, "/xyz", "abc");
  REMOVE_PROTOCOL_TEST("abc:/xyz", kLocalPathFlags, "abc:/xyz", "");
  REMOVE_PROTOCOL_TEST("ABC:xyz", kGenericPathFlags, "ABC:xyz", "");
  REMOVE_PROTOCOL_TEST("ABC:xyz", PathFlag::kAllowProtocol, "ABC:xyz", "");
  REMOVE_PROTOCOL_TEST("ABC:xyz", PathFlag::kRequireProtocol, "ABC:xyz", "");
  REMOVE_PROTOCOL_TEST("ABC:xyz", kLocalPathFlags, "ABC:xyz", "");
}

TEST(PathTest, RemoveRoot) {
  PATH_TEST_FUNCTIONS(RemoveRoot);
#define REMOVE_ROOT_TEST(path, flags, result, out_result) \
  EXPECT_TEST_FUNCTIONS(RemoveRoot, path, flags, result, out_result)

  // clang-format off
  REMOVE_ROOT_TEST("", kGenericPathFlags, "", "");
  REMOVE_ROOT_TEST("", PathFlag::kAllowProtocol, "", "");
  REMOVE_ROOT_TEST("", PathFlag::kRequireProtocol, "", "");
  REMOVE_ROOT_TEST("", PathFlag::kAllowHost, "", "");
  REMOVE_ROOT_TEST("", PathFlag::kRequireHost, "", "");
  REMOVE_ROOT_TEST("", kLocalPathFlags, "", "");
  REMOVE_ROOT_TEST(":", kGenericPathFlags, ":", "");
  REMOVE_ROOT_TEST(":", PathFlag::kAllowProtocol, ":", "");
  REMOVE_ROOT_TEST(":", PathFlag::kRequireProtocol, ":", "");
  REMOVE_ROOT_TEST(":", PathFlag::kAllowHost, ":", "");
  REMOVE_ROOT_TEST(":", PathFlag::kRequireHost, ":", "");
  REMOVE_ROOT_TEST(":", kLocalPathFlags, ":", "");
  REMOVE_ROOT_TEST("abc", kGenericPathFlags, "abc", "");
  REMOVE_ROOT_TEST("abc", PathFlag::kAllowProtocol, "abc", "");
  REMOVE_ROOT_TEST("abc", PathFlag::kRequireProtocol, "abc", "");
  REMOVE_ROOT_TEST("abc", PathFlag::kAllowHost, "abc", "");
  REMOVE_ROOT_TEST("abc", PathFlag::kRequireHost, "abc", "");
  REMOVE_ROOT_TEST("abc", kLocalPathFlags, "abc", "");
  REMOVE_ROOT_TEST("/", kGenericPathFlags, "", "/");
  REMOVE_ROOT_TEST("/", PathFlag::kAllowProtocol, "", "/");
  REMOVE_ROOT_TEST("/", PathFlag::kRequireProtocol, "", "/");
  REMOVE_ROOT_TEST("/", PathFlag::kAllowHost, "", "/");
  REMOVE_ROOT_TEST("/", PathFlag::kRequireHost, "", "/");
  REMOVE_ROOT_TEST("/", kLocalPathFlags, "", "/");
  REMOVE_ROOT_TEST("/abc", kGenericPathFlags, "abc", "/");
  REMOVE_ROOT_TEST("/abc", PathFlag::kAllowProtocol, "abc", "/");
  REMOVE_ROOT_TEST("/abc", PathFlag::kRequireProtocol, "abc", "/");
  REMOVE_ROOT_TEST("/abc", PathFlag::kAllowHost, "abc", "/");
  REMOVE_ROOT_TEST("/abc", PathFlag::kRequireHost, "abc", "/");
  REMOVE_ROOT_TEST("/abc", kLocalPathFlags, "abc", "/");
  REMOVE_ROOT_TEST("/abc/", kGenericPathFlags, "abc/", "/");
  REMOVE_ROOT_TEST("/abc/", PathFlag::kAllowProtocol, "abc/", "/");
  REMOVE_ROOT_TEST("/abc/", PathFlag::kRequireProtocol, "abc/", "/");
  REMOVE_ROOT_TEST("/abc/", PathFlag::kAllowHost, "abc/", "/");
  REMOVE_ROOT_TEST("/abc/", PathFlag::kRequireHost, "abc/", "/");
  REMOVE_ROOT_TEST("/abc/", kLocalPathFlags, "abc/", "/");
  REMOVE_ROOT_TEST("/abc/xyz", kGenericPathFlags, "abc/xyz", "/");
  REMOVE_ROOT_TEST("/abc/xyz", PathFlag::kAllowProtocol, "abc/xyz", "/");
  REMOVE_ROOT_TEST("/abc/xyz", PathFlag::kRequireProtocol, "abc/xyz", "/");
  REMOVE_ROOT_TEST("/abc/xyz", PathFlag::kAllowHost, "abc/xyz", "/");
  REMOVE_ROOT_TEST("/abc/xyz", PathFlag::kRequireHost, "abc/xyz", "/");
  REMOVE_ROOT_TEST("/abc/xyz", kLocalPathFlags, "abc/xyz", "/");
  REMOVE_ROOT_TEST("//", kGenericPathFlags, "", "/");
  REMOVE_ROOT_TEST("//", PathFlag::kAllowProtocol, "", "/");
  REMOVE_ROOT_TEST("//", PathFlag::kRequireProtocol, "", "/");
  REMOVE_ROOT_TEST("//", PathFlag::kAllowHost, "", "/");
  REMOVE_ROOT_TEST("//", PathFlag::kRequireHost, "", "/");
  REMOVE_ROOT_TEST("//", kLocalPathFlags, "", "/");
  REMOVE_ROOT_TEST("//abc", kGenericPathFlags, "", "//abc");
  REMOVE_ROOT_TEST("//abc", PathFlag::kAllowProtocol, "abc", "/");
  REMOVE_ROOT_TEST("//abc", PathFlag::kRequireProtocol, "abc", "/");
  REMOVE_ROOT_TEST("//abc", PathFlag::kAllowHost, "", "//abc");
  REMOVE_ROOT_TEST("//abc", PathFlag::kRequireHost, "", "//abc");
  REMOVE_ROOT_TEST("//abc", kLocalPathFlags, "abc", "/");
  REMOVE_ROOT_TEST("//abc/", kGenericPathFlags, "", "//abc");
  REMOVE_ROOT_TEST("//abc/", PathFlag::kAllowProtocol, "abc/", "/");
  REMOVE_ROOT_TEST("//abc/", PathFlag::kRequireProtocol, "abc/", "/");
  REMOVE_ROOT_TEST("//abc/", PathFlag::kAllowHost, "", "//abc");
  REMOVE_ROOT_TEST("//abc/", PathFlag::kRequireHost, "", "//abc");
  REMOVE_ROOT_TEST("//abc/", kLocalPathFlags, "abc/", "/");
  REMOVE_ROOT_TEST("//abc/xyz", kGenericPathFlags, "xyz", "//abc");
  REMOVE_ROOT_TEST("//abc/xyz", PathFlag::kAllowProtocol, "abc/xyz", "/");
  REMOVE_ROOT_TEST("//abc/xyz", PathFlag::kRequireProtocol, "abc/xyz", "/");
  REMOVE_ROOT_TEST("//abc/xyz", PathFlag::kAllowHost, "xyz", "//abc");
  REMOVE_ROOT_TEST("//abc/xyz", PathFlag::kRequireHost, "xyz", "//abc");
  REMOVE_ROOT_TEST("//abc/xyz", kLocalPathFlags, "abc/xyz", "/");
  REMOVE_ROOT_TEST("abc:", kGenericPathFlags, "", "abc:");
  REMOVE_ROOT_TEST("abc:", PathFlag::kAllowProtocol, "", "abc:");
  REMOVE_ROOT_TEST("abc:", PathFlag::kRequireProtocol, "", "abc:");
  REMOVE_ROOT_TEST("abc:", PathFlag::kAllowHost, "abc:", "");
  REMOVE_ROOT_TEST("abc:", PathFlag::kRequireHost, "abc:", "");
  REMOVE_ROOT_TEST("abc:", kLocalPathFlags, "abc:", "");
  REMOVE_ROOT_TEST("abc:xyz", kGenericPathFlags, "xyz", "abc:");
  REMOVE_ROOT_TEST("abc:xyz", PathFlag::kAllowProtocol, "xyz", "abc:");
  REMOVE_ROOT_TEST("abc:xyz", PathFlag::kRequireProtocol, "xyz", "abc:");
  REMOVE_ROOT_TEST("abc:xyz", PathFlag::kAllowHost, "abc:xyz", "");
  REMOVE_ROOT_TEST("abc:xyz", PathFlag::kRequireHost, "abc:xyz", "");
  REMOVE_ROOT_TEST("abc:xyz", kLocalPathFlags, "abc:xyz", "");
  REMOVE_ROOT_TEST("abc:/", kGenericPathFlags, "", "abc:/");
  REMOVE_ROOT_TEST("abc:/", PathFlag::kAllowProtocol, "", "abc:/");
  REMOVE_ROOT_TEST("abc:/", PathFlag::kRequireProtocol, "", "abc:/");
  REMOVE_ROOT_TEST("abc:/", PathFlag::kAllowHost, "abc:/", "");
  REMOVE_ROOT_TEST("abc:/", PathFlag::kRequireHost, "abc:/", "");
  REMOVE_ROOT_TEST("abc:/", kLocalPathFlags, "abc:/", "");
  REMOVE_ROOT_TEST("abc:/xyz", kGenericPathFlags, "xyz", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz", PathFlag::kAllowProtocol, "xyz", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz", PathFlag::kRequireProtocol, "xyz", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz", PathFlag::kAllowHost, "abc:/xyz", "");
  REMOVE_ROOT_TEST("abc:/xyz", PathFlag::kRequireHost, "abc:/xyz", "");
  REMOVE_ROOT_TEST("abc:/xyz", kLocalPathFlags, "abc:/xyz", "");
  REMOVE_ROOT_TEST("abc:/xyz/", kGenericPathFlags, "xyz/", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz/", PathFlag::kAllowProtocol, "xyz/", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz/", PathFlag::kRequireProtocol, "xyz/", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz/", PathFlag::kAllowHost, "abc:/xyz/", "");
  REMOVE_ROOT_TEST("abc:/xyz/", PathFlag::kRequireHost, "abc:/xyz/", "");
  REMOVE_ROOT_TEST("abc:/xyz/", kLocalPathFlags, "abc:/xyz/", "");
  REMOVE_ROOT_TEST("abc:/xyz/ijk", kGenericPathFlags, "xyz/ijk", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz/ijk", PathFlag::kAllowProtocol, "xyz/ijk", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz/ijk", PathFlag::kRequireProtocol, "xyz/ijk", "abc:/");
  REMOVE_ROOT_TEST("abc:/xyz/ijk", PathFlag::kAllowHost, "abc:/xyz/ijk", "");
  REMOVE_ROOT_TEST("abc:/xyz/ijk", PathFlag::kRequireHost, "abc:/xyz/ijk", "");
  REMOVE_ROOT_TEST("abc:/xyz/ijk", kLocalPathFlags, "abc:/xyz/ijk", "");
  REMOVE_ROOT_TEST("abc://", kGenericPathFlags, "", "abc:/");
  REMOVE_ROOT_TEST("abc://", PathFlag::kAllowProtocol, "", "abc:/");
  REMOVE_ROOT_TEST("abc://", PathFlag::kRequireProtocol, "", "abc:/");
  REMOVE_ROOT_TEST("abc://", PathFlag::kAllowHost, "abc://", "");
  REMOVE_ROOT_TEST("abc://", PathFlag::kRequireHost, "abc://", "");
  REMOVE_ROOT_TEST("abc://", kLocalPathFlags, "abc://", "");
  REMOVE_ROOT_TEST("abc://xyz", kGenericPathFlags, "", "abc://xyz");
  REMOVE_ROOT_TEST("abc://xyz", PathFlag::kAllowProtocol, "xyz", "abc:/");
  REMOVE_ROOT_TEST("abc://xyz", PathFlag::kRequireProtocol, "xyz", "abc:/");
  REMOVE_ROOT_TEST("abc://xyz", PathFlag::kAllowHost, "abc://xyz", "");
  REMOVE_ROOT_TEST("abc://xyz", PathFlag::kRequireHost, "abc://xyz", "");
  REMOVE_ROOT_TEST("abc://xyz", kLocalPathFlags, "abc://xyz", "");
  REMOVE_ROOT_TEST("abc://xyz/", kGenericPathFlags, "", "abc://xyz");
  REMOVE_ROOT_TEST("abc://xyz/", PathFlag::kAllowProtocol, "xyz/", "abc:/");
  REMOVE_ROOT_TEST("abc://xyz/", PathFlag::kRequireProtocol, "xyz/", "abc:/");
  REMOVE_ROOT_TEST("abc://xyz/", PathFlag::kAllowHost, "abc://xyz/", "");
  REMOVE_ROOT_TEST("abc://xyz/", PathFlag::kRequireHost, "abc://xyz/", "");
  REMOVE_ROOT_TEST("abc://xyz/", kLocalPathFlags, "abc://xyz/", "");
  REMOVE_ROOT_TEST("abc://xyz/ijk", kGenericPathFlags, "ijk", "abc://xyz");
  REMOVE_ROOT_TEST("abc://xyz/ijk", PathFlag::kAllowProtocol, "xyz/ijk", "abc:/");
  REMOVE_ROOT_TEST("abc://xyz/ijk", PathFlag::kRequireProtocol, "xyz/ijk", "abc:/");
  REMOVE_ROOT_TEST("abc://xyz/ijk", PathFlag::kAllowHost, "abc://xyz/ijk", "");
  REMOVE_ROOT_TEST("abc://xyz/ijk", PathFlag::kRequireHost, "abc://xyz/ijk", "");
  REMOVE_ROOT_TEST("abc://xyz/ijk", kLocalPathFlags, "abc://xyz/ijk", "");
  REMOVE_ROOT_TEST("ABC:xyz", kGenericPathFlags, "ABC:xyz", "");
  REMOVE_ROOT_TEST("ABC:xyz", PathFlag::kAllowProtocol, "ABC:xyz", "");
  REMOVE_ROOT_TEST("ABC:xyz", PathFlag::kRequireProtocol, "ABC:xyz", "");
  REMOVE_ROOT_TEST("ABC:xyz", PathFlag::kAllowHost, "ABC:xyz", "");
  REMOVE_ROOT_TEST("ABC:xyz", PathFlag::kRequireHost, "ABC:xyz", "");
  REMOVE_ROOT_TEST("ABC:xyz", kLocalPathFlags, "ABC:xyz", "");
  //clang-format on
}

TEST(PathTest, RemoveFilename) {
  PATH_TEST_FUNCTIONS(RemoveFilename);
#define REMOVE_FILENAME_TEST(path, flags, result, out_result) \
  EXPECT_TEST_FUNCTIONS(RemoveFilename, path, flags, result, out_result)

  // clang-format off
  REMOVE_FILENAME_TEST("", kGenericPathFlags, "", "");
  REMOVE_FILENAME_TEST("", PathFlag::kAllowProtocol, "", "");
  REMOVE_FILENAME_TEST("", PathFlag::kRequireProtocol, "", "");
  REMOVE_FILENAME_TEST("", PathFlag::kAllowHost, "", "");
  REMOVE_FILENAME_TEST("", PathFlag::kRequireHost, "", "");
  REMOVE_FILENAME_TEST("", kLocalPathFlags, "", "");
  REMOVE_FILENAME_TEST(":", kGenericPathFlags, "", ":");
  REMOVE_FILENAME_TEST(":", PathFlag::kAllowProtocol, "", ":");
  REMOVE_FILENAME_TEST(":", PathFlag::kRequireProtocol, "", ":");
  REMOVE_FILENAME_TEST(":", PathFlag::kAllowHost, "", ":");
  REMOVE_FILENAME_TEST(":", PathFlag::kRequireHost, "", ":");
  REMOVE_FILENAME_TEST(":", kLocalPathFlags, "", ":");
  REMOVE_FILENAME_TEST("abc", kGenericPathFlags, "", "abc");
  REMOVE_FILENAME_TEST("abc", PathFlag::kAllowProtocol, "", "abc");
  REMOVE_FILENAME_TEST("abc", PathFlag::kRequireProtocol, "", "abc");
  REMOVE_FILENAME_TEST("abc", PathFlag::kAllowHost, "", "abc");
  REMOVE_FILENAME_TEST("abc", PathFlag::kRequireHost, "", "abc");
  REMOVE_FILENAME_TEST("abc", kLocalPathFlags, "", "abc");
  REMOVE_FILENAME_TEST("/", kGenericPathFlags, "/", "");
  REMOVE_FILENAME_TEST("/", PathFlag::kAllowProtocol, "/", "");
  REMOVE_FILENAME_TEST("/", PathFlag::kRequireProtocol, "/", "");
  REMOVE_FILENAME_TEST("/", PathFlag::kAllowHost, "/", "");
  REMOVE_FILENAME_TEST("/", PathFlag::kRequireHost, "/", "");
  REMOVE_FILENAME_TEST("/", kLocalPathFlags, "/", "");
  REMOVE_FILENAME_TEST("/abc", kGenericPathFlags, "/", "abc");
  REMOVE_FILENAME_TEST("/abc", PathFlag::kAllowProtocol, "/", "abc");
  REMOVE_FILENAME_TEST("/abc", PathFlag::kRequireProtocol, "/", "abc");
  REMOVE_FILENAME_TEST("/abc", PathFlag::kAllowHost, "/", "abc");
  REMOVE_FILENAME_TEST("/abc", PathFlag::kRequireHost, "/", "abc");
  REMOVE_FILENAME_TEST("/abc", kLocalPathFlags, "/", "abc");
  REMOVE_FILENAME_TEST("/abc/", kGenericPathFlags, "/abc", "");
  REMOVE_FILENAME_TEST("/abc/", PathFlag::kAllowProtocol, "/abc", "");
  REMOVE_FILENAME_TEST("/abc/", PathFlag::kRequireProtocol, "/abc", "");
  REMOVE_FILENAME_TEST("/abc/", PathFlag::kAllowHost, "/abc", "");
  REMOVE_FILENAME_TEST("/abc/", PathFlag::kRequireHost, "/abc", "");
  REMOVE_FILENAME_TEST("/abc/", kLocalPathFlags, "/abc", "");
  REMOVE_FILENAME_TEST("/abc/xyz", kGenericPathFlags, "/abc", "xyz");
  REMOVE_FILENAME_TEST("/abc/xyz", PathFlag::kAllowProtocol, "/abc", "xyz");
  REMOVE_FILENAME_TEST("/abc/xyz", PathFlag::kRequireProtocol, "/abc", "xyz");
  REMOVE_FILENAME_TEST("/abc/xyz", PathFlag::kAllowHost, "/abc", "xyz");
  REMOVE_FILENAME_TEST("/abc/xyz", PathFlag::kRequireHost, "/abc", "xyz");
  REMOVE_FILENAME_TEST("/abc/xyz", kLocalPathFlags, "/abc", "xyz");
  REMOVE_FILENAME_TEST("//", kGenericPathFlags, "/", "");
  REMOVE_FILENAME_TEST("//", PathFlag::kAllowProtocol, "/", "");
  REMOVE_FILENAME_TEST("//", PathFlag::kRequireProtocol, "/", "");
  REMOVE_FILENAME_TEST("//", PathFlag::kAllowHost, "/", "");
  REMOVE_FILENAME_TEST("//", PathFlag::kRequireHost, "/", "");
  REMOVE_FILENAME_TEST("//", kLocalPathFlags, "/", "");
  REMOVE_FILENAME_TEST("//abc", kGenericPathFlags, "//abc", "");
  REMOVE_FILENAME_TEST("//abc", PathFlag::kAllowProtocol, "/", "abc");
  REMOVE_FILENAME_TEST("//abc", PathFlag::kRequireProtocol, "/", "abc");
  REMOVE_FILENAME_TEST("//abc", PathFlag::kAllowHost, "//abc", "");
  REMOVE_FILENAME_TEST("//abc", PathFlag::kRequireHost, "//abc", "");
  REMOVE_FILENAME_TEST("//abc", kLocalPathFlags, "/", "abc");
  REMOVE_FILENAME_TEST("//abc/", kGenericPathFlags, "//abc", "");
  REMOVE_FILENAME_TEST("//abc/", PathFlag::kAllowProtocol, "//abc", "");
  REMOVE_FILENAME_TEST("//abc/", PathFlag::kRequireProtocol, "//abc", "");
  REMOVE_FILENAME_TEST("//abc/", PathFlag::kAllowHost, "//abc", "");
  REMOVE_FILENAME_TEST("//abc/", PathFlag::kRequireHost, "//abc", "");
  REMOVE_FILENAME_TEST("//abc/", kLocalPathFlags, "//abc", "");
  REMOVE_FILENAME_TEST("//abc/xyz", kGenericPathFlags, "//abc", "xyz");
  REMOVE_FILENAME_TEST("//abc/xyz", PathFlag::kAllowProtocol, "//abc", "xyz");
  REMOVE_FILENAME_TEST("//abc/xyz", PathFlag::kRequireProtocol, "//abc", "xyz");
  REMOVE_FILENAME_TEST("//abc/xyz", PathFlag::kAllowHost, "//abc", "xyz");
  REMOVE_FILENAME_TEST("//abc/xyz", PathFlag::kRequireHost, "//abc", "xyz");
  REMOVE_FILENAME_TEST("//abc/xyz", kLocalPathFlags, "//abc", "xyz");
  REMOVE_FILENAME_TEST("abc:", kGenericPathFlags, "abc:", "");
  REMOVE_FILENAME_TEST("abc:", PathFlag::kAllowProtocol, "abc:", "");
  REMOVE_FILENAME_TEST("abc:", PathFlag::kRequireProtocol, "abc:", "");
  REMOVE_FILENAME_TEST("abc:", PathFlag::kAllowHost, "", "abc:");
  REMOVE_FILENAME_TEST("abc:", PathFlag::kRequireHost, "", "abc:");
  REMOVE_FILENAME_TEST("abc:", kLocalPathFlags, "", "abc:");
  REMOVE_FILENAME_TEST("abc:xyz", kGenericPathFlags, "abc:", "xyz");
  REMOVE_FILENAME_TEST("abc:xyz", PathFlag::kAllowProtocol, "abc:", "xyz");
  REMOVE_FILENAME_TEST("abc:xyz", PathFlag::kRequireProtocol, "abc:", "xyz");
  REMOVE_FILENAME_TEST("abc:xyz", PathFlag::kAllowHost, "", "abc:xyz");
  REMOVE_FILENAME_TEST("abc:xyz", PathFlag::kRequireHost, "", "abc:xyz");
  REMOVE_FILENAME_TEST("abc:xyz", kLocalPathFlags, "", "abc:xyz");
  REMOVE_FILENAME_TEST("abc:/", kGenericPathFlags, "abc:/", "");
  REMOVE_FILENAME_TEST("abc:/", PathFlag::kAllowProtocol, "abc:/", "");
  REMOVE_FILENAME_TEST("abc:/", PathFlag::kRequireProtocol, "abc:/", "");
  REMOVE_FILENAME_TEST("abc:/", PathFlag::kAllowHost, "abc:", "");
  REMOVE_FILENAME_TEST("abc:/", PathFlag::kRequireHost, "abc:", "");
  REMOVE_FILENAME_TEST("abc:/", kLocalPathFlags, "abc:", "");
  REMOVE_FILENAME_TEST("abc:/xyz", kGenericPathFlags, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc:/xyz", PathFlag::kAllowProtocol, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc:/xyz", PathFlag::kRequireProtocol, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc:/xyz", PathFlag::kAllowHost, "abc:", "xyz");
  REMOVE_FILENAME_TEST("abc:/xyz", PathFlag::kRequireHost, "abc:", "xyz");
  REMOVE_FILENAME_TEST("abc:/xyz", kLocalPathFlags, "abc:", "xyz");
  REMOVE_FILENAME_TEST("abc:/xyz/", kGenericPathFlags, "abc:/xyz", "");
  REMOVE_FILENAME_TEST("abc:/xyz/", PathFlag::kAllowProtocol, "abc:/xyz", "");
  REMOVE_FILENAME_TEST("abc:/xyz/", PathFlag::kRequireProtocol, "abc:/xyz", "");
  REMOVE_FILENAME_TEST("abc:/xyz/", PathFlag::kAllowHost, "abc:/xyz", "");
  REMOVE_FILENAME_TEST("abc:/xyz/", PathFlag::kRequireHost, "abc:/xyz", "");
  REMOVE_FILENAME_TEST("abc:/xyz/", kLocalPathFlags, "abc:/xyz", "");
  REMOVE_FILENAME_TEST("abc:/xyz/ijk", kGenericPathFlags, "abc:/xyz", "ijk");
  REMOVE_FILENAME_TEST("abc:/xyz/ijk", PathFlag::kAllowProtocol, "abc:/xyz", "ijk");
  REMOVE_FILENAME_TEST("abc:/xyz/ijk", PathFlag::kRequireProtocol, "abc:/xyz", "ijk");
  REMOVE_FILENAME_TEST("abc:/xyz/ijk", PathFlag::kAllowHost, "abc:/xyz", "ijk");
  REMOVE_FILENAME_TEST("abc:/xyz/ijk", PathFlag::kRequireHost, "abc:/xyz", "ijk");
  REMOVE_FILENAME_TEST("abc:/xyz/ijk", kLocalPathFlags, "abc:/xyz", "ijk");
  REMOVE_FILENAME_TEST("abc://", kGenericPathFlags, "abc:/", "");
  REMOVE_FILENAME_TEST("abc://", PathFlag::kAllowProtocol, "abc:/", "");
  REMOVE_FILENAME_TEST("abc://", PathFlag::kRequireProtocol, "abc:/", "");
  REMOVE_FILENAME_TEST("abc://", PathFlag::kAllowHost, "abc:/", "");
  REMOVE_FILENAME_TEST("abc://", PathFlag::kRequireHost, "abc:/", "");
  REMOVE_FILENAME_TEST("abc://", kLocalPathFlags, "abc:/", "");
  REMOVE_FILENAME_TEST("abc://xyz", kGenericPathFlags, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz", PathFlag::kAllowProtocol, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc://xyz", PathFlag::kRequireProtocol, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc://xyz", PathFlag::kAllowHost, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc://xyz", PathFlag::kRequireHost, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc://xyz", kLocalPathFlags, "abc:/", "xyz");
  REMOVE_FILENAME_TEST("abc://xyz/", kGenericPathFlags, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz/", PathFlag::kAllowProtocol, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz/", PathFlag::kRequireProtocol, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz/", PathFlag::kAllowHost, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz/", PathFlag::kRequireHost, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz/", kLocalPathFlags, "abc://xyz", "");
  REMOVE_FILENAME_TEST("abc://xyz/ijk", kGenericPathFlags, "abc://xyz", "ijk");
  REMOVE_FILENAME_TEST("abc://xyz/ijk", PathFlag::kAllowProtocol, "abc://xyz", "ijk");
  REMOVE_FILENAME_TEST("abc://xyz/ijk", PathFlag::kRequireProtocol, "abc://xyz", "ijk");
  REMOVE_FILENAME_TEST("abc://xyz/ijk", PathFlag::kAllowHost, "abc://xyz", "ijk");
  REMOVE_FILENAME_TEST("abc://xyz/ijk", PathFlag::kRequireHost, "abc://xyz", "ijk");
  REMOVE_FILENAME_TEST("abc://xyz/ijk", kLocalPathFlags, "abc://xyz", "ijk");
  REMOVE_FILENAME_TEST("ABC:xyz", kGenericPathFlags, "", "ABC:xyz");
  REMOVE_FILENAME_TEST("ABC:xyz", PathFlag::kAllowProtocol, "", "ABC:xyz");
  REMOVE_FILENAME_TEST("ABC:xyz", PathFlag::kRequireProtocol, "", "ABC:xyz");
  REMOVE_FILENAME_TEST("ABC:xyz", PathFlag::kAllowHost, "", "ABC:xyz");
  REMOVE_FILENAME_TEST("ABC:xyz", PathFlag::kRequireHost, "", "ABC:xyz");
  REMOVE_FILENAME_TEST("ABC:xyz", kLocalPathFlags, "", "ABC:xyz");
  // clang-format on
}

TEST(PathTest, RemoveFolder) {
  PATH_TEST_FUNCTIONS(RemoveFolder);
#define REMOVE_FOLDER_TEST(path, flags, result, out_result) \
  EXPECT_TEST_FUNCTIONS(RemoveFolder, path, flags, result, out_result)

  // clang-format off
  REMOVE_FOLDER_TEST("", kGenericPathFlags, "", "");
  REMOVE_FOLDER_TEST("", PathFlag::kAllowProtocol, "", "");
  REMOVE_FOLDER_TEST("", PathFlag::kRequireProtocol, "", "");
  REMOVE_FOLDER_TEST("", PathFlag::kAllowHost, "", "");
  REMOVE_FOLDER_TEST("", PathFlag::kRequireHost, "", "");
  REMOVE_FOLDER_TEST("", kLocalPathFlags, "", "");
  REMOVE_FOLDER_TEST(":", kGenericPathFlags, ":", "");
  REMOVE_FOLDER_TEST(":", PathFlag::kAllowProtocol, ":", "");
  REMOVE_FOLDER_TEST(":", PathFlag::kRequireProtocol, ":", "");
  REMOVE_FOLDER_TEST(":", PathFlag::kAllowHost, ":", "");
  REMOVE_FOLDER_TEST(":", PathFlag::kRequireHost, ":", "");
  REMOVE_FOLDER_TEST(":", kLocalPathFlags, ":", "");
  REMOVE_FOLDER_TEST("abc", kGenericPathFlags, "abc", "");
  REMOVE_FOLDER_TEST("abc", PathFlag::kAllowProtocol, "abc", "");
  REMOVE_FOLDER_TEST("abc", PathFlag::kRequireProtocol, "abc", "");
  REMOVE_FOLDER_TEST("abc", PathFlag::kAllowHost, "abc", "");
  REMOVE_FOLDER_TEST("abc", PathFlag::kRequireHost, "abc", "");
  REMOVE_FOLDER_TEST("abc", kLocalPathFlags, "abc", "");
  REMOVE_FOLDER_TEST("/", kGenericPathFlags, "", "/");
  REMOVE_FOLDER_TEST("/", PathFlag::kAllowProtocol, "", "/");
  REMOVE_FOLDER_TEST("/", PathFlag::kRequireProtocol, "", "/");
  REMOVE_FOLDER_TEST("/", PathFlag::kAllowHost, "", "/");
  REMOVE_FOLDER_TEST("/", PathFlag::kRequireHost, "", "/");
  REMOVE_FOLDER_TEST("/", kLocalPathFlags, "", "/");
  REMOVE_FOLDER_TEST("/abc", kGenericPathFlags, "abc", "/");
  REMOVE_FOLDER_TEST("/abc", PathFlag::kAllowProtocol, "abc", "/");
  REMOVE_FOLDER_TEST("/abc", PathFlag::kRequireProtocol, "abc", "/");
  REMOVE_FOLDER_TEST("/abc", PathFlag::kAllowHost, "abc", "/");
  REMOVE_FOLDER_TEST("/abc", PathFlag::kRequireHost, "abc", "/");
  REMOVE_FOLDER_TEST("/abc", kLocalPathFlags, "abc", "/");
  REMOVE_FOLDER_TEST("/abc/", kGenericPathFlags, "", "/abc");
  REMOVE_FOLDER_TEST("/abc/", PathFlag::kAllowProtocol, "", "/abc");
  REMOVE_FOLDER_TEST("/abc/", PathFlag::kRequireProtocol, "", "/abc");
  REMOVE_FOLDER_TEST("/abc/", PathFlag::kAllowHost, "", "/abc");
  REMOVE_FOLDER_TEST("/abc/", PathFlag::kRequireHost, "", "/abc");
  REMOVE_FOLDER_TEST("/abc/", kLocalPathFlags, "", "/abc");
  REMOVE_FOLDER_TEST("/abc/xyz", kGenericPathFlags, "xyz", "/abc");
  REMOVE_FOLDER_TEST("/abc/xyz", PathFlag::kAllowProtocol, "xyz", "/abc");
  REMOVE_FOLDER_TEST("/abc/xyz", PathFlag::kRequireProtocol, "xyz", "/abc");
  REMOVE_FOLDER_TEST("/abc/xyz", PathFlag::kAllowHost, "xyz", "/abc");
  REMOVE_FOLDER_TEST("/abc/xyz", PathFlag::kRequireHost, "xyz", "/abc");
  REMOVE_FOLDER_TEST("/abc/xyz", kLocalPathFlags, "xyz", "/abc");
  REMOVE_FOLDER_TEST("//", kGenericPathFlags, "", "/");
  REMOVE_FOLDER_TEST("//", PathFlag::kAllowProtocol, "", "/");
  REMOVE_FOLDER_TEST("//", PathFlag::kRequireProtocol, "", "/");
  REMOVE_FOLDER_TEST("//", PathFlag::kAllowHost, "", "/");
  REMOVE_FOLDER_TEST("//", PathFlag::kRequireHost, "", "/");
  REMOVE_FOLDER_TEST("//", kLocalPathFlags, "", "/");
  REMOVE_FOLDER_TEST("//abc", kGenericPathFlags, "", "//abc");
  REMOVE_FOLDER_TEST("//abc", PathFlag::kAllowProtocol, "abc", "/");
  REMOVE_FOLDER_TEST("//abc", PathFlag::kRequireProtocol, "abc", "/");
  REMOVE_FOLDER_TEST("//abc", PathFlag::kAllowHost, "", "//abc");
  REMOVE_FOLDER_TEST("//abc", PathFlag::kRequireHost, "", "//abc");
  REMOVE_FOLDER_TEST("//abc", kLocalPathFlags, "abc", "/");
  REMOVE_FOLDER_TEST("//abc/", kGenericPathFlags, "", "//abc");
  REMOVE_FOLDER_TEST("//abc/", PathFlag::kAllowProtocol, "", "//abc");
  REMOVE_FOLDER_TEST("//abc/", PathFlag::kRequireProtocol, "", "//abc");
  REMOVE_FOLDER_TEST("//abc/", PathFlag::kAllowHost, "", "//abc");
  REMOVE_FOLDER_TEST("//abc/", PathFlag::kRequireHost, "", "//abc");
  REMOVE_FOLDER_TEST("//abc/", kLocalPathFlags, "", "//abc");
  REMOVE_FOLDER_TEST("//abc/xyz", kGenericPathFlags, "xyz", "//abc");
  REMOVE_FOLDER_TEST("//abc/xyz", PathFlag::kAllowProtocol, "xyz", "//abc");
  REMOVE_FOLDER_TEST("//abc/xyz", PathFlag::kRequireProtocol, "xyz", "//abc");
  REMOVE_FOLDER_TEST("//abc/xyz", PathFlag::kAllowHost, "xyz", "//abc");
  REMOVE_FOLDER_TEST("//abc/xyz", PathFlag::kRequireHost, "xyz", "//abc");
  REMOVE_FOLDER_TEST("//abc/xyz", kLocalPathFlags, "xyz", "//abc");
  REMOVE_FOLDER_TEST("abc:", kGenericPathFlags, "", "abc:");
  REMOVE_FOLDER_TEST("abc:", PathFlag::kAllowProtocol, "", "abc:");
  REMOVE_FOLDER_TEST("abc:", PathFlag::kRequireProtocol, "", "abc:");
  REMOVE_FOLDER_TEST("abc:", PathFlag::kAllowHost, "abc:", "");
  REMOVE_FOLDER_TEST("abc:", PathFlag::kRequireHost, "abc:", "");
  REMOVE_FOLDER_TEST("abc:", kLocalPathFlags, "abc:", "");
  REMOVE_FOLDER_TEST("abc:xyz", kGenericPathFlags, "xyz", "abc:");
  REMOVE_FOLDER_TEST("abc:xyz", PathFlag::kAllowProtocol, "xyz", "abc:");
  REMOVE_FOLDER_TEST("abc:xyz", PathFlag::kRequireProtocol, "xyz", "abc:");
  REMOVE_FOLDER_TEST("abc:xyz", PathFlag::kAllowHost, "abc:xyz", "");
  REMOVE_FOLDER_TEST("abc:xyz", PathFlag::kRequireHost, "abc:xyz", "");
  REMOVE_FOLDER_TEST("abc:xyz", kLocalPathFlags, "abc:xyz", "");
  REMOVE_FOLDER_TEST("abc:/", kGenericPathFlags, "", "abc:/");
  REMOVE_FOLDER_TEST("abc:/", PathFlag::kAllowProtocol, "", "abc:/");
  REMOVE_FOLDER_TEST("abc:/", PathFlag::kRequireProtocol, "", "abc:/");
  REMOVE_FOLDER_TEST("abc:/", PathFlag::kAllowHost, "", "abc:");
  REMOVE_FOLDER_TEST("abc:/", PathFlag::kRequireHost, "", "abc:");
  REMOVE_FOLDER_TEST("abc:/", kLocalPathFlags, "", "abc:");
  REMOVE_FOLDER_TEST("abc:/xyz", kGenericPathFlags, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc:/xyz", PathFlag::kAllowProtocol, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc:/xyz", PathFlag::kRequireProtocol, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc:/xyz", PathFlag::kAllowHost, "xyz", "abc:");
  REMOVE_FOLDER_TEST("abc:/xyz", PathFlag::kRequireHost, "xyz", "abc:");
  REMOVE_FOLDER_TEST("abc:/xyz", kLocalPathFlags, "xyz", "abc:");
  REMOVE_FOLDER_TEST("abc:/xyz/", kGenericPathFlags, "", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/", PathFlag::kAllowProtocol, "", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/", PathFlag::kRequireProtocol, "", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/", PathFlag::kAllowHost, "", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/", PathFlag::kRequireHost, "", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/", kLocalPathFlags, "", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/ijk", kGenericPathFlags, "ijk", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/ijk", PathFlag::kAllowProtocol, "ijk", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/ijk", PathFlag::kRequireProtocol, "ijk", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/ijk", PathFlag::kAllowHost, "ijk", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/ijk", PathFlag::kRequireHost, "ijk", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc:/xyz/ijk", kLocalPathFlags, "ijk", "abc:/xyz");
  REMOVE_FOLDER_TEST("abc://", kGenericPathFlags, "", "abc:/");
  REMOVE_FOLDER_TEST("abc://", PathFlag::kAllowProtocol, "", "abc:/");
  REMOVE_FOLDER_TEST("abc://", PathFlag::kRequireProtocol, "", "abc:/");
  REMOVE_FOLDER_TEST("abc://", PathFlag::kAllowHost, "", "abc:/");
  REMOVE_FOLDER_TEST("abc://", PathFlag::kRequireHost, "", "abc:/");
  REMOVE_FOLDER_TEST("abc://", kLocalPathFlags, "", "abc:/");
  REMOVE_FOLDER_TEST("abc://xyz", kGenericPathFlags, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz", PathFlag::kAllowProtocol, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc://xyz", PathFlag::kRequireProtocol, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc://xyz", PathFlag::kAllowHost, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc://xyz", PathFlag::kRequireHost, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc://xyz", kLocalPathFlags, "xyz", "abc:/");
  REMOVE_FOLDER_TEST("abc://xyz/", kGenericPathFlags, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/", PathFlag::kAllowProtocol, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/", PathFlag::kRequireProtocol, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/", PathFlag::kAllowHost, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/", PathFlag::kRequireHost, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/", kLocalPathFlags, "", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/ijk", kGenericPathFlags, "ijk", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/ijk", PathFlag::kAllowProtocol, "ijk", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/ijk", PathFlag::kRequireProtocol, "ijk", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/ijk", PathFlag::kAllowHost, "ijk", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/ijk", PathFlag::kRequireHost, "ijk", "abc://xyz");
  REMOVE_FOLDER_TEST("abc://xyz/ijk", kLocalPathFlags, "ijk", "abc://xyz");
  REMOVE_FOLDER_TEST("ABC:xyz", kGenericPathFlags, "ABC:xyz", "");
  REMOVE_FOLDER_TEST("ABC:xyz", PathFlag::kAllowProtocol, "ABC:xyz", "");
  REMOVE_FOLDER_TEST("ABC:xyz", PathFlag::kRequireProtocol, "ABC:xyz", "");
  REMOVE_FOLDER_TEST("ABC:xyz", PathFlag::kAllowHost, "ABC:xyz", "");
  REMOVE_FOLDER_TEST("ABC:xyz", PathFlag::kRequireHost, "ABC:xyz", "");
  REMOVE_FOLDER_TEST("ABC:xyz", kLocalPathFlags, "ABC:xyz", "");
  // clang-format on
}

bool TestGetHostName(const char* path, PathFlags flags,
                     const std::string& result) {
  std::string_view path_view(path);
  std::string_view result_view;
  std::string_view out_view;

  std::string path_string(path);
  std::string result_string;
  std::string out_string;

  result_view = GetHostName(path_view, flags);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  result_view = GetHostName(path, flags);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  result_string = GetHostName(path_string, flags);
  TEST_PATH_EXPECT(result_string, result);

  if (flags != kGenericPathFlags) {
    return true;
  }

  result_view = GetHostName(path_view);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  result_view = GetHostName(path);
  TEST_PATH_EXPECT(std::string(result_view.data(), result_view.size()), result);

  result_string = GetHostName(path_string);
  TEST_PATH_EXPECT(result_string, result);

  return true;
}

TEST(PathTest, GetHostName) {
  static_assert(std::is_same_v<decltype(GetHostName("")), std::string_view>);
  static_assert(std::is_same_v<decltype(GetHostName(std::string_view{})),
                               std::string_view>);
  static_assert(
      std::is_same_v<decltype(GetHostName(std::string{})), std::string>);

  EXPECT_TRUE(TestGetHostName("", kGenericPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("/", kGenericPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("/", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("/", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("/", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("//", kGenericPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("//", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("//", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("//", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("//abc", kGenericPathFlags, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc", PathFlag::kAllowHost, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc", PathFlag::kRequireHost, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("//abc/", kGenericPathFlags, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc/", PathFlag::kAllowHost, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc/", PathFlag::kRequireHost, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc/", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("//abc/xyz", kGenericPathFlags, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc/xyz", PathFlag::kAllowHost, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc/xyz", PathFlag::kRequireHost, "abc"));
  EXPECT_TRUE(TestGetHostName("//abc/xyz", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk:", kGenericPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk:", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk:", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk:", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk:/", kGenericPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk:/", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk:/", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk:/", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk://", kGenericPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk://", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc", kGenericPathFlags, "abc"));
  EXPECT_TRUE(TestGetHostName("ijk://abc", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc/", kGenericPathFlags, "abc"));
  EXPECT_TRUE(TestGetHostName("ijk://abc/", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc/", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc/", kLocalPathFlags, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc/xyz", kGenericPathFlags, "abc"));
  EXPECT_TRUE(TestGetHostName("ijk://abc/xyz", PathFlag::kAllowHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc/xyz", PathFlag::kRequireHost, ""));
  EXPECT_TRUE(TestGetHostName("ijk://abc/xyz", kLocalPathFlags, ""));
}

TEST(PathTest, IsPathAbsolute) {
  EXPECT_FALSE(IsPathAbsolute(""));
  EXPECT_FALSE(IsPathAbsolute(":"));
  EXPECT_FALSE(IsPathAbsolute("abc:"));
  EXPECT_TRUE(IsPathAbsolute("abc:/"));
  EXPECT_FALSE(IsPathAbsolute("xyz"));
  EXPECT_TRUE(IsPathAbsolute("/xyz"));
  EXPECT_FALSE(IsPathAbsolute(":xyz"));
  EXPECT_FALSE(IsPathAbsolute(":/xyz"));
  EXPECT_FALSE(IsPathAbsolute("abc:xyz"));
  EXPECT_TRUE(IsPathAbsolute("abc:/xyz"));
  EXPECT_FALSE(IsPathAbsolute("ABC:xyz"));
  EXPECT_FALSE(IsPathAbsolute("ABC:/xyz"));
  EXPECT_FALSE(IsPathAbsolute("abc:xyz:123"));
  EXPECT_TRUE(IsPathAbsolute("abc:/xyz:123"));
  EXPECT_TRUE(IsPathAbsolute("http://xyz.com:123/a/b/c?q=test#frag"));
  EXPECT_TRUE(IsPathAbsolute("//xyz.com:123"));
}

bool TestJoinPath(const char* path_a, const char* path_b,
                  const std::string& result,
                  const std::string& protocol_only_result,
                  const std::string& host_only_result,
                  const std::string local_result) {
  std::string result_string;

  result_string = JoinPath(path_a, path_b);
  TEST_PATH_EXPECT(result_string, result);

  result_string = JoinPath(path_a, path_b, kGenericPathFlags);
  TEST_PATH_EXPECT(result_string, result);

  result_string = JoinPath(path_a, path_b, PathFlag::kAllowProtocol);
  TEST_PATH_EXPECT(result_string, protocol_only_result);

  result_string = JoinPath(path_a, path_b, PathFlag::kRequireProtocol);
  TEST_PATH_EXPECT(result_string, protocol_only_result);

  result_string = JoinPath(path_a, path_b, PathFlag::kAllowHost);
  TEST_PATH_EXPECT(result_string, host_only_result);

  result_string = JoinPath(path_a, path_b, PathFlag::kRequireHost);
  TEST_PATH_EXPECT(result_string, host_only_result);

  result_string = JoinPath(path_a, path_b, kLocalPathFlags);
  TEST_PATH_EXPECT(result_string, local_result);

  return true;
}

TEST(PathTest, JoinPath) {
  // clang-format off
  EXPECT_TRUE(TestJoinPath("", "", "", "", "", ""));
  EXPECT_TRUE(TestJoinPath("a", "", "a", "a", "a", "a"));
  EXPECT_TRUE(TestJoinPath("", "b", "b", "b", "b", "b"));
  EXPECT_TRUE(TestJoinPath("/", "", "/", "/", "/", "/"));
  EXPECT_TRUE(TestJoinPath("", "/", "/", "/", "/", "/"));
  EXPECT_TRUE(TestJoinPath("/", "/", "/", "/", "/", "/"));
  EXPECT_TRUE(TestJoinPath("a/", "", "a/", "a/", "a/", "a/"));
  EXPECT_TRUE(TestJoinPath("", "b/", "b/", "b/", "b/", "b/"));
  EXPECT_TRUE(TestJoinPath("a/", "b/", "a/b/", "a/b/", "a/b/", "a/b/"));
  EXPECT_TRUE(TestJoinPath("/a", "", "/a", "/a", "/a", "/a"));
  EXPECT_TRUE(TestJoinPath("", "/b", "/b", "/b", "/b", "/b"));
  EXPECT_TRUE(TestJoinPath("/a", "/b", "/a/b", "/a/b", "/a/b", "/a/b"));
  EXPECT_TRUE(TestJoinPath("//a", "", "//a", "//a", "//a", "//a"));
  EXPECT_TRUE(TestJoinPath("", "//b", "//b", "//b", "//b", "//b"));
  EXPECT_TRUE(TestJoinPath("//a", "//b", "", "//a//b", "", "//a//b"));
  EXPECT_TRUE(TestJoinPath("//a", "c", "//a/c", "//a/c", "//a/c", "//a/c"));
  EXPECT_TRUE(TestJoinPath("c", "//b", "//b/c", "c//b", "//b/c", "c//b"));
  EXPECT_TRUE(TestJoinPath("//a/", "c", "//a/c", "//a/c", "//a/c", "//a/c"));
  EXPECT_TRUE(TestJoinPath("c", "//b/", "//b/c", "c//b/", "//b/c", "c//b/"));
  EXPECT_TRUE(TestJoinPath("//a/x", "c", "//a/x/c", "//a/x/c", "//a/x/c", "//a/x/c"));
  EXPECT_TRUE(TestJoinPath("c", "//b/y", "//b/c/y", "c//b/y", "//b/c/y", "c//b/y"));
  EXPECT_TRUE(TestJoinPath("//a", "//a", "//a", "//a//a", "//a", "//a//a"));
  EXPECT_TRUE(TestJoinPath("//a/", "//a", "//a/", "//a//a", "//a/", "//a//a"));
  EXPECT_TRUE(TestJoinPath("//a", "//a/", "//a/", "//a//a/", "//a/", "//a//a/"));
  EXPECT_TRUE(TestJoinPath("//a/b", "//a", "//a/b", "//a/b//a", "//a/b", "//a/b//a"));
  EXPECT_TRUE(TestJoinPath("//a", "//a/b", "//a/b", "//a//a/b", "//a/b", "//a//a/b"));
  EXPECT_TRUE(TestJoinPath("//a/b", "//a/", "//a/b", "//a/b//a/", "//a/b", "//a/b//a/"));
  EXPECT_TRUE(TestJoinPath("//a/", "//a/b", "//a/b", "//a//a/b", "//a/b", "//a//a/b"));
  EXPECT_TRUE(TestJoinPath("//a/b", "//a/c", "//a/b/c", "//a/b//a/c", "//a/b/c", "//a/b//a/c"));
  EXPECT_TRUE(TestJoinPath("abc:", "", "abc:", "abc:", "abc:", "abc:"));
  EXPECT_TRUE(TestJoinPath("abc:a", "", "abc:a", "abc:a", "abc:a", "abc:a"));
  EXPECT_TRUE(TestJoinPath("abc:", "b", "abc:b", "abc:b", "abc:/b", "abc:/b"));
  EXPECT_TRUE(TestJoinPath("abc:/", "", "abc:/", "abc:/", "abc:/", "abc:/"));
  EXPECT_TRUE(TestJoinPath("abc:", "/", "abc:/", "abc:/", "abc:", "abc:"));
  EXPECT_TRUE(TestJoinPath("abc:/", "/", "abc:/", "abc:/", "abc:/", "abc:/"));
  EXPECT_TRUE(TestJoinPath("abc:a/", "", "abc:a/", "abc:a/", "abc:a/", "abc:a/"));
  EXPECT_TRUE(TestJoinPath("abc:", "b/", "abc:b/", "abc:b/", "abc:/b/", "abc:/b/"));
  EXPECT_TRUE(TestJoinPath("abc:a/", "b/", "abc:a/b/", "abc:a/b/", "abc:a/b/", "abc:a/b/"));
  EXPECT_TRUE(TestJoinPath("abc:/a", "", "abc:/a", "abc:/a", "abc:/a", "abc:/a"));
  EXPECT_TRUE(TestJoinPath("abc:", "/b", "abc:/b", "abc:/b", "abc:/b", "abc:/b"));
  EXPECT_TRUE(TestJoinPath("abc:/a", "/b", "abc:/a/b", "abc:/a/b", "abc:/a/b", "abc:/a/b"));
  EXPECT_TRUE(TestJoinPath("abc://a", "", "abc://a", "abc://a", "abc://a", "abc://a"));
  EXPECT_TRUE(TestJoinPath("abc:", "//b", "abc://b", "abc://b", "//b/abc:", "abc://b"));
  EXPECT_TRUE(TestJoinPath("abc://a", "//b", "", "abc://a//b", "//b/abc://a", "abc://a//b"));
  EXPECT_TRUE(TestJoinPath("abc://a", "c", "abc://a/c", "abc://a/c", "abc://a/c", "abc://a/c"));
  EXPECT_TRUE(TestJoinPath("abc:c", "//b", "abc://b/c", "abc:c//b", "//b/abc:c", "abc:c//b"));
  EXPECT_TRUE(TestJoinPath("abc://a/", "c", "abc://a/c", "abc://a/c", "abc://a/c", "abc://a/c"));
  EXPECT_TRUE(TestJoinPath("abc:c", "//b/", "abc://b/c", "abc:c//b/", "//b/abc:c", "abc:c//b/"));
  EXPECT_TRUE(TestJoinPath("abc://a/x", "c", "abc://a/x/c", "abc://a/x/c", "abc://a/x/c", "abc://a/x/c"));
  EXPECT_TRUE(TestJoinPath("abc:c", "//b/y", "abc://b/c/y", "abc:c//b/y", "//b/abc:c/y", "abc:c//b/y"));
  EXPECT_TRUE(TestJoinPath("abc://a", "//a", "abc://a", "abc://a//a", "//a/abc://a", "abc://a//a"));
  EXPECT_TRUE(TestJoinPath("abc://a/", "//a", "abc://a/", "abc://a//a", "//a/abc://a/", "abc://a//a"));
  EXPECT_TRUE(TestJoinPath("abc://a", "//a/", "abc://a/", "abc://a//a/", "//a/abc://a", "abc://a//a/"));
  EXPECT_TRUE(TestJoinPath("abc://a/b", "//a", "abc://a/b", "abc://a/b//a", "//a/abc://a/b", "abc://a/b//a"));
  EXPECT_TRUE(TestJoinPath("abc://a", "//a/b", "abc://a/b", "abc://a//a/b", "//a/abc://a/b", "abc://a//a/b"));
  EXPECT_TRUE(TestJoinPath("abc://a/b", "//a/", "abc://a/b", "abc://a/b//a/", "//a/abc://a/b", "abc://a/b//a/"));
  EXPECT_TRUE(TestJoinPath("abc://a/", "//a/b", "abc://a/b", "abc://a//a/b", "//a/abc://a/b", "abc://a//a/b"));
  EXPECT_TRUE(TestJoinPath("abc://a/b", "//a/c", "abc://a/b/c", "abc://a/b//a/c", "//a/abc://a/b/c", "abc://a/b//a/c"));
  EXPECT_TRUE(TestJoinPath("", "xyz:", "xyz:", "xyz:", "xyz:", "xyz:"));
  EXPECT_TRUE(TestJoinPath("a", "xyz:", "xyz:a", "xyz:a", "a/xyz:", "a/xyz:"));
  EXPECT_TRUE(TestJoinPath("", "xyz:b", "xyz:b", "xyz:b", "xyz:b", "xyz:b"));
  EXPECT_TRUE(TestJoinPath("/", "xyz:", "xyz:/", "xyz:/", "/xyz:", "/xyz:"));
  EXPECT_TRUE(TestJoinPath("", "xyz:/", "xyz:/", "xyz:/", "xyz:/", "xyz:/"));
  EXPECT_TRUE(TestJoinPath("/", "xyz:/", "xyz:/", "xyz:/", "/xyz:/", "/xyz:/"));
  EXPECT_TRUE(TestJoinPath("a/", "xyz:", "xyz:a/", "xyz:a/", "a/xyz:", "a/xyz:"));
  EXPECT_TRUE(TestJoinPath("", "xyz:b/", "xyz:b/", "xyz:b/", "xyz:b/", "xyz:b/"));
  EXPECT_TRUE(TestJoinPath("a/", "xyz:b/", "xyz:a/b/", "xyz:a/b/", "a/xyz:b/", "a/xyz:b/"));
  EXPECT_TRUE(TestJoinPath("/a", "xyz:", "xyz:/a", "xyz:/a", "/a/xyz:", "/a/xyz:"));
  EXPECT_TRUE(TestJoinPath("", "xyz:/b", "xyz:/b", "xyz:/b", "xyz:/b", "xyz:/b"));
  EXPECT_TRUE(TestJoinPath("/a", "xyz:/b", "xyz:/a/b", "xyz:/a/b", "/a/xyz:/b", "/a/xyz:/b"));
  EXPECT_TRUE(TestJoinPath("//a", "xyz:", "xyz://a", "xyz://a", "//a/xyz:", "//a/xyz:"));
  EXPECT_TRUE(TestJoinPath("", "xyz://b", "xyz://b", "xyz://b", "xyz://b", "xyz://b"));
  EXPECT_TRUE(TestJoinPath("//a", "xyz://b", "", "xyz://a//b", "//a/xyz://b", "//a/xyz://b"));
  EXPECT_TRUE(TestJoinPath("//a", "xyz:c", "xyz://a/c", "xyz://a/c", "//a/xyz:c", "//a/xyz:c"));
  EXPECT_TRUE(TestJoinPath("c", "xyz://b", "xyz://b/c", "xyz:c//b", "c/xyz://b", "c/xyz://b"));
  EXPECT_TRUE(TestJoinPath("//a/", "xyz:c", "xyz://a/c", "xyz://a/c", "//a/xyz:c", "//a/xyz:c"));
  EXPECT_TRUE(TestJoinPath("c", "xyz://b/", "xyz://b/c", "xyz:c//b/", "c/xyz://b/", "c/xyz://b/"));
  EXPECT_TRUE(TestJoinPath("//a/x", "xyz:c", "xyz://a/x/c", "xyz://a/x/c", "//a/x/xyz:c", "//a/x/xyz:c"));
  EXPECT_TRUE(TestJoinPath("c", "xyz://b/y", "xyz://b/c/y", "xyz:c//b/y", "c/xyz://b/y", "c/xyz://b/y"));
  EXPECT_TRUE(TestJoinPath("//a", "xyz://a", "xyz://a", "xyz://a//a", "//a/xyz://a", "//a/xyz://a"));
  EXPECT_TRUE(TestJoinPath("//a/", "xyz://a", "xyz://a/", "xyz://a//a", "//a/xyz://a", "//a/xyz://a"));
  EXPECT_TRUE(TestJoinPath("//a", "xyz://a/", "xyz://a/", "xyz://a//a/", "//a/xyz://a/", "//a/xyz://a/"));
  EXPECT_TRUE(TestJoinPath("//a/b", "xyz://a", "xyz://a/b", "xyz://a/b//a", "//a/b/xyz://a", "//a/b/xyz://a"));
  EXPECT_TRUE(TestJoinPath("//a", "xyz://a/b", "xyz://a/b", "xyz://a//a/b", "//a/xyz://a/b", "//a/xyz://a/b"));
  EXPECT_TRUE(TestJoinPath("//a/b", "xyz://a/", "xyz://a/b", "xyz://a/b//a/", "//a/b/xyz://a/", "//a/b/xyz://a/"));
  EXPECT_TRUE(TestJoinPath("//a/", "xyz://a/b", "xyz://a/b", "xyz://a//a/b", "//a/xyz://a/b", "//a/xyz://a/b"));
  EXPECT_TRUE(TestJoinPath("//a/b", "xyz://a/c", "xyz://a/b/c", "xyz://a/b//a/c", "//a/b/xyz://a/c", "//a/b/xyz://a/c"));
  EXPECT_TRUE(TestJoinPath("abc:", "xyz:", "", "", "abc:/xyz:", "abc:/xyz:"));
  EXPECT_TRUE(TestJoinPath("abc:", "abc:", "abc:", "abc:", "abc:/abc:", "abc:/abc:"));
  EXPECT_TRUE(TestJoinPath("abc:a", "abc:", "abc:a", "abc:a", "abc:a/abc:", "abc:a/abc:"));
  EXPECT_TRUE(TestJoinPath("abc:", "abc:b", "abc:b", "abc:b", "abc:/abc:b", "abc:/abc:b"));
  EXPECT_TRUE(TestJoinPath("abc:/", "abc:", "abc:/", "abc:/", "abc:/abc:", "abc:/abc:"));
  EXPECT_TRUE(TestJoinPath("abc:", "abc:/", "abc:/", "abc:/", "abc:/abc:/", "abc:/abc:/"));
  EXPECT_TRUE(TestJoinPath("abc:/", "abc:/", "abc:/", "abc:/", "abc:/abc:/", "abc:/abc:/"));
  EXPECT_TRUE(TestJoinPath("abc:a/", "abc:", "abc:a/", "abc:a/", "abc:a/abc:", "abc:a/abc:"));
  EXPECT_TRUE(TestJoinPath("abc:", "abc:b/", "abc:b/", "abc:b/", "abc:/abc:b/", "abc:/abc:b/"));
  EXPECT_TRUE(TestJoinPath("abc:a/", "abc:b/", "abc:a/b/", "abc:a/b/", "abc:a/abc:b/", "abc:a/abc:b/"));
  EXPECT_TRUE(TestJoinPath("abc:/a", "abc:", "abc:/a", "abc:/a", "abc:/a/abc:", "abc:/a/abc:"));
  EXPECT_TRUE(TestJoinPath("abc:", "abc:/b", "abc:/b", "abc:/b", "abc:/abc:/b", "abc:/abc:/b"));
  EXPECT_TRUE(TestJoinPath("abc:/a", "abc:/b", "abc:/a/b", "abc:/a/b", "abc:/a/abc:/b", "abc:/a/abc:/b"));
  EXPECT_TRUE(TestJoinPath("abc://a", "abc:", "abc://a", "abc://a", "abc://a/abc:", "abc://a/abc:"));
  EXPECT_TRUE(TestJoinPath("abc:", "abc://b", "abc://b", "abc://b", "abc:/abc://b", "abc:/abc://b"));
  EXPECT_TRUE(TestJoinPath("abc://a", "abc://b", "", "abc://a//b", "abc://a/abc://b", "abc://a/abc://b"));
  EXPECT_TRUE(TestJoinPath("abc://a", "abc:c", "abc://a/c", "abc://a/c", "abc://a/abc:c", "abc://a/abc:c"));
  EXPECT_TRUE(TestJoinPath("abc:c", "abc://b", "abc://b/c", "abc:c//b", "abc:c/abc://b", "abc:c/abc://b"));
  EXPECT_TRUE(TestJoinPath("abc://a/", "abc:c", "abc://a/c", "abc://a/c", "abc://a/abc:c", "abc://a/abc:c"));
  EXPECT_TRUE(TestJoinPath("abc:c", "abc://b/", "abc://b/c", "abc:c//b/", "abc:c/abc://b/", "abc:c/abc://b/"));
  EXPECT_TRUE(TestJoinPath("abc://a/x", "abc:c", "abc://a/x/c", "abc://a/x/c", "abc://a/x/abc:c", "abc://a/x/abc:c"));
  EXPECT_TRUE(TestJoinPath("abc:c", "abc://b/y", "abc://b/c/y", "abc:c//b/y", "abc:c/abc://b/y", "abc:c/abc://b/y"));
  EXPECT_TRUE(TestJoinPath("abc://a", "abc://a", "abc://a", "abc://a//a", "abc://a/abc://a", "abc://a/abc://a"));
  EXPECT_TRUE(TestJoinPath("abc://a/", "abc://a", "abc://a/", "abc://a//a", "abc://a/abc://a", "abc://a/abc://a"));
  EXPECT_TRUE(TestJoinPath("abc://a", "abc://a/", "abc://a/", "abc://a//a/", "abc://a/abc://a/", "abc://a/abc://a/"));
  EXPECT_TRUE(TestJoinPath("abc://a/b", "abc://a", "abc://a/b", "abc://a/b//a", "abc://a/b/abc://a", "abc://a/b/abc://a"));
  EXPECT_TRUE(TestJoinPath("abc://a", "abc://a/b", "abc://a/b", "abc://a//a/b", "abc://a/abc://a/b", "abc://a/abc://a/b"));
  EXPECT_TRUE(TestJoinPath("abc://a/b", "abc://a/", "abc://a/b", "abc://a/b//a/", "abc://a/b/abc://a/", "abc://a/b/abc://a/"));
  EXPECT_TRUE(TestJoinPath("abc://a/", "abc://a/b", "abc://a/b", "abc://a//a/b", "abc://a/abc://a/b", "abc://a/abc://a/b"));
  EXPECT_TRUE(TestJoinPath("abc://a/b", "abc://a/c", "abc://a/b/c", "abc://a/b//a/c", "abc://a/b/abc://a/c", "abc://a/b/abc://a/c"));
  // clang-format on
}

TEST(PathTest, PathMatchesPattern) {
  EXPECT_TRUE(PathMatchesPattern("", ""));
  EXPECT_TRUE(PathMatchesPattern("a", "a"));
  EXPECT_FALSE(PathMatchesPattern("ab", "a"));
  EXPECT_FALSE(PathMatchesPattern("a", "ab"));
  EXPECT_FALSE(PathMatchesPattern("ac", "ab"));
  EXPECT_TRUE(PathMatchesPattern("a", "a*"));
  EXPECT_TRUE(PathMatchesPattern("ab", "a*"));
  EXPECT_TRUE(PathMatchesPattern("abc", "a*"));
  EXPECT_FALSE(PathMatchesPattern("a", "a*x"));
  EXPECT_TRUE(PathMatchesPattern("ax", "a*x"));
  EXPECT_TRUE(PathMatchesPattern("ax", "a*x"));
  EXPECT_FALSE(PathMatchesPattern("ax", "a*y"));
  EXPECT_TRUE(PathMatchesPattern("abx", "a*x"));
  EXPECT_TRUE(PathMatchesPattern("abcx", "a*x"));
  EXPECT_FALSE(PathMatchesPattern("axy", "a*x"));
  EXPECT_TRUE(PathMatchesPattern("axy", "a*x*"));
  EXPECT_TRUE(PathMatchesPattern("axyz", "a*x*"));
  EXPECT_TRUE(PathMatchesPattern("a", "a**"));
  EXPECT_TRUE(PathMatchesPattern("ab", "a**"));
  EXPECT_TRUE(PathMatchesPattern("abc", "a**"));
  EXPECT_FALSE(PathMatchesPattern("a", "a**x"));
  EXPECT_TRUE(PathMatchesPattern("ax", "a**x"));
  EXPECT_TRUE(PathMatchesPattern("ax", "a**x"));
  EXPECT_FALSE(PathMatchesPattern("ax", "a**y"));
  EXPECT_TRUE(PathMatchesPattern("abx", "a**x"));
  EXPECT_TRUE(PathMatchesPattern("abcx", "a**x"));
  EXPECT_FALSE(PathMatchesPattern("axy", "a**x"));
  EXPECT_TRUE(PathMatchesPattern("axy", "a**x*"));
  EXPECT_TRUE(PathMatchesPattern("axyz", "a**x*"));
  EXPECT_TRUE(PathMatchesPattern("file:/assets/textures/en/image.png",
                                 "*/textures/*/*.png"));
}

bool TestNormalize(std::string_view path, PathFlags flags,
                   const std::string& expected_path,
                   PathFlags expected_failed_flag) {
  PathFlags failed_flag;
  failed_flag += PathFlag::kRequireLowercase;
  NormalizePath(path, flags, nullptr);
  auto normalized_path = NormalizePath(path, flags, &failed_flag);
  EXPECT_EQ(normalized_path, expected_path);
  EXPECT_EQ(failed_flag, expected_failed_flag);
  return normalized_path == expected_path &&
         failed_flag == expected_failed_flag;
}

TEST(PathTest, NormalizePath) {
  // clang-format off
  EXPECT_TRUE(TestNormalize("", {}, "", {}));
  EXPECT_TRUE(TestNormalize(":", {}, ":", {}));
  EXPECT_TRUE(TestNormalize("/", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("//", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("///", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("a", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", {}, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("a/", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("a//", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("a///", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("a\\", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("a\\\\", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("a\\/\\", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("/a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("//a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("///a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("\\a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("\\\\a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("\\/\\a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("a/b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("a//b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("a///b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("a\\b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("a\\\\b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("a\\/\\b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", {}, "PROTOCOL:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", {}, "PROTOCOL:/a/b", {}));
  EXPECT_TRUE(TestNormalize("./a", {}, "a", {}));
  EXPECT_TRUE(TestNormalize("./a/././b", {}, "a/b", {}));
  EXPECT_TRUE(TestNormalize("a/../b", {}, "b", {}));
  EXPECT_TRUE(TestNormalize("a/../../b", {}, "../b", {}));
  EXPECT_TRUE(TestNormalize("a/../../../b/../c", {}, "../../c", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/../b", {}, "b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/../../b", {}, "../b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/../../b", {}, "b", {}));
  EXPECT_TRUE(TestNormalize("abc/def/ghi/../../xyz", {}, "abc/xyz", {}));
  EXPECT_TRUE(TestNormalize("abc/def/./ghi/.././../xyz", {}, "abc/xyz", {}));
  EXPECT_TRUE(TestNormalize("abc/def/.//ghi/..///.////../xyz", {}, "abc/xyz", {}));
  EXPECT_TRUE(TestNormalize("abc/def/./\\ghi/../\\/.\\\\\\..\\xyz", {}, "abc/xyz", {}));
  EXPECT_TRUE(TestNormalize("/./a", {}, "/a", {}));
  EXPECT_TRUE(TestNormalize("/./a/././b", {}, "/a/b", {}));
  EXPECT_TRUE(TestNormalize("/a/../b", {}, "/b", {}));
  EXPECT_TRUE(TestNormalize("/a/../../b", {}, "/../b", {}));
  EXPECT_TRUE(TestNormalize("/.", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("/./", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("/..", {}, "/..", {}));
  EXPECT_TRUE(TestNormalize("/../", {}, "/..", {}));
  EXPECT_TRUE(TestNormalize("/a/..", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/../", {}, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/../..", {}, "/..", {}));
  EXPECT_TRUE(TestNormalize("/a/../../", {}, "/..", {}));
  EXPECT_TRUE(TestNormalize("/a/../b/c/../../../..", {}, "/../..", {}));
  EXPECT_TRUE(TestNormalize(".", {}, "", {}));
  EXPECT_TRUE(TestNormalize("./", {}, "", {}));
  EXPECT_TRUE(TestNormalize("..", {}, "..", {}));
  EXPECT_TRUE(TestNormalize("../", {}, "..", {}));
  EXPECT_TRUE(TestNormalize("a/..", {}, "", {}));
  EXPECT_TRUE(TestNormalize("a/../", {}, "", {}));
  EXPECT_TRUE(TestNormalize("a/../..", {}, "..", {}));
  EXPECT_TRUE(TestNormalize("a/../../", {}, "..", {}));
  EXPECT_TRUE(TestNormalize("a/../b/c/../../../..", {}, "../..", {}));

  EXPECT_TRUE(TestNormalize("", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("//", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("///", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/b/c", PathFlag::kRequireRoot, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("MixedCase/", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kRequireRoot, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", PathFlag::kRequireRoot, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", PathFlag::kRequireRoot, "/Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/.", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("/./", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("/..", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/../", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/a/..", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/../", PathFlag::kRequireRoot, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/../..", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/a/../../", PathFlag::kRequireRoot, "", PathFlag::kRequireRoot));

  EXPECT_TRUE(TestNormalize("", PathFlag::kAllowProtocol, "", {}));
  EXPECT_TRUE(TestNormalize("/", PathFlag::kAllowProtocol, "/", {}));
  EXPECT_TRUE(TestNormalize("//", PathFlag::kAllowProtocol, "/", {}));
  EXPECT_TRUE(TestNormalize("///", PathFlag::kAllowProtocol, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/b/c", PathFlag::kAllowProtocol, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kAllowProtocol, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", PathFlag::kAllowProtocol, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kAllowProtocol, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", PathFlag::kAllowProtocol, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", PathFlag::kAllowProtocol, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", PathFlag::kAllowProtocol, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", PathFlag::kAllowProtocol, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", PathFlag::kAllowProtocol, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", PathFlag::kAllowProtocol, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", PathFlag::kAllowProtocol, "protocol:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kAllowProtocol, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", PathFlag::kAllowProtocol, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", PathFlag::kAllowProtocol, "/Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kAllowProtocol, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", PathFlag::kAllowProtocol, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", PathFlag::kAllowProtocol, "protocol:/Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("aBcDeFgHiJkLmNoPqRsTuVwXyZ1234567890:", PathFlag::kAllowProtocol, "abcdefghijklmnopqrstuvwxyz1234567890:", {}));
  EXPECT_TRUE(TestNormalize("proto-call:", PathFlag::kAllowProtocol, "", PathFlag::kAllowProtocol));
  EXPECT_TRUE(TestNormalize("proto-call:/a/b", PathFlag::kAllowProtocol, "", PathFlag::kAllowProtocol));
  EXPECT_TRUE(TestNormalize(":", PathFlag::kAllowProtocol, "", PathFlag::kAllowProtocol));

  EXPECT_TRUE(TestNormalize("", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/", {}));
  EXPECT_TRUE(TestNormalize("//", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/", {}));
  EXPECT_TRUE(TestNormalize("///", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/", {}));
  EXPECT_TRUE(TestNormalize("/a/b/c", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("MixedCase/", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "/Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", {PathFlag::kAllowProtocol, PathFlag::kRequireRoot}, "protocol:/Host/a/b", {}));

  EXPECT_TRUE(TestNormalize("", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("/", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("///", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("/a/b/c", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("MixedCase/", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kRequireProtocol, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", PathFlag::kRequireProtocol, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", PathFlag::kRequireProtocol, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", PathFlag::kRequireProtocol, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", PathFlag::kRequireProtocol, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", PathFlag::kRequireProtocol, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", PathFlag::kRequireProtocol, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", PathFlag::kRequireProtocol, "protocol:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//Host/", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//Host/a/b", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kRequireProtocol, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", PathFlag::kRequireProtocol, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", PathFlag::kRequireProtocol, "protocol:/Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("aBcDeFgHiJkLmNoPqRsTuVwXyZ1234567890:", PathFlag::kRequireProtocol, "abcdefghijklmnopqrstuvwxyz1234567890:", {}));
  EXPECT_TRUE(TestNormalize("proto-call:", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("proto-call:/a/b", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize(":", PathFlag::kRequireProtocol, "", PathFlag::kRequireProtocol));

  EXPECT_TRUE(TestNormalize("", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("/", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("///", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("/a/b/c", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("MixedCase", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("MixedCase/", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", kProtocolPathFlags, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", kProtocolPathFlags, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", kProtocolPathFlags, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", kProtocolPathFlags, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", kProtocolPathFlags, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", kProtocolPathFlags, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", kProtocolPathFlags, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", kProtocolPathFlags, "protocol:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//Host/", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//Host/a/b", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", kProtocolPathFlags, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", kProtocolPathFlags, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", kProtocolPathFlags, "protocol:/Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("aBcDeFgHiJkLmNoPqRsTuVwXyZ1234567890:", kProtocolPathFlags, "abcdefghijklmnopqrstuvwxyz1234567890:", {}));
  EXPECT_TRUE(TestNormalize("proto-call:", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("proto-call:/a/b", kProtocolPathFlags, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize(":", kProtocolPathFlags, "", PathFlag::kRequireProtocol));

  EXPECT_TRUE(TestNormalize("", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("/", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("///", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("/a/b/c", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("MixedCase", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("MixedCase/", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//Host/", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("//Host/a/b", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "", PathFlag::kRequireProtocol));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", {PathFlag::kRequireProtocol, PathFlag::kRequireRoot}, "protocol:/Host/a/b", {}));

  EXPECT_TRUE(TestNormalize("", PathFlag::kAllowHost, "", {}));
  EXPECT_TRUE(TestNormalize("/", PathFlag::kAllowHost, "/", {}));
  EXPECT_TRUE(TestNormalize("//", PathFlag::kAllowHost, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("///", PathFlag::kAllowHost, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", PathFlag::kAllowHost, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kAllowHost, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", PathFlag::kAllowHost, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kAllowHost, "PROTOCOL:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", PathFlag::kAllowHost, "PROTOCOL:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", PathFlag::kAllowHost, "PROTOCOL:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", PathFlag::kAllowHost, "PROTOCOL:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", PathFlag::kAllowHost, "PROTOCOL:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", PathFlag::kAllowHost, "PROTOCOL:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", PathFlag::kAllowHost, "PROTOCOL:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", PathFlag::kAllowHost, "PROTOCOL:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kAllowHost, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", PathFlag::kAllowHost, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/.", PathFlag::kAllowHost, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/..", PathFlag::kAllowHost, "//Host/..", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", PathFlag::kAllowHost, "//Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kAllowHost, "PROTOCOL:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", PathFlag::kAllowHost, "PROTOCOL:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", PathFlag::kAllowHost, "PROTOCOL:/Host/a/b", {}));

  EXPECT_TRUE(TestNormalize("", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "/", {}));
  EXPECT_TRUE(TestNormalize("//", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("///", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("MixedCase/", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/.", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/..", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host/a/b", {PathFlag::kAllowHost, PathFlag::kRequireRoot}, "//Host/a/b", {}));

  EXPECT_TRUE(TestNormalize("", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("/", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("//", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("///", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kRequireHost, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kRequireHost, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", PathFlag::kRequireHost, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/.", PathFlag::kRequireHost, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/..", PathFlag::kRequireHost, "//Host/..", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", PathFlag::kRequireHost, "//Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kRequireHost, "", PathFlag::kRequireHost));

  EXPECT_TRUE(TestNormalize("", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("/", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("//", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("///", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("MixedCase", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", kHostPathFlags, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("//Host", kHostPathFlags, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", kHostPathFlags, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/.", kHostPathFlags, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/..", kHostPathFlags, "//Host/..", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", kHostPathFlags, "//Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", kHostPathFlags, "", PathFlag::kRequireHost));

  EXPECT_TRUE(TestNormalize("", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("/", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("//", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("///", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("MixedCase", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));
  EXPECT_TRUE(TestNormalize("//Host", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/.", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/..", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host/a/b", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "//Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", {PathFlag::kRequireHost, PathFlag::kRequireRoot}, "", PathFlag::kRequireHost));

  EXPECT_TRUE(TestNormalize("", kGenericPathFlags, "", {}));
  EXPECT_TRUE(TestNormalize("/", kGenericPathFlags, "/", {}));
  EXPECT_TRUE(TestNormalize("//", kGenericPathFlags, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("///", kGenericPathFlags, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", kGenericPathFlags, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", kGenericPathFlags, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", kGenericPathFlags, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", kGenericPathFlags, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", kGenericPathFlags, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", kGenericPathFlags, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", kGenericPathFlags, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:.", kGenericPathFlags, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:..", kGenericPathFlags, "protocol:..", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/.", kGenericPathFlags, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/..", kGenericPathFlags, "protocol:/..", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", kGenericPathFlags, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", kGenericPathFlags, "protocol:a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", kGenericPathFlags, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", kGenericPathFlags, "protocol:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host", kGenericPathFlags, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", kGenericPathFlags, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/.", kGenericPathFlags, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/..", kGenericPathFlags, "//Host/..", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", kGenericPathFlags, "//Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", kGenericPathFlags, "protocol://Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", kGenericPathFlags, "protocol://Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/.", kGenericPathFlags, "protocol://Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/..", kGenericPathFlags, "protocol://Host/..", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", kGenericPathFlags, "protocol://Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("proto-call:", kGenericPathFlags, "", PathFlag::kAllowProtocol));
  EXPECT_TRUE(TestNormalize("proto-call:/a/b", kGenericPathFlags, "", PathFlag::kAllowProtocol));
  EXPECT_TRUE(TestNormalize(":", kGenericPathFlags, "", PathFlag::kAllowProtocol));

  EXPECT_TRUE(TestNormalize("", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("/", {kGenericPathFlags, PathFlag::kRequireRoot}, "/", {}));
  EXPECT_TRUE(TestNormalize("//", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("///", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("/a/b/c", {kGenericPathFlags, PathFlag::kRequireRoot}, "/a/b/c", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("MixedCase/", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", {kGenericPathFlags, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:///", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowHost));
  EXPECT_TRUE(TestNormalize("PROTOCOL:.", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:..", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/.", {kGenericPathFlags, PathFlag::kRequireRoot}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/..", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:a/b/", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/a/b", {kGenericPathFlags, PathFlag::kRequireRoot}, "protocol:/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kRequireRoot));
  EXPECT_TRUE(TestNormalize("//Host", {kGenericPathFlags, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", {kGenericPathFlags, PathFlag::kRequireRoot}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/a/b", {kGenericPathFlags, PathFlag::kRequireRoot}, "//Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", {kGenericPathFlags, PathFlag::kRequireRoot}, "protocol://Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", {kGenericPathFlags, PathFlag::kRequireRoot}, "protocol://Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/a/b", {kGenericPathFlags, PathFlag::kRequireRoot}, "protocol://Host/a/b", {}));
  EXPECT_TRUE(TestNormalize("proto-call:", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowProtocol));
  EXPECT_TRUE(TestNormalize("proto-call:/a/b", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowProtocol));
  EXPECT_TRUE(TestNormalize(":", {kGenericPathFlags, PathFlag::kRequireRoot}, "", PathFlag::kAllowProtocol));

  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kRequireLowercase, "mixedcase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", PathFlag::kRequireLowercase, "mixedcase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kRequireLowercase, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", PathFlag::kRequireLowercase, "protocol:mixedcase", {}));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kRequireLowercase, "/host", {}));
  EXPECT_TRUE(TestNormalize("//Host/MixedCase", PathFlag::kRequireLowercase, "/host/mixedcase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kRequireLowercase, "protocol:/host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/MixedCase", PathFlag::kRequireLowercase, "protocol:/host/mixedcase", {}));

  EXPECT_TRUE(TestNormalize("MixedCase", {kGenericPathFlags, PathFlag::kRequireLowercase}, "mixedcase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", {kGenericPathFlags, PathFlag::kRequireLowercase}, "mixedcase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {kGenericPathFlags, PathFlag::kRequireLowercase}, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", {kGenericPathFlags, PathFlag::kRequireLowercase}, "protocol:mixedcase", {}));
  EXPECT_TRUE(TestNormalize("//Host", {kGenericPathFlags, PathFlag::kRequireLowercase}, "//host", {}));
  EXPECT_TRUE(TestNormalize("//Host/MixedCase", {kGenericPathFlags, PathFlag::kRequireLowercase}, "//host/mixedcase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", {kGenericPathFlags, PathFlag::kRequireLowercase}, "protocol://host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/MixedCase", {kGenericPathFlags, PathFlag::kRequireLowercase}, "protocol://host/mixedcase", {}));

  EXPECT_TRUE(TestNormalize("/", PathFlag::kAllowTrailingSlash, "/", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", PathFlag::kAllowTrailingSlash, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", PathFlag::kAllowTrailingSlash, "MixedCase/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", PathFlag::kAllowTrailingSlash, "PROTOCOL:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", PathFlag::kAllowTrailingSlash, "PROTOCOL:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", PathFlag::kAllowTrailingSlash, "PROTOCOL:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase/", PathFlag::kAllowTrailingSlash, "PROTOCOL:MixedCase/", {}));
  EXPECT_TRUE(TestNormalize("//Host", PathFlag::kAllowTrailingSlash, "/Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", PathFlag::kAllowTrailingSlash, "/Host/", {}));
  EXPECT_TRUE(TestNormalize("//Host/MixedCase", PathFlag::kAllowTrailingSlash, "/Host/MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host/MixedCase/", PathFlag::kAllowTrailingSlash, "/Host/MixedCase/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", PathFlag::kAllowTrailingSlash, "PROTOCOL:/Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", PathFlag::kAllowTrailingSlash, "PROTOCOL:/Host/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/MixedCase", PathFlag::kAllowTrailingSlash, "PROTOCOL:/Host/MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/MixedCase/", PathFlag::kAllowTrailingSlash, "PROTOCOL:/Host/MixedCase/", {}));

  EXPECT_TRUE(TestNormalize("/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "/", {}));
  EXPECT_TRUE(TestNormalize("MixedCase", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "MixedCase", {}));
  EXPECT_TRUE(TestNormalize("MixedCase/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "MixedCase/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol:", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol:/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol:MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL:MixedCase/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol:MixedCase/", {}));
  EXPECT_TRUE(TestNormalize("//Host", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "//Host", {}));
  EXPECT_TRUE(TestNormalize("//Host/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "//Host/", {}));
  EXPECT_TRUE(TestNormalize("//Host/MixedCase", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "//Host/MixedCase", {}));
  EXPECT_TRUE(TestNormalize("//Host/MixedCase/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "//Host/MixedCase/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol://Host", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol://Host/", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/MixedCase", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol://Host/MixedCase", {}));
  EXPECT_TRUE(TestNormalize("PROTOCOL://Host/MixedCase/", {kGenericPathFlags, PathFlag::kAllowTrailingSlash}, "protocol://Host/MixedCase/", {}));
  // clang-format on
}

}  // namespace
}  // namespace gb
