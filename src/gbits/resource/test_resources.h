#ifndef GBITS_RESOURCE_TEST_RESOURCES_H_
#define GBITS_RESOURCE_TEST_RESOURCES_H_

#include <vector>

#include "gbits/base/callback.h"
#include "gbits/resource/resource.h"

namespace gb {

class TestResource : public Resource {
 public:
  struct Counts {
    Counts() = default;
    int construct = 0;
    int destruct = 0;
  };

  TestResource(ResourceEntry entry) : Resource(std::move(entry)) {}
  TestResource(Counts* counts, ResourceEntry entry)
      : Resource(std::move(entry)) {
    counts_ = counts;
    counts_->construct += 1;
  }
  TestResource(Counts* counts, ResourceEntry entry, ResourceFlags flags)
      : Resource(std::move(entry), flags) {
    counts_ = counts;
    counts_->construct += 1;
  }

  void SetDeleteCallback(Callback<void()> callback) {
    delete_callback_ = std::move(callback);
  }

  void SetResourceDependencies(std::vector<TestResource*> dependencies) {
    dependencies_ = dependencies;
  }
  void GetResourceDependencies(
      ResourceDependencyList* dependencies) const override {
    dependencies->insert(dependencies->end(), dependencies_.begin(),
                         dependencies_.end());
  }

 protected:
  ~TestResource() override {
    if (counts_ != nullptr) {
      counts_->destruct += 1;
    }
    if (delete_callback_) {
      delete_callback_();
    }
  }

 private:
  Counts* counts_ = nullptr;
  Callback<void()> delete_callback_;
  std::vector<TestResource*> dependencies_;
};

class ResourceA : public TestResource {
 public:
  using TestResource::TestResource;

 private:
  ~ResourceA() override = default;
};

class ResourceB : public TestResource {
 public:
  using TestResource::TestResource;

 private:
  ~ResourceB() override = default;
};

class ResourceC : public TestResource {
 public:
  using TestResource::TestResource;

 private:
  ~ResourceC() override = default;
};

}  // namespace gb

#endif  // GBITS_RESOURCE_TEST_RESOURCES_H_
