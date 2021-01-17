// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_FILE_RESOURCE_FILE_WRITER_H_
#define GB_RESOURCE_FILE_RESOURCE_FILE_WRITER_H_

#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "flatbuffers/flatbuffers.h"
#include "gb/base/callback.h"
#include "gb/base/validated_context.h"
#include "gb/file/chunk_writer.h"
#include "gb/file/file_types.h"
#include "gb/resource/resource_types.h"

namespace gb {

// This class supports saving resources to chunk files.
//
// Resource files are chunk files that conform to the following:
//    Chunk "GBFI"
//      size: 0
//      version: 1
//      file: "XXXX"  <-- The chunk type of the resource this file is for.
//    Chunk "GBRL"    <-- Optional resource list of dependencies.
//    Chunk "...."    <-- Zero or more generic or embedded resource chunks.
//    Chunk "XXXX"    <-- Specified resource. This is the returned resource.
//
// To use this class, call Create and re4gister one or more resource writers.
// Call Write to write a resource out to a file.
//
// A context is passed in to all writers. This may be used as desired to store
// data needed for the writer. The ResourceFileWriter does not add anything to
// the context beyond what the caller adds.
class ResourceFileWriter final {
 public:
  // A resource writer must write the resource out to one or more chunks in
  // out_chunks. These will be written to the same order.
  template <typename ResourceType>
  using ResourceWriter = Callback<bool(Context* context, ResourceType* resource,
                                       std::vector<ChunkWriter>* out_chunks)>;

  // A resource writer must write the resource to the provided flat buffer
  // builder.
  //
  // If writing the resource is successful, the writer must call Finish on the
  // builder before returning true.
  template <typename ResourceType>
  using ResourceFlatBufferWriter =
      Callback<bool(Context* context, ResourceType* resource,
                    flatbuffers::FlatBufferBuilder* builder)>;

  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: FileSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // REQUIRED: ResourceSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kInRequired,
                               ResourceSystem);

  // Contract for creating a new ResourceFileWriter
  using CreateContract =
      ContextContract<kConstraintFileSystem, kConstraintResourceSystem>;

  // OPTIONAL: Determines whether the resource name is updated when a resource
  // is written. By default, the resource name is updated.
  static inline constexpr const char* kKeySetResourceName = "SetResourceName";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(kConstraintSetResourceName,
                                             kInOptional, bool,
                                             kKeySetResourceName, true);

  // OPTIONAL: Determines whether the resource may be saved if it has unnamed
  // resource dependencies. By default, this is not allowed as it likely could
  // never be loaded again.
  static inline constexpr const char* kKeyAllowUnnamedDependencies =
      "AllowUnnamedDependencies";
  static GB_CONTEXT_CONSTRAINT_NAMED_DEFAULT(
      kConstraintAllowUnnamedDependencies, kInOptional, bool,
      kKeyAllowUnnamedDependencies, false);

  // SCOPED: This is set to the resource name being written to. It may be used
  // for error messages, or may be used to determine the correct resource ID to
  // write into the file (this is recommended to avoid resource ID collisions
  // between resource names).
  static inline constexpr const char* kKeyResourceName = "ResourceName";
  static GB_CONTEXT_CONSTRAINT_NAMED(kConstraintResourceName, kScoped,
                                     std::string, kKeyResourceName);

  // Contract for Write calls.
  using WriteContract = ContextContract<kConstraintSetResourceName,
                                        kConstraintAllowUnnamedDependencies,
                                        kConstraintResourceName>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new ResourceFileWriter.
  static std::unique_ptr<ResourceFileWriter> Create(CreateContract contract);

  ResourceFileWriter(const ResourceFileWriter&) = delete;
  ResourceFileWriter(ResourceFileWriter&&) = delete;
  ResourceFileWriter& operator=(const ResourceFileWriter&) = delete;
  ResourceFileWriter& operator=(ResourceFileWriter&&) = delete;
  ~ResourceFileWriter();

  //----------------------------------------------------------------------------
  // Resource writer registration
  //----------------------------------------------------------------------------

  // Registers a resource writer.
  //
  // The ResourceType must be a derived class of Resource. This is validated at
  // compile time.
  //
  // See ResourceWriter for details on the writer callback itself.
  template <typename ResourceType>
  bool RegisterResourceWriter(const ChunkType& chunk_type,
                              ResourceWriter<ResourceType> writer);

  // Registers a resource writer based on flat buffers.
  //
  // The ResourceType must be a derived class of Resource. This is validated at
  // compile time.
  //
  // See ResourceFlatBufferWriter for details on the writer callback itself.
  template <typename ResourceType>
  bool RegisterResourceFlatBufferWriter(
      const ChunkType& chunk_type, int32_t version,
      ResourceFlatBufferWriter<ResourceType> writer);

  //----------------------------------------------------------------------------
  // File writing
  //----------------------------------------------------------------------------

  // Writes the specified to the name.
  bool Write(std::string_view name, Resource* resource,
             WriteContract contract = Context());

 private:
  static inline constexpr int kInitFlatBufferSize = 16 * 1024;

  using FlatBuffers = std::vector<flatbuffers::FlatBufferBuilder>;
  using GenericWriter = Callback<bool(Context* context, Resource* resource,
                                      std::vector<ChunkWriter>* out_chunks,
                                      FlatBuffers* flat_buffers)>;
  struct WriterInfo {
    ChunkType chunk_type;
    GenericWriter writer;
  };

  // Custom allocator to reserve room in the flat buffer for the required
  // ResourceId in the chunk.
  class ResourceFlatBufferAllocator : public flatbuffers::Allocator {
   public:
    static inline constexpr size_t kExtraSpace = sizeof(ResourceId);
    uint8_t* allocate(size_t size) override {
      uint8_t* ptr = new uint8_t[size + kExtraSpace];
      return ptr + kExtraSpace;
    }
    void deallocate(uint8_t* ptr, size_t size) override {
      uint8_t* const actual_ptr = ptr - kExtraSpace;
      delete[] actual_ptr;
    }
  };

  ResourceFileWriter(ValidatedContext context);

  bool DoRegisterResourceWriter(const ChunkType& chunk_type, TypeKey* type,
                                GenericWriter writer);

  ValidatedContext context_;
  absl::flat_hash_map<TypeKey*, WriterInfo> writers_;
  ResourceFlatBufferAllocator flat_buffer_allocator_;
};

//==============================================================================
// Template / inline implementation
//==============================================================================

template <typename ResourceType>
bool ResourceFileWriter::RegisterResourceWriter(
    const ChunkType& chunk_type, ResourceWriter<ResourceType> writer) {
  static_assert(std::is_base_of_v<Resource, ResourceType>,
                "ResourceType must be a Resource");
  return DoRegisterResourceWriter(
      chunk_type, TypeKey::Get<ResourceType>(),
      [writer = std::move(writer)](Context* context, Resource* resource,
                                   std::vector<ChunkWriter>* out_chunks,
                                   FlatBuffers*) {
        return writer(context, static_cast<ResourceType*>(resource),
                      out_chunks);
      });
}

template <typename ResourceType>
bool ResourceFileWriter::RegisterResourceFlatBufferWriter(
    const ChunkType& chunk_type, int32_t version,
    ResourceFlatBufferWriter<ResourceType> writer) {
  static_assert(std::is_base_of_v<Resource, ResourceType>,
                "ResourceType must be a Resource");
  return DoRegisterResourceWriter(
      chunk_type, TypeKey::Get<ResourceType>(),
      [this, chunk_type, version, writer = std::move(writer)](
          Context* context, Resource* resource,
          std::vector<ChunkWriter>* out_chunks, FlatBuffers* flat_buffers) {
        flatbuffers::FlatBufferBuilder builder(kInitFlatBufferSize,
                                               &flat_buffer_allocator_);
        if (!writer(context, static_cast<ResourceType*>(resource), &builder)) {
          return false;
        }
        ResourceId* id =
            reinterpret_cast<ResourceId*>(builder.GetBufferPointer()) - 1;
        *id = resource->GetResourceId();
        out_chunks->emplace_back(ChunkWriter::New(
            chunk_type, version, id, builder.GetSize() + sizeof(ResourceId)));
        flat_buffers->emplace_back(std::move(builder));
        return true;
      });
}

}  // namespace gb

#endif  // GB_RESOURCE_FILE_RESOURCE_FILE_WRITER_H_
