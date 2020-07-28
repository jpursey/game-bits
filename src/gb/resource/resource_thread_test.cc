#include <random>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gb/resource/resource.h"
#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_ptr.h"
#include "gb/resource/resource_set.h"
#include "gb/resource/resource_system.h"
#include "gb/resource/test_resources.h"
#include "gb/test/thread_tester.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(ResourceThreadTest, ThreadAbuse) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);

  ResourceManager manager_1;
  manager_1.InitLoader<TestResource>([&manager_1](std::string_view name) {
    return new TestResource(manager_1.NewResourceEntry<TestResource>());
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager_1));

  ResourceManager manager_2;
  manager_2.InitLoader<ResourceA>([&manager_2](std::string_view name) {
    return new ResourceA(manager_2.NewResourceEntry<ResourceA>());
  });
  manager_2.InitLoader<ResourceB>([&manager_2](std::string_view name) {
    return new ResourceB(manager_2.NewResourceEntry<ResourceB>());
  });
  manager_2.InitLoader<ResourceC>([&manager_2](std::string_view name) {
    return new ResourceC(manager_2.NewResourceEntry<ResourceC>());
  });
  EXPECT_TRUE((system->Register<ResourceA, ResourceB, ResourceC>(&manager_2)));

  std::vector<TestResource*> global_resources = {
      new ResourceA(manager_2.NewResourceEntry<ResourceA>()),
      new ResourceB(manager_2.NewResourceEntry<ResourceB>()),
      new ResourceC(manager_2.NewResourceEntry<ResourceC>()),
      new TestResource(manager_1.NewResourceEntry<TestResource>()),
  };
  std::vector<ResourceId> global_resource_ids;
  auto global_set = std::make_unique<ResourceSet>();
  for (auto* resource : global_resources) {
    global_resource_ids.push_back(resource->GetResourceId());
    global_set->Add(resource);
  }

  std::atomic<int> next_id = 0;

  ThreadTester tester;

  auto id_incrementer = [&next_id] {
    next_id = (next_id + 1) % 10;
    return true;
  };
  auto loader = [&] {
    ResourceSet loader_set;
    system->Load<TestResource>(
        &loader_set, absl::StrCat(next_id.load(std::memory_order_relaxed)));
    system->Load<ResourceA>(
        &loader_set, absl::StrCat(next_id.load(std::memory_order_relaxed)));
    system->Load<ResourceB>(
        &loader_set, absl::StrCat(next_id.load(std::memory_order_relaxed)));
    system->Load<ResourceC>(
        &loader_set, absl::StrCat(next_id.load(std::memory_order_relaxed)));
    absl::SleepFor(absl::Milliseconds(5));
    return true;
  };
  auto reader = [&] {
    ResourceSet set_1;
    system->Get<ResourceA>(&set_1, global_resource_ids[0]);
    system->Get<ResourceB>(&set_1, global_resource_ids[1]);
    system->Get<ResourceC>(&set_1, global_resource_ids[2]);
    system->Get<TestResource>(&set_1, global_resource_ids[3]);

    ResourceSet set_2;
    for (int i = 0; i < 10; i += 2) {
      auto test_resource = system->Get<TestResource>(absl::StrCat(i));
      if (test_resource != nullptr) {
        set_2.Add(test_resource.Get());
      }
      system->Get<ResourceA>(&set_2, absl::StrCat(i + 1));
      system->Get<ResourceB>(&set_2, absl::StrCat(i + 2));
      system->Get<ResourceC>(&set_2, absl::StrCat(i + 3));
    }

    absl::SleepFor(absl::Milliseconds(1));

    set_1 = set_2;

    absl::SleepFor(absl::Milliseconds(1));

    system->Get<ResourceA>(&set_2, global_resource_ids[0]);
    system->Get<ResourceB>(&set_2, global_resource_ids[1]);
    system->Get<ResourceC>(&set_2, global_resource_ids[2]);
    system->Get<TestResource>(&set_2, global_resource_ids[3]);
    return true;
  };
  tester.RunLoop(1, "id_incrementer", id_incrementer);
  tester.RunLoop(2, "loader", loader, ThreadTester::MaxConcurrency());
  tester.RunLoop(3, "reader", reader, ThreadTester::MaxConcurrency());
  absl::SleepFor(absl::Milliseconds(500));
  global_set.reset();
  absl::SleepFor(absl::Milliseconds(500));
  EXPECT_TRUE(tester.Complete()) << tester.GetResultString();
}

}  // namespace
}  // namespace gb
