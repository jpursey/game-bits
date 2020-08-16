// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_FILE_RESOURCE_FILE_WRITER_H_
#define GB_RESOURCE_FILE_RESOURCE_FILE_WRITER_H_

#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
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

  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: FileSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // Contract for creating a new ResourceFileWriter
  using CreateContract = ContextContract<kConstraintFileSystem>;

  // Contract for Write calls.
  using WriteContract = ContextContract<>;

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

  //----------------------------------------------------------------------------
  // File writing
  //----------------------------------------------------------------------------

  // Writes the specified to the name.
  bool Write(std::string_view name, Resource* resource,
             WriteContract contract = Context());

 private:
  using GenericWriter = Callback<bool(Context* context, Resource* resource,
                                      std::vector<ChunkWriter>* out_chunks)>;
  struct WriterInfo {
    ChunkType chunk_type;
    GenericWriter writer;
  };

  ResourceFileWriter(ValidatedContext context);

  bool DoRegisterResourceWriter(const ChunkType& chunk_type, TypeKey* type,
                                GenericWriter writer);

  ValidatedContext context_;
  absl::flat_hash_map<TypeKey*, WriterInfo> writers_;
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
                                   std::vector<ChunkWriter>* out_chunks) {
        return writer(context, static_cast<ResourceType*>(resource),
                      out_chunks);
      });
}

}  // namespace gb

#endif  // GB_RESOURCE_FILE_RESOURCE_FILE_WRITER_H_
