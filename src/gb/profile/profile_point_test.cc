// Copyright (c) 2026 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/profile/profile_point.h"

#include <optional>
#include <string>

#include "absl/strings/str_cat.h"
#include "gb/profile/fake_ticks.h"
#include "gb/profile/profiler.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

// Points are registered for the whole program, so each test uses names of its
// own.

TEST(ProfilePointTest, RegistersKindAndName) {
  ProfilePoint point(ProfilePoint::Kind::kCall, "PointTest/KindAndName");
  EXPECT_EQ(point.GetKind(), ProfilePoint::Kind::kCall);
  EXPECT_EQ(point.GetName(), "PointTest/KindAndName");
  EXPECT_GE(point.GetIndex(), 0);
  EXPECT_LT(point.GetIndex(), kMaxProfilePoints);
}

TEST(ProfilePointTest, SameNameIsSamePoint) {
  ProfilePoint first(ProfilePoint::Kind::kScope, "PointTest/Same");
  ProfilePoint second(ProfilePoint::Kind::kScope, "PointTest/Same");
  ProfilePoint other(ProfilePoint::Kind::kScope, "PointTest/Other");
  EXPECT_EQ(first.GetIndex(), second.GetIndex());
  EXPECT_NE(first.GetIndex(), other.GetIndex());
}

TEST(ProfilePointTest, NameIsKeptByRegistry) {
  std::string name = "PointTest/Kept";
  ProfilePoint point(ProfilePoint::Kind::kScope, name);
  name = "changed";
  EXPECT_EQ(point.GetName(), "PointTest/Kept");
}

TEST(ProfilePointTest, NameIsKeptAsMorePointsAreRegistered) {
  // Short enough to be stored inside a std::string, so the name would move if
  // the registry moved its strings as it grew.
  ProfilePoint point(ProfilePoint::Kind::kScope, "PointTest/K");
  for (int i = 0; i < 100; ++i) {
    ProfilePoint(ProfilePoint::Kind::kScope, absl::StrCat("PointTest/K", i));
  }
  EXPECT_EQ(point.GetName(), "PointTest/K");
}

TEST(ProfilePointTest, FindRegisteredPoint) {
  ProfilePoint point(ProfilePoint::Kind::kCounter, "PointTest/Find");
  std::optional<ProfilePoint> found = ProfilePoint::Find("PointTest/Find");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->GetKind(), ProfilePoint::Kind::kCounter);
  EXPECT_EQ(found->GetIndex(), point.GetIndex());
}

TEST(ProfilePointTest, FindUnknownPoint) {
  EXPECT_FALSE(ProfilePoint::Find("PointTest/Unknown").has_value());
}

TEST(ProfilePointTest, SameNameWithAnotherKindDies) {
  ProfilePoint point(ProfilePoint::Kind::kScope, "PointTest/Kind");
  EXPECT_DEATH(ProfilePoint(ProfilePoint::Kind::kCall, "PointTest/Kind"),
               "registered with another kind");
}

// Tests that record points in a Profiler.
class ProfilePointRecordTest : public ::testing::Test {
 protected:
  FakeTicks ticks_;
  Profiler profiler_{{.fake_ticks = &ticks_}};
};

TEST_F(ProfilePointRecordTest, CountAdds) {
  ProfilePoint point(ProfilePoint::Kind::kCounter, "PointTest/Count");
  point.Count(2);
  point.Count(3);
  EXPECT_EQ(profiler_.GetCount("PointTest/Count"), 5);
}

TEST_F(ProfilePointRecordTest, SetValueKeepsLatest) {
  ProfilePoint point(ProfilePoint::Kind::kValue, "PointTest/Value");
  point.SetValue(2);
  point.SetValue(7);
  EXPECT_EQ(profiler_.GetValue("PointTest/Value"), 7);
}

TEST_F(ProfilePointRecordTest, NamedPointsAreSameAsRegistered) {
  ProfilePoint counter(ProfilePoint::Kind::kCounter, "PointTest/NamedCount");
  ProfilePoint value(ProfilePoint::Kind::kValue, "PointTest/NamedValue");
  ProfileCount<"PointTest/NamedCount">(2);
  counter.Count(3);
  ProfileSetValue<"PointTest/NamedValue">(4);
  EXPECT_EQ(profiler_.GetCount("PointTest/NamedCount"), 5);
  EXPECT_EQ(profiler_.GetValue("PointTest/NamedValue"), 4);
}

TEST(ProfilePointTest, IgnoredWithoutProfiler) {
  ProfileCount<"PointTest/NoProfilerCount">(2);
  ProfileSetValue<"PointTest/NoProfilerValue">(3);

  FakeTicks ticks;
  Profiler profiler({.fake_ticks = &ticks});
  EXPECT_EQ(profiler.GetCount("PointTest/NoProfilerCount"), 0);
  EXPECT_EQ(profiler.GetValue("PointTest/NoProfilerValue"), 0);
}

}  // namespace
}  // namespace gb
