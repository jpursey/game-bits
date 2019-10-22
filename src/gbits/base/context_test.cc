#include "gbits/base/context.h"

#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(ContextTest, ConstructEmpty) {
  Context context;
  EXPECT_TRUE(context.IsEmpty());
}

}  // namespace 
}  // namespace gb