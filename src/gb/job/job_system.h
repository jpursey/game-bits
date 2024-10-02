// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_JOB_JOB_SYSTEM_H_
#define GB_JOB_JOB_SYSTEM_H_

#include <string_view>

#include "absl/log/log.h"
#include "gb/base/callback.h"
#include "gb/base/context.h"
#include "gb/job/job_counter.h"
#include "gb/job/job_types.h"

namespace gb {

// Handle to a job data slot within a job system.
using JobDataHandle = size_t;

// Marker for an explicitly invalid job data handle.
inline constexpr size_t kInvalidJobDataHandle = 0;

// The maximum number of job data handles that can be created per job system.
inline constexpr size_t kMaxJobDataHandles = 128;

// The JobSystem class is a scheduler for a set of jobs which are run on one or
// more job threads.
//
// Jobs may be scheduled from any thread by calling the Run method, which will
// asynchronously execute the job on a job thread. Most other operations are
// only callable from within a job system thread. To determine the which job
// system a thread is associated with (if any), call JobSystem::Get().
//
// This class is thread-safe.
class JobSystem {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  JobSystem(const JobSystem&) = delete;
  JobSystem(JobSystem&&) = delete;
  JobSystem& operator=(const JobSystem&) = delete;
  JobSystem& operator=(JobSystem&&) = delete;
  virtual ~JobSystem() = default;

  // Returns the JobSystem for the current thread, iff it is a job system
  // managed thread. If the thread is *not* a job thread, this returns nullptr.
  static JobSystem* Get();

  //----------------------------------------------------------------------------
  // Job data
  //
  // Arbitrary per-job data can be stored by a job system. This is similar to
  // thread-local storage, but for jobs. It has the same caveats that allocating
  // space may impact *all* jobs so it should be used sparingly. See GetData()
  // for details.
  //
  // If arbitrary context-specific per-job data is desired, use GetContext()
  // instead to retrieve a per-job Context object. This context may be
  // initialized when calling Run.
  //----------------------------------------------------------------------------

  // Allocates a data handle which can be used in future calls to GetData within
  // any job.
  //
  // Up to kMaxJobDataHandles handles may be created. If this limit is reached,
  // then AllocDataHandle() will return kInvalidJobDataHandle.
  //
  // An optional callback may be specified to do the per-job creation the data.
  // If not specified, the default constructor and destructor for the Type will
  // be invoked.
  //
  // Any data created for a job will be constructed within the context of the
  // job, so the job's context and other job data can be queried. However, job
  // data destruction should be as trivial as possible. It may not be destructed
  // within the context of the job running, and the job system itself may be in
  // the process of getting destructed. The only guarantees are that the job's
  // context will not yet be destructed (a pointer to the context may be
  // cached), and destruction of data in a job will occur before any blocked
  // jobs resume.
  template <typename Type>
  JobDataHandle AllocDataHandle();
  template <typename Type>
  JobDataHandle AllocDataHandle(Callback<Type*()> create);

  //----------------------------------------------------------------------------
  // Job execution
  //
  // These may called from any thread.
  //----------------------------------------------------------------------------

  // Runs a single job.
  //
  // If a counter is provided, it must outlive the job. Counters can only be
  // used within a single JobSystem.
  //
  // If a context is provided, the job will be initialized with that context,
  // which can be retrieved within the job by calling JobSystem::GetContext.
  bool Run(std::string_view name, JobCounter* counter,
           Callback<void()> callback);
  bool Run(JobCounter* counter, Callback<void()> callback);
  bool Run(std::string_view name, Callback<void()> callback);
  bool Run(Callback<void()> callback);
  bool Run(std::string_view name, JobCounter* counter, Context context,
           Callback<void()> callback);
  bool Run(JobCounter* counter, Context context, Callback<void()> callback);
  bool Run(std::string_view name, Context context, Callback<void()> callback);
  bool Run(Context context, Callback<void()> callback);

  //----------------------------------------------------------------------------
  // Job operations
  //
  // These may only be called within the context of a job callback.
  //----------------------------------------------------------------------------

  // Blocks this job, waiting until the JobCounter reaches zero.
  //
  // Only counters for jobs run in this job system can be waited on.
  static void Wait(JobCounter* counter);

  // Returns the job specific context.
  //
  // If no context was set for this job, then an empty context will be created
  // and returned.
  static Context& GetContext();

  // Gets or creates job-local data.
  //
  // Returns the requested job-specific data for the specified job handle,
  // creating it if it doesn't already exist.
  //
  // The handle and type *must* be valid (they must match the type and return
  // value of a previous call to AllocDataHandle).
  template <typename Type>
  static Type* GetData(JobDataHandle handle);

 protected:
  JobSystem() {
    // This ensures no reallocation will take place during AllocDataHandle.
    job_data_types_.reserve(kMaxJobDataHandles);
  }

  using JobData = std::vector<void*>;

  //----------------------------------------------------------------------------
  // Derived class interface
  //----------------------------------------------------------------------------

  // Frees job data. This must be called at job destruction
  void FreeJobData(JobData& job_data);

  // Sets the job system for the current thread. Derived class must call this
  // for each thread they run jobs on.
  void SetThreadState();

  virtual bool DoRun(std::string_view name, JobCounter* counter,
                     Context* context, Callback<void()> callback) = 0;
  virtual void DoWait(JobCounter* counter) = 0;
  virtual Context& DoGetContext() = 0;
  virtual JobData& DoGetJobData() = 0;

 private:
  struct JobDataType {
    JobDataType(TypeInfo* in_type, Callback<void*()> in_alloc)
        : type(in_type), alloc(std::move(in_alloc)) {}
    TypeInfo* type;
    Callback<void*()> alloc;
  };

  absl::Mutex job_data_mutex_;
  std::vector<JobDataType> job_data_types_;
};

template <typename Type>
JobDataHandle JobSystem::AllocDataHandle() {
  absl::WriterMutexLock lock(&job_data_mutex_);
  if (job_data_types_.size() == kMaxJobDataHandles) {
    return kInvalidJobDataHandle;
  }
  job_data_types_.emplace_back(
      TypeInfo::Get<Type>(), +[]() { return new Type(); });
  return job_data_types_.size();
}

template <typename Type>
JobDataHandle JobSystem::AllocDataHandle(Callback<Type*()> create) {
  absl::WriterMutexLock lock(&job_data_mutex_);
  if (job_data_types_.size() == kMaxJobDataHandles) {
    return kInvalidJobDataHandle;
  }
  job_data_types_.emplace_back(TypeInfo::Get<Type>(), std::move(create));
  return job_data_types_.size();
}

inline bool JobSystem::Run(std::string_view name, JobCounter* counter,
                           Callback<void()> callback) {
  return DoRun(name, counter, nullptr, std::move(callback));
}

inline bool JobSystem::Run(JobCounter* counter, Callback<void()> callback) {
  return DoRun({}, counter, nullptr, std::move(callback));
}

inline bool JobSystem::Run(std::string_view name, Callback<void()> callback) {
  return DoRun(name, nullptr, nullptr, std::move(callback));
}

inline bool JobSystem::Run(Callback<void()> callback) {
  return DoRun({}, nullptr, nullptr, std::move(callback));
}

inline bool JobSystem::Run(std::string_view name, JobCounter* counter,
                           Context context, Callback<void()> callback) {
  return DoRun(name, counter, &context, std::move(callback));
}

inline bool JobSystem::Run(JobCounter* counter, Context context,
                           Callback<void()> callback) {
  return DoRun({}, counter, &context, std::move(callback));
}

inline bool JobSystem::Run(std::string_view name, Context context,
                           Callback<void()> callback) {
  return DoRun(name, nullptr, &context, std::move(callback));
}

inline bool JobSystem::Run(Context context, Callback<void()> callback) {
  return DoRun({}, nullptr, &context, std::move(callback));
}

inline void JobSystem::Wait(JobCounter* counter) { Get()->DoWait(counter); }

inline Context& JobSystem::GetContext() { return Get()->DoGetContext(); }

template <typename Type>
Type* JobSystem::GetData(JobDataHandle handle) {
  JobSystem* self = Get();
  DCHECK(handle > 0 && handle < kMaxJobDataHandles);
  const auto index = handle - 1;
  JobData& job_data = self->DoGetJobData();
  if (job_data.size() <= index) {
    job_data.resize(handle);
  }
  void*& data = job_data[index];
  if (data != nullptr) {
    return static_cast<Type*>(data);
  }
  Callback<void*()>* alloc;
  {
    absl::ReaderMutexLock lock(&self->job_data_mutex_);
    DCHECK(handle <= self->job_data_types_.size() &&
           self->job_data_types_[index].type == TypeInfo::Get<Type>());
    alloc = &self->job_data_types_[index].alloc;
  }
  // We can no longer use "data", as the alloc function may end up recursing
  // into this function to retrieve other data.
  job_data[index] = (*alloc)();
  return static_cast<Type*>(job_data[index]);
}

}  // namespace gb

#endif  // GB_JOB_JOB_SYSTEM_H_
