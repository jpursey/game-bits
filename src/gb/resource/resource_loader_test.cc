// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_system.h"
#include "gb/resource/test_resources.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::IsEmpty;

TEST(ResourceLoaderTest, LoadUnregisterdResourceType) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  auto resource_ptr = system->Load<TestResource>("name");
  EXPECT_EQ(resource_ptr.Get(), nullptr);
}

TEST(ResourceLoaderTest, DefaultLoaderIsNull) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_ptr = system->Load<TestResource>("name");
  EXPECT_EQ(resource_ptr.Get(), nullptr);
}

TEST(ResourceLoaderTest, GenericLoader) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  TestResource* resource = nullptr;
  int load_count = 0;
  manager.InitGenericLoader([&](TypeKey* type,
                                std::string_view name) -> Resource* {
    EXPECT_EQ(name, "name");
    ++load_count;
    resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    return resource;
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  // Initial load should call loader.
  auto resource_ptr = system->Load<TestResource>("name");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->GetResourceName(), "name");
  EXPECT_EQ(resource_ptr.Get(), resource);
  EXPECT_EQ(load_count, 1);

  // Subsequent load return cached resource, even though it is not visible.
  Resource* old_resource = resource;
  auto other_resource_ptr = system->Load<TestResource>("name");
  ASSERT_EQ(resource, old_resource);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_EQ(load_count, 1);
}

TEST(ResourceLoaderTest, TypeSpecificLoader) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;

  ResourceA* resource_a = nullptr;
  int load_count_a = 0;
  manager.InitLoader<ResourceA>([&](std::string_view name) -> ResourceA* {
    EXPECT_EQ(name, "a");
    ++load_count_a;
    resource_a =
        new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(), {});
    return resource_a;
  });

  ResourceB* resource_b = nullptr;
  int load_count_b = 0;
  manager.InitGenericLoader(
      [&](TypeKey* type, std::string_view name) -> Resource* {
        EXPECT_EQ(type, TypeKey::Get<ResourceB>());
        EXPECT_EQ(name, "b");
        ++load_count_b;
        resource_b =
            new ResourceB(&counts, manager.NewResourceEntry<ResourceB>(), {});
        return resource_b;
      });

  EXPECT_TRUE((system->Register<ResourceA, ResourceB>(&manager)));

  auto resource_a_ptr = system->Load<ResourceA>("a");
  ASSERT_NE(resource_a, nullptr);
  EXPECT_EQ(resource_a->GetResourceName(), "a");
  EXPECT_EQ(resource_a_ptr.Get(), resource_a);
  EXPECT_EQ(load_count_a, 1);

  auto resource_b_ptr = system->Load<ResourceB>("b");
  ASSERT_NE(resource_b, nullptr);
  EXPECT_EQ(resource_b->GetResourceName(), "b");
  EXPECT_EQ(resource_b_ptr.Get(), resource_b);
  EXPECT_EQ(load_count_b, 1);
}

TEST(ResourceLoaderTest, DuplicateInitLoaderFails) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  int generic_load = 0;
  manager.InitGenericLoader(
      [&generic_load](TypeKey* type, std::string_view name) -> Resource* {
        ++generic_load;
        return nullptr;
      });
  manager.InitGenericLoader(
      [&generic_load](TypeKey* type, std::string_view name) -> Resource* {
        generic_load += 100;
        return nullptr;
      });
  int typed_load = 0;
  manager.InitLoader<ResourceA>(
      [&typed_load](std::string_view name) -> ResourceA* {
        ++typed_load;
        return nullptr;
      });
  manager.InitLoader<ResourceA>(
      [&typed_load](std::string_view name) -> ResourceA* {
        typed_load += 100;
        return nullptr;
      });
  EXPECT_TRUE((system->Register<ResourceA, ResourceB>(&manager)));

  auto resource_a_ptr = system->Load<ResourceA>("a");
  EXPECT_EQ(typed_load, 1);
  EXPECT_EQ(generic_load, 0);

  auto resource_b_ptr = system->Load<ResourceB>("b");
  EXPECT_EQ(typed_load, 1);
  EXPECT_EQ(generic_load, 1);
}

TEST(ResourceLoaderTest, InitLoaderAfterRegisterFails) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE((system->Register<ResourceA, ResourceB>(&manager)));
  int generic_load = 0;
  manager.InitGenericLoader(
      [&generic_load](TypeKey* type, std::string_view name) -> Resource* {
        ++generic_load;
        return nullptr;
      });
  int typed_load = 0;
  manager.InitLoader<ResourceA>(
      [&typed_load](std::string_view name) -> ResourceA* {
        ++typed_load;
        return nullptr;
      });

  auto resource_a_ptr = system->Load<ResourceA>("a");
  EXPECT_EQ(typed_load, 0);
  EXPECT_EQ(generic_load, 0);

  auto resource_b_ptr = system->Load<ResourceB>("b");
  EXPECT_EQ(typed_load, 0);
  EXPECT_EQ(generic_load, 0);
}

TEST(ResourceLoaderTest, ReloadAfterDelete) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  TestResource* resource = nullptr;
  int load_count = 0;
  manager.InitLoader<TestResource>([&](std::string_view name) -> TestResource* {
    EXPECT_EQ(name, "name");
    ++load_count;
    resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    return resource;
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_ptr = system->Load<TestResource>("name");
  EXPECT_EQ(load_count, 1);
  resource_ptr.Reset();

  EXPECT_TRUE(manager.MaybeDeleteResource(resource));

  auto other_resource_ptr = system->Load<TestResource>("name");
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_EQ(load_count, 2);
}

TEST(ResourceLoaderTest, LoadedResourceIsNotVisible) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  TestResource* resource = nullptr;
  manager.InitLoader<TestResource>([&](std::string_view name) -> TestResource* {
    EXPECT_EQ(name, "name");
    resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    return resource;
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_ptr = system->Load<TestResource>("name");
  auto other_resource_ptr = system->Get<TestResource>("name");
  EXPECT_EQ(other_resource_ptr.Get(), nullptr);
}

TEST(ResourceLoaderTest, ManagerDestructEdgeConditions) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceSystem* system_ptr = system.get();
  auto manager = std::make_unique<ResourceManager>();
  manager->InitLoader<TestResource>([&](std::string_view name) {
    TestResource* resource = new TestResource(
        &counts, manager->NewResourceEntry<TestResource>(),
        {ResourceFlag::kAutoRelease, ResourceFlag::kAutoVisible});
    resource->SetDeleteCallback([resource, system_ptr] {
      auto resource_ptr = system_ptr->Get<TestResource>("name");
      EXPECT_EQ(resource_ptr, nullptr);
      resource_ptr = system_ptr->Load<TestResource>("name");
      EXPECT_EQ(resource_ptr, nullptr);
    });
    return resource;
  });
  EXPECT_TRUE((system->Register<TestResource>(manager.get())));
  auto resource_ptr = system->Load<TestResource>("name");
  manager.reset();
}

TEST(ResourceLoaderTest, SystemDestructEdgeConditions) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceSystem* system_ptr = system.get();
  auto manager = std::make_unique<ResourceManager>();
  manager->InitLoader<TestResource>([&](std::string_view name) {
    TestResource* resource = new TestResource(
        &counts, manager->NewResourceEntry<TestResource>(),
        {ResourceFlag::kAutoRelease, ResourceFlag::kAutoVisible});
    resource->SetDeleteCallback([resource, system_ptr] {
      auto resource_ptr = system_ptr->Get<TestResource>("name");
      EXPECT_EQ(resource_ptr, nullptr);
      resource_ptr = system_ptr->Load<TestResource>("name");
      EXPECT_EQ(resource_ptr, nullptr);
    });
    return resource;
  });
  EXPECT_TRUE((system->Register<TestResource>(manager.get())));
  auto resource_ptr = system->Load<TestResource>("name");
  resource_ptr.Reset();
  system.reset();
}

}  // namespace
}  // namespace gb
