// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/test/thread_tester.h"

#include "gtest/gtest.h"

namespace gb {

TEST(ThreadTesterTest, MaxConcurrency) {
  EXPECT_GT(ThreadTester::MaxConcurrency(), 1);
  EXPECT_LE(static_cast<int>(std::thread::hardware_concurrency()),
            ThreadTester::MaxConcurrency());
}

TEST(ThreadTesterTest, RunSucceeds) {
  std::atomic<bool> done = false;
  ThreadTester tester;
  tester.Run("test", [&done]() {
    done = true;
    return true;
  });
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kSuccess);
  EXPECT_TRUE(done);
}

TEST(ThreadTesterTest, RunFails) {
  ThreadTester tester;
  tester.Run("test", []() { return false; });
  EXPECT_FALSE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kFailure);
}

TEST(ThreadTesterTest, RunMultiple) {
  std::atomic<int> count = 0;
  ThreadTester tester;
  auto func = [&count]() {
    ++count;
    return true;
  };
  tester.Run("test", func, ThreadTester::MaxConcurrency());
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kSuccess);
  EXPECT_EQ(count, ThreadTester::MaxConcurrency());
}

TEST(ThreadTesterTest, RunMultipleOneFailure) {
  std::atomic<int> count = 0;
  ThreadTester tester;
  auto func = [&count]() { return ++count != 2; };
  tester.Run("test", func, 3);
  EXPECT_FALSE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kFailure);
  EXPECT_EQ(count, 3);
}

TEST(ThreadTesterTest, RunWaits) {
  std::atomic<bool> done = false;
  ThreadTester tester;
  tester.Run("test", [&done, &tester]() {
    tester.Signal(1);
    tester.Wait(2);
    done = true;
    return true;
  });
  tester.Wait(1);
  EXPECT_FALSE(done);
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kRunning);
  tester.Signal(2);
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kSuccess);
  EXPECT_TRUE(done);
}

TEST(ThreadTesterTest, RunThenSignalSucceeds) {
  ThreadTester tester;
  tester.RunThenSignal(1, "test", []() { return true; });
  tester.Wait(1);
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kSuccess);
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

TEST(ThreadTesterTest, RunThenSignalFails) {
  ThreadTester tester;
  tester.RunThenSignal(1, "test", []() { return false; });
  tester.Wait(1);
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kFailure);
  EXPECT_FALSE(tester.Complete()) << tester.GetResultString();
}

TEST(ThreadTesterTest, RunLoopSucceeds) {
  ThreadTester tester;
  std::atomic<int> id = 0;
  std::atomic<int> count = 0;
  auto func = [&tester, &id, &count]() {
    tester.Wait(++id);
    ++count;
    tester.Signal(++id);
    return true;
  };
  tester.RunLoop(100, "test", func);
  EXPECT_EQ(count, 0);
  tester.Signal(1);
  tester.Wait(2);
  EXPECT_EQ(count, 1);
  tester.Signal(3);
  tester.Wait(4);
  EXPECT_EQ(count, 2);
  tester.Signal(100);
  tester.Signal(5);
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kSuccess);
  EXPECT_EQ(count, 3);
}

TEST(ThreadTesterTest, RunLoopFails) {
  ThreadTester tester;
  std::atomic<int> count = 0;
  auto func = [&tester, &count]() {
    ++count;
    return count < 2;
  };
  tester.RunLoop(100, "test", func);
  while (count < 2) {
    std::this_thread::yield();
  }
  EXPECT_FALSE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("test"), ThreadTester::kFailure);
  EXPECT_EQ(count, 2);
}

TEST(ThreadTesterTest, MultipleNamedThreadsSuccess) {
  ThreadTester tester;
  tester.RunThenSignal(100, "a", [&tester]() {
    tester.Wait(1);
    return true;
  });
  tester.Run("b", [&tester]() {
    tester.Wait(2);
    return true;
  });
  tester.Run("c", [&tester]() {
    tester.Wait(2);
    return true;
  });
  EXPECT_EQ(tester.GetRunResult("a"), ThreadTester::kRunning);
  EXPECT_EQ(tester.GetRunResult("b"), ThreadTester::kRunning);
  EXPECT_EQ(tester.GetRunResult("c"), ThreadTester::kRunning);
  tester.Signal(1);
  tester.Wait(100);
  EXPECT_EQ(tester.GetRunResult("a"), ThreadTester::kSuccess);
  EXPECT_EQ(tester.GetRunResult("b"), ThreadTester::kRunning);
  EXPECT_EQ(tester.GetRunResult("c"), ThreadTester::kRunning);
  tester.Signal(2);
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("a"), ThreadTester::kSuccess);
  EXPECT_EQ(tester.GetRunResult("b"), ThreadTester::kSuccess);
  EXPECT_EQ(tester.GetRunResult("c"), ThreadTester::kSuccess);
}

TEST(ThreadTesterTest, MultipleNamedThreadsOneFailure) {
  ThreadTester tester;
  tester.Run("a", [&tester]() { return true; });
  tester.Run("b", [&tester]() { return false; });
  tester.Run("c", [&tester]() { return true; });
  EXPECT_FALSE(tester.Complete()) << tester.GetResultString();
  EXPECT_EQ(tester.GetRunResult("a"), ThreadTester::kSuccess);
  EXPECT_EQ(tester.GetRunResult("b"), ThreadTester::kFailure);
  EXPECT_EQ(tester.GetRunResult("c"), ThreadTester::kSuccess);
}

}  // namespace gb
