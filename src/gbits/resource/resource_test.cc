#include "gbits/resource/resource.h"

#include "gbits/resource/resource_manager.h"
#include "gbits/resource/resource_ptr.h"
#include "gbits/resource/resource_set.h"
#include "gbits/resource/resource_system.h"
#include "gbits/resource/test_resources.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::IsEmpty;

TEST(ResourceTest, RegisterNullManager) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  EXPECT_FALSE(system->Register<TestResource>(nullptr));
}

TEST(ResourceTest, RegisterManagerWithNoTypes) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_FALSE(system->Register<>(&manager));
}

TEST(ResourceTest, RegisterManagerTwice) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<ResourceA>(&manager));
  EXPECT_EQ(system.get(), manager.GetSystem());
  EXPECT_FALSE(system->Register<ResourceB>(&manager));
}

TEST(ResourceTest, RegisterManagerWithTheSameType) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager1;
  EXPECT_TRUE(system->Register<ResourceA>(&manager1));
  ResourceManager manager2;
  EXPECT_FALSE(system->Register<ResourceA>(&manager2));
  ResourceManager manager3;
  EXPECT_FALSE((system->Register<ResourceB, ResourceC, ResourceA>(&manager3)));
}

TEST(ResourceTest, ResourceManagerNewEntry) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  ResourceEntry entry = manager.NewResourceEntry<TestResource>();
  EXPECT_TRUE(entry);
  EXPECT_EQ(entry.GetSystem(), system.get());
  EXPECT_EQ(entry.GetType(), TypeKey::Get<TestResource>());
  EXPECT_NE(entry.GetId(), 0);
  EXPECT_THAT(entry.GetName(), IsEmpty());
}

TEST(ResourceTest, ResourceManagerNewEntryWithId) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  ResourceEntry entry = manager.NewResourceEntryWithId<TestResource>(1);
  EXPECT_TRUE(entry);
  EXPECT_EQ(entry.GetSystem(), system.get());
  EXPECT_EQ(entry.GetType(), TypeKey::Get<TestResource>());
  EXPECT_EQ(entry.GetId(), 1);
  EXPECT_THAT(entry.GetName(), IsEmpty());
}

TEST(ResourceTest, ResourceManagerNewEntryInvalidType) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<ResourceA>(&manager));

  EXPECT_FALSE(manager.NewResourceEntry<ResourceB>());
  EXPECT_FALSE(manager.NewResourceEntryWithId<ResourceB>(1));
}

TEST(ResourceTest, ResourceManagerNewEntryInvalidId) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  EXPECT_FALSE(manager.NewResourceEntryWithId<TestResource>(0));
}

TEST(ResourceTest, ResourceManagerNewEntryInUseId) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto entry = manager.NewResourceEntryWithId<TestResource>(1);
  EXPECT_TRUE(entry);
  EXPECT_FALSE(manager.NewResourceEntryWithId<TestResource>(1));
}

TEST(ResourceTest, ResourceManagerEntryIdCanBeReused) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  {
    EXPECT_TRUE(manager.NewResourceEntryWithId<TestResource>(1));
    // Entry is deleted.
  }
  EXPECT_TRUE(manager.NewResourceEntryWithId<TestResource>(1));
}

TEST(ResourceTest, ResourceManagerNewEntryIdsAreUnique) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  std::vector<ResourceEntry> entries;
  absl::flat_hash_set<ResourceId> ids;
  for (int i = 0; i < 10000; ++i) {
    ResourceEntry entry = manager.NewResourceEntry<TestResource>();
    ASSERT_TRUE(entry) << "Entry " << i << " failed";
    ASSERT_FALSE(ids.contains(entry.GetId()))
        << "Entry was generated with duplicate ID";
    ids.insert(entry.GetId());
    entries.emplace_back(std::move(entry));
  }
}

TEST(ResourceTest, CreateResource) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource = new TestResource(
      &counts, manager.NewResourceEntryWithId<TestResource>(1), {});
  EXPECT_EQ(resource->GetResourceSystem(), system.get());
  EXPECT_EQ(resource->GetResourceType(), TypeKey::Get<TestResource>());
  EXPECT_EQ(resource->GetResourceId(), 1);
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_THAT(resource->GetResourceName(), IsEmpty());
  EXPECT_TRUE(manager.MaybeDeleteResource(resource));
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ResourceTest, ResourceDeletedWhenManagerIsDestroyed) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  auto manager = std::make_unique<ResourceManager>();
  EXPECT_TRUE(system->Register<TestResource>(manager.get()));

  auto* resource = new TestResource(
      &counts, manager->NewResourceEntryWithId<TestResource>(1), {});
  manager.reset();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ResourceTest, ResourceDeletedWhenSystemIsDestroyed) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  auto manager = std::make_unique<ResourceManager>();
  EXPECT_TRUE(system->Register<TestResource>(manager.get()));

  auto* resource = new TestResource(
      &counts, manager->NewResourceEntryWithId<TestResource>(1), {});
  system.reset();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ResourceTest, ResourcePtrConstructDefault) {
  ResourcePtr<TestResource> resource_ptr;
  EXPECT_FALSE(resource_ptr);
  EXPECT_EQ(resource_ptr, nullptr);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
}

TEST(ResourceTest, ResourcePtrConstructRawPointer) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  {
    ResourcePtr<TestResource> resource_ptr(resource);
    EXPECT_TRUE(resource_ptr);
    EXPECT_NE(resource_ptr, nullptr);
    EXPECT_EQ(resource_ptr.Get(), resource);
    EXPECT_TRUE(resource->IsResourceReferenced());
  }
  EXPECT_FALSE(resource->IsResourceReferenced());
}

TEST(ResourceTest, ResourcePtrReset) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  ResourcePtr<TestResource> resource_ptr(resource);
  resource_ptr.Reset();
  EXPECT_FALSE(resource->IsResourceReferenced());

  resource_ptr.Reset(resource);
  EXPECT_EQ(resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  resource_ptr.Reset(other_resource);
  EXPECT_EQ(resource_ptr.Get(), other_resource);
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_TRUE(other_resource->IsResourceReferenced());

  resource_ptr.Reset(other_resource);
  EXPECT_EQ(resource_ptr.Get(), other_resource);
  EXPECT_TRUE(other_resource->IsResourceReferenced());

  resource_ptr.Reset(nullptr);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_FALSE(other_resource->IsResourceReferenced());
}

TEST(ResourceTest, ResourcePtrCopy) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  ResourcePtr<TestResource> resource_ptr(resource);
  ResourcePtr<TestResource> other_resource_ptr(resource_ptr);
  EXPECT_EQ(resource_ptr.Get(), other_resource_ptr.Get());
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  resource_ptr.Reset();
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  other_resource_ptr = other_resource_ptr;
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  other_resource_ptr = other_resource;
  EXPECT_EQ(other_resource_ptr.Get(), other_resource);
  EXPECT_TRUE(other_resource->IsResourceReferenced());
  EXPECT_FALSE(resource->IsResourceReferenced());

  resource_ptr = other_resource_ptr;
  EXPECT_EQ(resource_ptr.Get(), other_resource);
  EXPECT_EQ(other_resource_ptr.Get(), other_resource);
  EXPECT_TRUE(other_resource->IsResourceReferenced());

  resource_ptr = resource;
  other_resource_ptr = resource_ptr;
  EXPECT_EQ(resource_ptr.Get(), resource);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_FALSE(other_resource->IsResourceReferenced());
}

TEST(ResourceTest, ResourcePtrCopyDerived) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<ResourceA>(&manager));

  auto* resource =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(), {});
  ResourcePtr<ResourceA> resource_ptr(resource);
  ResourcePtr<TestResource> other_resource_ptr(resource_ptr);
  EXPECT_EQ(resource_ptr.Get(), other_resource_ptr.Get());
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  resource_ptr.Reset();
  EXPECT_TRUE(resource->IsResourceReferenced());

  auto* other_resource =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(), {});
  other_resource_ptr = other_resource;
  EXPECT_EQ(other_resource_ptr.Get(), other_resource);
  EXPECT_TRUE(other_resource->IsResourceReferenced());
  EXPECT_FALSE(resource->IsResourceReferenced());

  resource_ptr = resource;
  other_resource_ptr = resource_ptr;
  EXPECT_EQ(resource_ptr.Get(), other_resource_ptr.Get());
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_FALSE(other_resource->IsResourceReferenced());
}

TEST(ResourceTest, ResourcePtrMove) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  ResourcePtr<TestResource> resource_ptr(resource);
  ResourcePtr<TestResource> other_resource_ptr(std::move(resource_ptr));
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  other_resource_ptr = std::move(other_resource_ptr);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  resource_ptr = std::move(other_resource_ptr);
  EXPECT_EQ(resource_ptr.Get(), resource);
  EXPECT_EQ(other_resource_ptr.Get(), nullptr);
  EXPECT_TRUE(resource->IsResourceReferenced());

  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  other_resource_ptr = other_resource;
  EXPECT_EQ(other_resource_ptr.Get(), other_resource);
  EXPECT_TRUE(other_resource->IsResourceReferenced());

  other_resource_ptr = std::move(resource_ptr);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());
  EXPECT_FALSE(other_resource->IsResourceReferenced());
}

TEST(ResourceTest, ResourcePtrMoveDerived) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<ResourceA>(&manager));

  auto* resource =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(), {});
  ResourcePtr<ResourceA> resource_ptr(resource);
  ResourcePtr<TestResource> other_resource_ptr(std::move(resource_ptr));
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  resource_ptr = resource;
  other_resource_ptr = std::move(resource_ptr);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  EXPECT_TRUE(resource->IsResourceReferenced());

  auto* other_resource =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(), {});
  resource_ptr = other_resource;
  other_resource_ptr = std::move(resource_ptr);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  EXPECT_EQ(other_resource_ptr.Get(), other_resource);
  EXPECT_FALSE(resource->IsResourceReferenced());
  EXPECT_TRUE(other_resource->IsResourceReferenced());
}

TEST(ResourceTest, MaybeDeleteSucceedsOnNull) {
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));
  EXPECT_TRUE(manager.MaybeDeleteResource(nullptr));
}

TEST(ResourceTest, MaybeDeleteFailsIfDifferentSystem) {
  TestResource::Counts counts;

  auto system_1 = ResourceSystem::Create();
  ASSERT_NE(system_1, nullptr);
  ResourceManager manager_1;
  EXPECT_TRUE(system_1->Register<TestResource>(&manager_1));

  auto system_2 = ResourceSystem::Create();
  ASSERT_NE(system_2, nullptr);
  ResourceManager manager_2;
  EXPECT_TRUE(system_2->Register<TestResource>(&manager_2));

  auto* resource =
      new TestResource(&counts, manager_1.NewResourceEntry<TestResource>(), {});
  EXPECT_FALSE(manager_2.MaybeDeleteResource(resource));
}

TEST(ResourceTest, MaybeDeleteFailsOnUnmanagedType) {
  TestResource::Counts counts;

  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager_1;
  EXPECT_TRUE(system->Register<ResourceA>(&manager_1));
  ResourceManager manager_2;
  EXPECT_TRUE(system->Register<ResourceB>(&manager_2));

  auto* resource =
      new ResourceA(&counts, manager_1.NewResourceEntry<ResourceA>(), {});
  EXPECT_FALSE(manager_2.MaybeDeleteResource(resource));
}

TEST(ResourceTest, MaybeDeleteFailsIfReferenced) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  ResourcePtr<TestResource> resource_ptr(resource);
  EXPECT_FALSE(manager.MaybeDeleteResource(resource));
  EXPECT_TRUE(resource->IsResourceReferenced());
}

TEST(ResourceTest, DeleteNewlyCreated) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  resource->Delete();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ResourceTest, DeleteReferencedButNeverVisible) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));
  ResourcePtr<TestResource> resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  TestResource* resource_ptr = resource.Get();
  resource = nullptr;
  EXPECT_EQ(counts.destruct, 0);
  resource_ptr->Delete();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ResourceTest, GetResourceById) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  const ResourceId resource_id = resource->GetResourceId();

  // Resource is not in the system until it is visible.
  auto resource_ptr = system->Get<TestResource>(resource_id);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
  resource->SetResourceVisible();
  resource_ptr = system->Get<TestResource>(resource_id);
  EXPECT_EQ(resource_ptr.Get(), resource);

  // Removing all references, it is still in the system.
  resource_ptr.Reset();
  EXPECT_FALSE(resource->IsResourceReferenced());
  resource_ptr = system->Get<TestResource>(resource_id);
  EXPECT_EQ(resource_ptr.Get(), resource);

  // Deleting the resource removes it from the system.
  resource_ptr.Reset();
  EXPECT_TRUE(manager.MaybeDeleteResource(resource));
  resource_ptr = system->Get<TestResource>(resource_id);
  EXPECT_EQ(resource_ptr.Get(), nullptr);
}

TEST(ResourceTest, AutoVisibleWorks) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  // Without auto-visible, adding a reference does not make it visibile.
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  ResourcePtr<TestResource> resource_ptr(resource);
  ResourcePtr<TestResource> other_resource_ptr =
      system->Get<TestResource>(resource->GetResourceId());
  EXPECT_EQ(other_resource_ptr.Get(), nullptr);

  // With auto-visible, it does make it visible on first reference.
  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(),
                       ResourceFlag::kAutoVisible);
  resource_ptr = other_resource;
  other_resource_ptr =
      system->Get<TestResource>(other_resource->GetResourceId());
  EXPECT_EQ(other_resource_ptr.Get(), other_resource);
}

TEST(ResourceTest, AutoReleaseWorks) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  int release = 0;
  manager.InitGenericReleaseHandler(
      [&release](Resource* resource) { ++release; });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  // Without auto-release, removing a reference does not trigger release.
  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(), {});
  ResourcePtr<TestResource> resource_ptr(resource);
  resource_ptr.Reset();
  EXPECT_EQ(release, 0);

  // With auto-release, it does make it trigger release on last reference.
  auto* other_resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(),
                       ResourceFlag::kAutoRelease);
  resource_ptr = other_resource;
  ResourcePtr<TestResource> other_resource_ptr = resource_ptr;
  resource_ptr.Reset();
  EXPECT_EQ(release, 0);
  other_resource_ptr.Reset();
  EXPECT_EQ(release, 1);
}

TEST(ResourceTest, ManagerDefaultFlagBehavior) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  int release = 0;
  manager.InitGenericReleaseHandler(
      [&release](Resource* resource) { ++release; });
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  auto* resource =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>());
  ResourcePtr<TestResource> resource_ptr(resource);
  ResourcePtr<TestResource> other_resource_ptr =
      system->Get<TestResource>(resource->GetResourceId());
  EXPECT_EQ(other_resource_ptr.Get(), resource);
  resource_ptr.Reset();
  other_resource_ptr.Reset();
  EXPECT_EQ(release, 1);
}

TEST(ResourceTest, DefaultReleaseIsDelete) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE(system->Register<TestResource>(&manager));

  ResourcePtr<TestResource> resource_ptr =
      new TestResource(&counts, manager.NewResourceEntry<TestResource>(),
                       ResourceFlag::kAutoRelease);
  resource_ptr.Reset();
  EXPECT_EQ(counts.destruct, 1);
}

TEST(ResourceTest, TypeSpecificReleaseHandler) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  int generic_release = 0;
  manager.InitGenericReleaseHandler(
      [&generic_release](Resource* resource) { ++generic_release; });
  int typed_release = 0;
  manager.InitReleaseHandler<ResourceA>(
      [&typed_release](ResourceA* resource) { ++typed_release; });
  EXPECT_TRUE((system->Register<ResourceA, ResourceB>(&manager)));

  ResourcePtr<ResourceA> resource_a_ptr =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(),
                    ResourceFlag::kAutoRelease);
  resource_a_ptr.Reset();
  EXPECT_EQ(typed_release, 1);
  EXPECT_EQ(generic_release, 0);

  ResourcePtr<ResourceB> resource_b_ptr =
      new ResourceB(&counts, manager.NewResourceEntry<ResourceB>(),
                    ResourceFlag::kAutoRelease);
  resource_b_ptr.Reset();
  EXPECT_EQ(typed_release, 1);
  EXPECT_EQ(generic_release, 1);
}

TEST(ResourceTest, DuplicateInitReleaseHandlerFails) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  int generic_release = 0;
  manager.InitGenericReleaseHandler(
      [&generic_release](Resource* resource) { ++generic_release; });
  manager.InitGenericReleaseHandler(
      [&generic_release](Resource* resource) { generic_release += 100; });
  int typed_release = 0;
  manager.InitReleaseHandler<ResourceA>(
      [&typed_release](ResourceA* resource) { ++typed_release; });
  manager.InitReleaseHandler<ResourceA>(
      [&typed_release](ResourceA* resource) { typed_release += 100; });
  EXPECT_TRUE((system->Register<ResourceA, ResourceB>(&manager)));

  ResourcePtr<ResourceA> resource_a_ptr =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(),
                    ResourceFlag::kAutoRelease);
  resource_a_ptr.Reset();
  EXPECT_EQ(typed_release, 1);
  EXPECT_EQ(generic_release, 0);

  ResourcePtr<ResourceB> resource_b_ptr =
      new ResourceB(&counts, manager.NewResourceEntry<ResourceB>(),
                    ResourceFlag::kAutoRelease);
  resource_b_ptr.Reset();
  EXPECT_EQ(typed_release, 1);
  EXPECT_EQ(generic_release, 1);
}

TEST(ResourceTest, InitReleaseHandlerAfterRegisterFails) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceManager manager;
  EXPECT_TRUE((system->Register<ResourceA, ResourceB>(&manager)));
  int generic_release = 0;
  manager.InitGenericReleaseHandler(
      [&generic_release](Resource* resource) { ++generic_release; });
  int typed_release = 0;
  manager.InitReleaseHandler<ResourceA>(
      [&typed_release](ResourceA* resource) { ++typed_release; });

  ResourcePtr<ResourceA> resource_a_ptr =
      new ResourceA(&counts, manager.NewResourceEntry<ResourceA>(),
                    ResourceFlag::kAutoRelease);
  resource_a_ptr.Reset();
  EXPECT_EQ(typed_release, 0);
  EXPECT_EQ(generic_release, 0);

  ResourcePtr<ResourceB> resource_b_ptr =
      new ResourceB(&counts, manager.NewResourceEntry<ResourceB>(),
                    ResourceFlag::kAutoRelease);
  resource_b_ptr.Reset();
  EXPECT_EQ(typed_release, 0);
  EXPECT_EQ(generic_release, 0);
}

TEST(ResourceTest, ManagerDestructEdgeConditions) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceSystem* system_ptr = system.get();
  auto manager = std::make_unique<ResourceManager>();
  EXPECT_TRUE((system->Register<TestResource>(manager.get())));

  TestResource* resource = new TestResource(
      &counts, manager->NewResourceEntry<TestResource>(),
      {ResourceFlag::kAutoRelease, ResourceFlag::kAutoVisible});
  ResourceId resource_id = resource->GetResourceId();
  resource->SetDeleteCallback([&] {
    EXPECT_EQ(resource->GetResourceSystem(), system_ptr);
    EXPECT_EQ(resource->GetResourceType(), TypeKey::Get<TestResource>());
    EXPECT_EQ(resource->GetResourceId(), resource_id);
    EXPECT_THAT(resource->GetResourceName(), IsEmpty());

    auto resource_ptr = system_ptr->Get<TestResource>(resource_id);
    EXPECT_EQ(resource_ptr, nullptr);
  });
  manager.reset();
}

TEST(ResourceTest, SystemDestructEdgeConditions) {
  TestResource::Counts counts;
  auto system = ResourceSystem::Create();
  ASSERT_NE(system, nullptr);
  ResourceSystem* system_ptr = system.get();
  auto manager = std::make_unique<ResourceManager>();
  EXPECT_TRUE((system->Register<TestResource>(manager.get())));

  TestResource* resource = new TestResource(
      &counts, manager->NewResourceEntry<TestResource>(),
      {ResourceFlag::kAutoRelease, ResourceFlag::kAutoVisible});
  ResourceId resource_id = resource->GetResourceId();
  resource->SetDeleteCallback([&] {
    EXPECT_EQ(resource->GetResourceSystem(), system_ptr);
    EXPECT_EQ(resource->GetResourceType(), TypeKey::Get<TestResource>());
    EXPECT_EQ(resource->GetResourceId(), resource_id);
    EXPECT_THAT(resource->GetResourceName(), IsEmpty());

    auto resource_ptr = system_ptr->Get<TestResource>(resource_id);
    EXPECT_EQ(resource_ptr, nullptr);
  });
  system.reset();
}

}  // namespace
}  // namespace gb
