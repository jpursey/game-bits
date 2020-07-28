#include "gb/resource/resource_set.h"

#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_system.h"
#include "gb/resource/test_resources.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(ResourceSetTest, DefaultResourceSet) {
  auto resource_set = std::make_unique<ResourceSet>();
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, AddNullResource) {
  auto resource_set = std::make_unique<ResourceSet>();
  EXPECT_FALSE(resource_set->Add(nullptr));
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, Add) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
}

TEST(ResourceSetTest, AddFromDifferentSystems) {
  TestResource::Counts counts;

  auto system_1 = ResourceSystem::Create();
  ASSERT_NE(system_1, nullptr);
  ResourceManager manager_1;
  EXPECT_TRUE(system_1->Register<TestResource>(&manager_1));
  auto* resource_1 =
      new TestResource(&counts, manager_1.NewResourceEntry<TestResource>(), {});

  auto system_2 = ResourceSystem::Create();
  ASSERT_NE(system_2, nullptr);
  ResourceManager manager_2;
  EXPECT_TRUE(system_2->Register<TestResource>(&manager_2));
  auto* resource_2 =
      new TestResource(&counts, manager_2.NewResourceEntry<TestResource>(), {});

  auto resource_set = std::make_unique<ResourceSet>();
  EXPECT_TRUE(resource_set->Add(resource_1, false));
  EXPECT_TRUE(resource_1->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system_1.get());
  EXPECT_FALSE(resource_set->Add(resource_2, false));
  EXPECT_FALSE(resource_2->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system_1.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource_1->IsResourceReferenced());
  EXPECT_FALSE(resource_2->IsResourceReferenced());
}

TEST(ResourceSetTest, AddWithEmptyDependencies) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  EXPECT_TRUE(resource_set->Add(resource, true));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
}

TEST(ResourceSetTest, AddWithoutDependencies) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceSetTest, AddWithDependencies) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource, true));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceSetTest, AddWithDefaultAddDependencies) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceSetTest, AddWithNestedDependencies) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies({sub_resources[0], sub_resources[1]});
  sub_resources[0]->SetResourceDependencies(
      {sub_resources[1], sub_resources[2], sub_resources[3]});
  sub_resources[2]->SetResourceDependencies({resource, sub_resources[3]});
  sub_resources[3]->SetResourceDependencies({sub_resources[1]});
  EXPECT_TRUE(resource_set->Add(resource, true));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[2]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[3]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[2]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[3]->IsResourceReferenced());
}

TEST(ResourceSetTest, GetByResourceId) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});

  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            nullptr);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
}

TEST(ResourceSetTest, GetByResourceName) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  TestResource* resource = nullptr;
  manager.InitLoader<TestResource>([&](std::string_view name) {
    resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    return resource;
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  system->Load<TestResource>("name");
  ASSERT_NE(resource, nullptr);

  EXPECT_EQ(resource_set->Get<TestResource>("name"), nullptr);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_EQ(resource_set->Get<TestResource>("name"), resource);
}

TEST(ResourceTest, CopyConstructor) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_TRUE(resource_set->Add(sub_resources[0], false));

  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  auto new_resource_set = std::make_unique<ResourceSet>(*resource_set);
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
            sub_resources[0]);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
            nullptr);
  EXPECT_EQ(new_resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
      sub_resources[0]);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
      nullptr);
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());
  EXPECT_EQ(new_resource_set->GetSystem(), system.get());
  EXPECT_FALSE(new_resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());

  new_resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceTest, MoveConstructor) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_TRUE(resource_set->Add(sub_resources[0], false));

  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  auto new_resource_set =
      std::make_unique<ResourceSet>(std::move(*resource_set));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(new_resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
      sub_resources[0]);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
      nullptr);
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
  EXPECT_EQ(new_resource_set->GetSystem(), system.get());
  EXPECT_FALSE(new_resource_set->IsEmpty());

  new_resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceTest, CopyAssignment) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_TRUE(resource_set->Add(sub_resources[0], false));

  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  // Self assignment.
  *resource_set = *resource_set;
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
            sub_resources[0]);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  // Assign over new resource set.
  auto new_resource_set = std::make_unique<ResourceSet>(*resource_set);
  EXPECT_TRUE(new_resource_set->Add(sub_resources[0], false));
  EXPECT_TRUE(new_resource_set->Add(sub_resources[1], false));
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(new_resource_set->GetSystem(), system.get());
  EXPECT_FALSE(new_resource_set->IsEmpty());

  *new_resource_set = *resource_set;
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
            sub_resources[0]);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
            nullptr);
  EXPECT_EQ(new_resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
      sub_resources[0]);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
      nullptr);
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());
  EXPECT_EQ(new_resource_set->GetSystem(), system.get());
  EXPECT_FALSE(new_resource_set->IsEmpty());

  resource_set.reset();
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());

  new_resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceTest, MoveAssignment) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies(sub_resources);
  EXPECT_TRUE(resource_set->Add(resource, false));
  EXPECT_TRUE(resource_set->Add(sub_resources[0], false));

  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  // Self assignment.
  *resource_set = std::move(*resource_set);
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
            sub_resources[0]);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  // Assign over new resource set.
  auto new_resource_set = std::make_unique<ResourceSet>(*resource_set);
  EXPECT_TRUE(new_resource_set->Add(sub_resources[0], false));
  EXPECT_TRUE(new_resource_set->Add(sub_resources[1], false));
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(new_resource_set->GetSystem(), system.get());
  EXPECT_FALSE(new_resource_set->IsEmpty());

  *new_resource_set = std::move(*resource_set);
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_EQ(new_resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
      sub_resources[0]);
  EXPECT_EQ(
      new_resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
      nullptr);
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
  EXPECT_EQ(new_resource_set->GetSystem(), system.get());
  EXPECT_FALSE(new_resource_set->IsEmpty());

  new_resource_set.reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
}

TEST(ResourceSetTest, RemoveNullResource) {
  auto resource_set = std::make_unique<ResourceSet>();
  EXPECT_TRUE(resource_set->Remove(nullptr));
}

TEST(ResourceSetTest, RemoveResource) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  EXPECT_TRUE(resource_set->Add(resource));
  EXPECT_TRUE(resource_set->Remove(resource, false));
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemoveResourceWithReference) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource_1 =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  auto* resource_2 =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  resource_2->SetResourceDependencies({resource_1});
  EXPECT_TRUE(resource_set->Add(resource_1, false));
  EXPECT_TRUE(resource_set->Add(resource_2, false));

  EXPECT_FALSE(resource_set->Remove(resource_1, false));
  EXPECT_TRUE(resource_1->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  EXPECT_TRUE(resource_set->Remove(resource_2, false));
  EXPECT_FALSE(resource_2->IsResourceReferenced());
  EXPECT_TRUE(resource_set->Remove(resource_1, false));
  EXPECT_FALSE(resource_1->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemoveWithNestedDependencies) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies({sub_resources[0], sub_resources[1]});
  sub_resources[0]->SetResourceDependencies(
      {sub_resources[1], sub_resources[2], sub_resources[3]});
  sub_resources[2]->SetResourceDependencies({resource, sub_resources[3]});
  sub_resources[3]->SetResourceDependencies({sub_resources[1]});
  EXPECT_TRUE(resource_set->Add(resource, true));
  EXPECT_TRUE(resource_set->Remove(resource, true));
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[2]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[3]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemovePartial) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {}),
  };
  other_resource->SetResourceDependencies({sub_resources[3]});
  resource->SetResourceDependencies({sub_resources[0], sub_resources[1]});
  sub_resources[0]->SetResourceDependencies(
      {sub_resources[1], sub_resources[2], sub_resources[3]});
  sub_resources[2]->SetResourceDependencies({resource, sub_resources[3]});
  sub_resources[3]->SetResourceDependencies({sub_resources[1]});
  EXPECT_TRUE(resource_set->Add(resource, true));
  EXPECT_TRUE(resource_set->Add(other_resource, true));
  EXPECT_TRUE(resource_set->Remove(resource, true));
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_TRUE(other_resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[2]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[3]->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemoveResourceOnly) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource_1 =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  auto* resource_2 =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  resource_1->SetResourceDependencies({resource_2});
  EXPECT_TRUE(resource_set->Add(resource_1, true));

  EXPECT_TRUE(resource_set->Remove(resource_1, false));
  EXPECT_FALSE(resource_1->IsResourceReferenced());
  EXPECT_TRUE(resource_2->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemoveResourceById) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  EXPECT_TRUE(
      resource_set->Remove<TestResource>(resource->GetResourceId(), false));

  EXPECT_TRUE(resource_set->Add(resource));

  EXPECT_TRUE(resource_set->Remove<ResourceA>(resource->GetResourceId()));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);

  EXPECT_TRUE(
      resource_set->Remove<TestResource>(resource->GetResourceId(), false));
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemoveResourceByName) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  TestResource* resource = nullptr;
  manager.InitLoader<TestResource>([&](std::string_view name) {
    resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    return resource;
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  system->Load<TestResource>("name");
  ASSERT_NE(resource, nullptr);

  EXPECT_TRUE(resource_set->Remove<TestResource>("name", false));

  EXPECT_TRUE(resource_set->Add(resource, false));

  EXPECT_TRUE(resource_set->Remove<ResourceA>("name", false));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);

  EXPECT_TRUE(resource_set->Remove<TestResource>("name", false));
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, RemoveAll) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource_1 =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  auto* resource_2 =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  resource_1->SetResourceDependencies({resource_2});
  EXPECT_TRUE(resource_set->Add(resource_1, true));

  resource_set->RemoveAll();
  EXPECT_FALSE(resource_1->IsResourceReferenced());
  EXPECT_FALSE(resource_2->IsResourceReferenced());
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());
}

TEST(ResourceSetTest, MultipleResourceTypesFromMultipleManagers) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager_1;
  EXPECT_TRUE(system->Register<TestResource>(&manager_1));
  ResourceManager manager_2;
  EXPECT_TRUE((system->Register<ResourceA, ResourceB, ResourceC>(&manager_2)));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager_1.NewResourceEntry<TestResource>(), {});
  std::vector<TestResource*> sub_resources = {
      new ResourceA(&counts, manager_2.NewResourceEntry<ResourceA>(), {}),
      new ResourceB(&counts, manager_2.NewResourceEntry<ResourceB>(), {}),
      new ResourceC(&counts, manager_2.NewResourceEntry<ResourceC>(), {}),
      new TestResource(&counts, manager_1.NewResourceEntry<TestResource>(), {}),
  };
  resource->SetResourceDependencies({sub_resources[0], sub_resources[1]});
  sub_resources[0]->SetResourceDependencies(
      {sub_resources[1], sub_resources[2], sub_resources[3]});
  sub_resources[2]->SetResourceDependencies({resource, sub_resources[3]});
  sub_resources[3]->SetResourceDependencies({sub_resources[1]});

  EXPECT_TRUE(resource_set->Add(resource, true));
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[0]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[1]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[2]->IsResourceReferenced());
  EXPECT_TRUE(sub_resources[3]->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[0]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[1]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[2]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[3]->GetResourceId()),
            sub_resources[3]);
  EXPECT_EQ(resource_set->Get<ResourceA>(sub_resources[0]->GetResourceId()),
            sub_resources[0]);
  EXPECT_EQ(resource_set->Get<ResourceB>(sub_resources[1]->GetResourceId()),
            sub_resources[1]);
  EXPECT_EQ(resource_set->Get<ResourceC>(sub_resources[2]->GetResourceId()),
            sub_resources[2]);

  EXPECT_TRUE(resource_set->Remove(resource, true));
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[0]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[1]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[2]->IsResourceReferenced());
  EXPECT_FALSE(sub_resources[3]->IsResourceReferenced());
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<ResourceA>(sub_resources[0]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<ResourceB>(sub_resources[1]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<ResourceC>(sub_resources[2]->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->Get<TestResource>(sub_resources[3]->GetResourceId()),
            nullptr);
}

TEST(ResourceSetTest, SystemAddToSetById) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  resource->SetResourceDependencies({other_resource});

  EXPECT_EQ(system->Get<TestResource>(resource_set.get(),
                                      resource->GetResourceId(), false),
            nullptr);
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());

  resource->SetResourceVisible();
  EXPECT_EQ(
      system->Get<TestResource>(nullptr, resource->GetResourceId(), false),
      nullptr);

  EXPECT_EQ(system->Get<TestResource>(resource_set.get(),
                                      resource->GetResourceId(), false),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(other_resource->GetResourceId()),
            nullptr);
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  EXPECT_TRUE(resource_set->Remove(resource));
  EXPECT_EQ(system->Get<TestResource>(resource_set.get(),
                                      resource->GetResourceId(), true),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(other_resource->GetResourceId()),
            other_resource);
}

TEST(ResourceSetTest, SystemAddToSetByName) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  manager.InitLoader<TestResource>([&](std::string_view name) {
    return new TestResource(&counts, manager.NewResourceEntry<TestResource>(),
                            {});
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  auto resource_ptr = system->Load<TestResource>("resource");
  auto other_resource_ptr = system->Load<TestResource>("other_resource");
  resource_ptr->SetResourceDependencies({other_resource_ptr.Get()});

  EXPECT_EQ(system->Get<TestResource>(resource_set.get(), "resource", false),
            nullptr);
  EXPECT_EQ(resource_set->GetSystem(), nullptr);
  EXPECT_TRUE(resource_set->IsEmpty());

  resource_ptr->SetResourceVisible();
  EXPECT_EQ(system->Get<TestResource>(nullptr, "resource", false), nullptr);

  EXPECT_EQ(system->Get<TestResource>(resource_set.get(), "resource", false),
            resource_ptr.Get());
  EXPECT_EQ(resource_set->Get<TestResource>(resource_ptr->GetResourceId()),
            resource_ptr.Get());
  EXPECT_EQ(
      resource_set->Get<TestResource>(other_resource_ptr->GetResourceId()),
      nullptr);
  EXPECT_EQ(resource_set->GetSystem(), system.get());
  EXPECT_FALSE(resource_set->IsEmpty());

  EXPECT_TRUE(resource_set->Remove(resource_ptr.Get()));
  EXPECT_EQ(system->Get<TestResource>(resource_set.get(), "resource", true),
            resource_ptr.Get());
  EXPECT_EQ(resource_set->Get<TestResource>(resource_ptr->GetResourceId()),
            resource_ptr.Get());
  EXPECT_EQ(
      resource_set->Get<TestResource>(other_resource_ptr->GetResourceId()),
      other_resource_ptr.Get());
}

TEST(ResourceSetTest, SystemLoadToSet) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  TestResource* resource = nullptr;
  TestResource* other_resource = nullptr;
  manager.InitLoader<TestResource>([&](std::string_view name) -> TestResource* {
    if (name != "resource") {
      return nullptr;
    }
    resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    other_resource =
        new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
    resource->SetResourceDependencies({other_resource});
    return resource;
  });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto resource_set = std::make_unique<ResourceSet>();
  EXPECT_EQ(system->Load<TestResource>(resource_set.get(), "missing"), nullptr);
  EXPECT_EQ(system->Load<TestResource>(nullptr, "resource"), nullptr);
  EXPECT_EQ(counts.construct, 0);

  EXPECT_EQ(system->Load<TestResource>(resource_set.get(), "resource"),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(resource->GetResourceId()),
            resource);
  EXPECT_EQ(resource_set->Get<TestResource>(other_resource->GetResourceId()),
            other_resource);
}

}  // namespace
}  // namespace gb
