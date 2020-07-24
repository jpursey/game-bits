#include "gbits/resource/resource.h"

#include "gbits/resource/resource_system.h"
#include "glog/logging.h"

namespace gb {

Resource::Resource(ResourceEntry entry, ResourceFlags flags)
    : entry_(std::move(entry)), flags_(flags) {
  entry_.GetSystem()->AddResource({}, this);
}

Resource::~Resource() {
  LOG_IF(ERROR, ref_count_ != 0)
      << "Resource " << entry_.GetType()->GetTypeName() << "(" << entry_.GetId()
      << ") is getting deleted with " << ref_count_
      << " references (including manager reference)";
  // ResourceEntry destructor will remove the resource from the system.
}

void Resource::SetResourceVisible(bool visible) {
  if (visible) {
    mutex_.Lock();
    if (state_ == State::kNew) {
      state_ = State::kActive;
    }
    mutex_.Unlock();
  }
  entry_.GetSystem()->SetResourceVisible({}, this, visible);
}

bool Resource::MaybeDelete(ResourceInternal) {
  // We have to lock the ResourceSystem while we attempt to delete the Resource,
  // in order to synchronize with any resource lookups that could be happening
  // at the same time.
  entry_.GetSystem()->ResourceLock({});
  mutex_.Lock();
  const bool in_release =
      (flags_.IsSet(ResourceFlag::kAutoRelease) && state_ == State::kReleasing);
  int expected = (in_release ? 2 : 1);
  bool can_delete = ref_count_.compare_exchange_strong(expected, 0);
  if (can_delete) {
    state_ = State::kDeleting;
  }
  mutex_.Unlock();
  entry_.GetSystem()->ResourceUnlock({});

  // If we are in a release method, we need to defer the deletion until this
  // Resource is no longer in the callstack;
  if (can_delete && !in_release) {
    delete this;
  }
  return can_delete;
}

void Resource::DoAutoVisible() {
  ++ref_count_;
  bool make_visible = false;

  mutex_.Lock();
  if (state_ == State::kNew) {
    make_visible = true;
    state_ = State::kActive;
  }
  mutex_.Unlock();

  if (make_visible) {
    entry_.GetSystem()->SetResourceVisible({}, this, true);
  }
}

void Resource::Release() {
  mutex_.Lock();
  state_ = State::kReleasing;

  // Trigger any registered release behavior.
  mutex_.Unlock();
  entry_.GetSystem()->ReleaseResource({}, this);
  mutex_.Lock();

  // If the reference count is zero, then MaybeDelete was successfully called
  // inside the release callback, so we need to delete ourselves.
  if (state_ == State::kDeleting) {
    mutex_.Unlock();
    delete this;
    return;
  }

  --ref_count_;
  state_ = State::kActive;
  mutex_.Unlock();
}

void Resource::Delete() {
  mutex_.Lock();
  CHECK(state_ == State::kNew) << "Resource was visibile in resource system "
                                  "and cannot be force deleted.";
  int expected = 1;
  CHECK(ref_count_.compare_exchange_strong(expected, 0))
      << "Resource is referenced already and cannot be force deleted.";
  state_ = State::kDeleting;
  mutex_.Unlock();
  delete this;
}

}  // namespace gb
