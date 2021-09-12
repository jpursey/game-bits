// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_RESOURCE_RESOURCE_FILE_READER_H_
#define GB_RESOURCE_RESOURCE_FILE_READER_H_

#include <type_traits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "flatbuffers/flatbuffers.h"
#include "gb/base/callback.h"
#include "gb/base/validated_context.h"
#include "gb/file/chunk_reader.h"
#include "gb/file/file_types.h"
#include "gb/resource/resource_entry.h"
#include "gb/resource/resource_types.h"

namespace gb {

//==============================================================================
// FileResources
//==============================================================================

// Contains a set of resources discovered or loaded by the ResourceFileReader.
//
// This is always available in the context passed to chunk readers, and should
// be used to look up dependent resources, instead of looking up resources via
// the ResourceSystem directly. The reason for this is two-fold:
//  1. Resources that are currently being loaded from the current file are
//     generally not yet visible in the resource system, so they cannot be
//     looked up that way.
//  2. The resource system only provides resources in a ResourcePtr or
//     ResourceSet, which may make the dependent resource visible before it is
//     ready and also could result in premature deletion if the reference is
//     released.
//
// Access this in a chunk reader, by calling:
//    auto* file_resources = context->GetPtr<FileResources>();
class FileResources final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  FileResources() = default;
  FileResources(const FileResources&) = delete;
  FileResources(FileResources&&) = delete;
  FileResources& operator=(const FileResources&) = delete;
  FileResources& operator=(FileResources&&) = delete;
  ~FileResources() = default;

  //----------------------------------------------------------------------------
  // Accessors
  //----------------------------------------------------------------------------

  template <typename Type>
  Type* GetResource(ResourceId id) const;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  void AddResource(ResourceInternal, TypeKey* type, ResourceId id,
                   Resource* resource);

 private:
  using ResourceKey = std::tuple<TypeKey*, ResourceId>;
  absl::flat_hash_map<ResourceKey, Resource*> resources_;
};

//==============================================================================
// FileResourceChunks
//==============================================================================

// Contains all chunks of registered chunk types loaded so far by the
// ResourceFileReader.
//
// This is always available in the context passed to chunk readers. Both generic
// and resource chunks may be read from this list. Access this in a chunk
// reader, by calling:
//    auto* file_chunks = context->GetPtr<ResourceFileChunks>();
class ResourceFileChunks final {
 public:
  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  ResourceFileChunks() = default;
  ResourceFileChunks(const ResourceFileChunks&) = delete;
  ResourceFileChunks(ResourceFileChunks&&) = delete;
  ResourceFileChunks& operator=(const ResourceFileChunks&) = delete;
  ResourceFileChunks& operator=(ResourceFileChunks&&) = delete;
  ~ResourceFileChunks();

  //----------------------------------------------------------------------------
  // Accessors
  //----------------------------------------------------------------------------

  // Returns the first chunk of the requested type, or null if is not loaded.
  //
  // If version is not null, the chunk version will also be returned.
  template <typename ChunkStruct>
  ChunkStruct* GetChunk(const ChunkType& type, int* version = nullptr) const;

  // Returns the nth chunk of the requested type, or null if there is no chunk
  // of the requested type and index.
  //
  // If version is not null, the chunk version will also be returned.
  template <typename ChunkStruct>
  ChunkStruct* GetChunk(int index, const ChunkType& type,
                        int* version = nullptr) const;

  // Returns all chunks of the specified type, in the order they present in the
  // file.
  //
  // If version is not null, the chunk version will also be returned.
  template <typename ChunkStruct>
  absl::Span<ChunkStruct* const> GetChunks(const ChunkType& type,
                                           int* version = nullptr) const;

  //----------------------------------------------------------------------------
  // Internal
  //----------------------------------------------------------------------------

  void AddChunk(ResourceInternal, TypeKey* struct_type, int struct_size,
                ChunkReader* chunk_reader);

 private:
  using ChunkKey = std::tuple<ChunkType, TypeKey*>;
  using ChunkValues = std::tuple<int, std::vector<void*>>;
  absl::flat_hash_map<ChunkKey, ChunkValues> chunks_;
  std::vector<void*> chunk_memory_;
};

//==============================================================================
// FileResourceLoaader
//==============================================================================

// This class supports loading resources from chunk files.
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
// Chunks may be resource chunks or generic chunks. Resource chunks *must*
// have their first member be "ResourceId id", as this is used by the file
// reader to coordinate with the associated ResourceSystem.
//
// To use this class, call Create and register one or more resource or generic
// chunk types. Call Read to load the resource from the resource file.
//
// A context is passed to all readers when loading a chunk file. This may be
// used as desired to store data that is relevant across chunks. The
// ResourceFileReader itself will always store two objects in the context:
//  - FileResources: This contains all resources loaded so far, or referenced
//    from the resource list chunk at the beginning of the file. Resources
//    that depend on other resources should *always* use this map to look up
//    dependent resources instead of querying the ResourceSystem directly. If
//    a dependency is not in this map, the dependent resource should fail to
//    load.
//  - ResourceFileChunks: This contains a map of chunks loaded so far (both
//    resource chunks and generic chunks). Chunks are queried according to
//    their ChunkType and ChunkStruct.
// In addition, the caller may add a ResourceSet to the context, in which case
// dependent resources are loaded into the set. If no ResourceSet is provided,
// then all dependent resources must already be loaded.
class ResourceFileReader final {
 public:
  // A resource chunk reader must return a resource of the specified type on
  // success.
  //
  // When this is called, the chunk is valid and is of the version specified at
  // registration.
  //
  // If the chunk reader takes ownership of the chunk (calls ReleaseChunkData),
  // then this chunk will not added to ResourceFileChunks; otherwise, it will.
  //
  // The provided ResourceEntry should be used to initialize the resource, and
  // it will be the same resource ID as is in the chunk. If a resource chunk was
  // encountered, but resource already existed, the pre-existing resource will
  // be returned instead of calling the resource chunk reader.
  //
  // See class description above for details on the passed in context.
  template <typename ResourceType>
  using ResourceChunkReader = Callback<ResourceType*(
      Context* context, ChunkReader* chunk_reader, ResourceEntry entry)>;

  // A resource flat buffer chunk reader must return a resource of the specified
  // type on success.
  //
  // When this is called, the chunk is valid and is of the
  // version specified at registration. The chunk as already been converted into
  // the specified FlatBufferType via flatbuffers::GetRoot<FlatBufferType>.
  //
  // Flat buffer resource chunks are never added to ResourceFileChunks, and the
  // passed in FlatBufferType instance will become invalid immediately after the
  // reader returns.
  //
  // The provided ResourceEntry should be used to initialize the resource, and
  // it will be the same resource ID as is in the chunk. If a resource chunk was
  // encountered, but resource already existed, the pre-existing resource will
  // be returned instead of calling the resource chunk reader.
  //
  // See class description above for details on the passed in context.
  template <typename ResourceType, typename FlatBufferType>
  using ResourceFlatBufferChunkReader = Callback<ResourceType*(
      Context* context, const FlatBufferType* chunk, ResourceEntry entry)>;

  // A generic chunk reader is called on generic chunks to do any processing and
  // patch-up on the chunk before it is generally available.
  //
  // When this is called, the chunk is valid and is of the version specified at
  // registration.
  //
  // If the chunk reader takes ownership of the chunk (calls ReleaseChunkData),
  // then this chunk will not added to ResourceFileChunks; otherwise, it will.
  //
  // See class description above for details on the passed in context.
  using GenericChunkReader =
      Callback<bool(Context* context, ChunkReader* chunk_reader)>;

  // A generic flat buffer chunk reader is called on generic chunks to do
  // processing on the chunk (for instance, adding data to the context for later
  // chunk readers).
  //
  // When this is called, the chunk is valid and is of the version specified at
  // registration. The chunk as already been converted into the specified
  // FlatBufferType via flatbuffers::GetRoot<FlatBufferType>.
  //
  // Flat buffer chunks are never added to ResourceFileChunks, and the passed in
  // FlatBufferType instance will become invalid immediately after the reader
  // returns.
  //
  // See class description above for details on the passed in context.
  template <typename FlatBufferType>
  using GenericFlatBufferChunkReader =
      Callback<bool(Context* context, const FlatBufferType* chunk)>;

  //----------------------------------------------------------------------------
  // Contract constraints
  //----------------------------------------------------------------------------

  // REQUIRED: ResourceSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSystem, kInRequired,
                               ResourceSystem);

  // REQUIRED: FileSystem interface.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileSystem, kInRequired, FileSystem);

  // Contract for creating a new ResourceFileReader
  using CreateContract =
      ContextContract<kConstraintResourceSystem, kConstraintFileSystem>;

  // SCOPED: During a load operation, a FileResources object will be in the
  // context, for use by readers.
  static GB_CONTEXT_CONSTRAINT(kConstraintFileResources, kScoped,
                               FileResources);

  // SCOPED: During a load operation, a ResourceFileChunks object will be in the
  // context, for use by readers.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceFileChunks, kScoped,
                               ResourceFileChunks);

  // OPTIONAL: A ResourceSet can be specified for load operations. If it is
  // specified, dependent resources and the loaded resource will be added to the
  // resource set. This also allows dependent resources to be dynamically loaded
  // if they have known resource names in the resource file.
  static GB_CONTEXT_CONSTRAINT(kConstraintResourceSet, kInOptional,
                               ResourceSet);

  // Contract for the context passed to Read functions.
  using LoadContract =
      ContextContract<kConstraintFileResources, kConstraintResourceFileChunks,
                      kConstraintResourceSet>;

  //----------------------------------------------------------------------------
  // Construction / Destruction
  //----------------------------------------------------------------------------

  // Creates a new ResourceFileReader.
  static std::unique_ptr<ResourceFileReader> Create(CreateContract contract);

  ResourceFileReader(const ResourceFileReader&) = delete;
  ResourceFileReader(ResourceFileReader&&) = delete;
  ResourceFileReader& operator=(const ResourceFileReader&) = delete;
  ResourceFileReader& operator=(ResourceFileReader&&) = delete;
  ~ResourceFileReader();

  //----------------------------------------------------------------------------
  // Chunk handler registration
  //----------------------------------------------------------------------------

  // Registers a resource chunk reader.
  //
  // The ResourceType must be a derived class of Resource, and the ChunkStruct
  // must have its first member define the resource ID for the resource:
  //    ResourceId id;
  // This is validated at compile time.
  //
  // The version indicates which chunk version the reader supports. If there are
  // multiple supported versions for resource, multiple chunk readers must be
  // registered (one per version).
  //
  // See ResourceChunkReader for details on the reader callback itself.
  template <typename ResourceType, typename ChunkStruct>
  bool RegisterResourceChunk(const ChunkType& chunk_type, int version,
                             ResourceChunkReader<ResourceType> reader);

  // Registers a resource chunk reader based on flat buffers.
  //
  // The ResourceType must be a derived class of Resource. This is validated at
  // compile time.
  //
  // The version indicates which chunk version the reader supports. If there are
  // multiple supported versions for resource, multiple chunk readers must be
  // registered (one per version).
  //
  // See ResourceFlatBufferChunkReader for details on the reader callback
  // itself.
  template <typename ResourceType, typename FlatBufferType>
  bool RegisterResourceFlatBufferChunk(
      const ChunkType& chunk_type, int version,
      ResourceFlatBufferChunkReader<ResourceType, FlatBufferType> reader);

  // Registers a generic chunk reader.
  //
  // The version indicates which chunk version the reader supports. If there are
  // multiple supported versions for resource, multiple chunk readers must be
  // registered (one per version).
  //
  // See GenericChunkReader for details on the reader callback itself.
  template <typename ChunkStruct>
  bool RegisterGenericChunk(const ChunkType& chunk_type, int version,
                            GenericChunkReader reader);

  // Registers a generic chunk reader based on flat buffers.
  //
  // The version indicates which chunk version the reader supports. If there are
  // multiple supported versions for resource, multiple chunk readers must be
  // registered (one per version).
  //
  // See GenericFlatBufferChunkReader for details on the reader callback itself.
  template <typename FlatBufferType>
  bool RegisterGenericFlatBufferChunk(
      const ChunkType& chunk_type, int version,
      GenericFlatBufferChunkReader<FlatBufferType> reader);

  //----------------------------------------------------------------------------
  // File loading
  //----------------------------------------------------------------------------

  // Loads a resource chunk file given the specified name.
  //
  // The context may optionally be seeded with whatever is appropriate for the
  // load. This is passed to all chunk readers. In addition, Read will add a
  // FileResources and ResourceFileChunks objects to the context.
  //
  // The returned resource is not registered with the resource system under the
  // name yet, and is also not referenced (and so generally is not visible).
  // This is intended to be used as an implementation by resource loaders
  // registered with a resource manager.
  //
  // Example:
  //     resource_manager_.InitLoader<Example>([](std::string_view name) {
  //       return resource_reader_->Read<Example>(name);
  //     });
  template <typename Type>
  Type* Read(std::string_view name, LoadContract contract = Context());

  // Loads a resource chunk file for any registered resource chunk.
  //
  // This is the same as Read, except it will load any kind of resource
  // registered with the resource reader. This is intended to be used as an
  // implementation by resource loaders registered with a resource manager.
  //
  // Example:
  //     resource_manager_.InitGenericLoader([](TypeKey* key,
  //                                            std::string_view name) {
  //       return resource_reader_->GenericLoad(key, name);
  //     });
  Resource* Read(TypeKey* type, std::string_view name,
                 LoadContract contract = Context());

 private:
  // Flat buffers always start with an 32-bit offset to the root. In the most
  // degenerate case, this would be zero -- indicating there is no data. As all
  // chunks are stored at 8-byte sizes, the minimum chunk size is 8.
  static inline constexpr int kMinSizeFlatBufferGenericChunk = 8;

  // Resource chunks always start with a ResourceId which is another 8 bytes.
  static inline constexpr int kMinSizeFlatBufferResourceChunk =
      kMinSizeFlatBufferGenericChunk + 8;

  using ChunkReaderCallback =
      Callback<bool(Context* context, ChunkReader* chunk_reader,
                    ResourceEntry resource_entry, Resource** out_resource)>;
  struct ChunkReaderInfo {
    TypeKey* resource_type = nullptr;
    TypeKey* struct_type =
        nullptr;  // A null struct_type indicated a flat buffer chunk.
    int struct_size = 0;
    int version = 0;
    ChunkReaderCallback reader;
  };

  ResourceFileReader(ValidatedContext context);

  bool DoRegisterChunkReader(const ChunkType& chunk_type, int version,
                             TypeKey* resource_type, TypeKey* struct_type,
                             int struct_size, ChunkReaderCallback reader);

  ValidatedContext context_;
  absl::flat_hash_map<TypeKey*, std::vector<ChunkType>> resource_chunks_;
  absl::flat_hash_map<ChunkType, std::vector<ChunkReaderInfo>> chunk_readers_;
};

//==============================================================================
// Template / inline implementation
//==============================================================================

template <typename Type>
inline Type* FileResources::GetResource(ResourceId id) const {
  auto it = resources_.find({TypeKey::Get<Type>(), id});
  return it != resources_.end() ? static_cast<Type*>(it->second) : nullptr;
}

inline void FileResources::AddResource(ResourceInternal, TypeKey* type,
                                       ResourceId id, Resource* resource) {
  resources_[{type, id}] = resource;
}

template <typename ChunkStruct>
inline absl::Span<ChunkStruct* const> ResourceFileChunks::GetChunks(
    const ChunkType& type, int* version) const {
  auto it = chunks_.find({type, TypeKey::Get<ChunkStruct>()});
  if (it == chunks_.end()) {
    return {};
  }
  const auto& chunks = std::get<std::vector<void*>>(it->second);
  if (version != nullptr) {
    *version = std::get<int>(it->second);
  }
  return absl::MakeSpan(reinterpret_cast<ChunkStruct* const*>(chunks.data()),
                        chunks.size());
}

template <typename ChunkStruct>
inline ChunkStruct* ResourceFileChunks::GetChunk(int index,
                                                 const ChunkType& type,
                                                 int* version) const {
  auto chunks = GetChunks<ChunkStruct>(type, version);
  return index >= 0 && index < static_cast<int>(chunks.size()) ? chunks[index]
                                                               : nullptr;
}

template <typename ChunkStruct>
inline ChunkStruct* ResourceFileChunks::GetChunk(const ChunkType& type,
                                                 int* version) const {
  return GetChunk<ChunkStruct>(0, type, version);
}

template <typename ResourceType, typename ChunkStruct>
bool ResourceFileReader::RegisterResourceChunk(
    const ChunkType& chunk_type, int version,
    ResourceChunkReader<ResourceType> reader) {
  static_assert(std::is_trivially_copyable_v<ChunkStruct>,
                "Chunk structure must be trivially copyable");
  static_assert(offsetof(ChunkStruct, id) == 0 &&
                    std::is_same_v<ResourceId, decltype(ChunkStruct::id)>,
                "Chunk structure must begin with \"ResourceId id;\"");
  static_assert(std::is_base_of_v<Resource, ResourceType>,
                "ResourceType must be a Resource");
  return DoRegisterChunkReader(
      chunk_type, version, TypeKey::Get<ResourceType>(),
      TypeKey::Get<ChunkStruct>(), static_cast<int>(sizeof(ChunkStruct)),
      [reader = std::move(reader)](Context* context, ChunkReader* chunk_reader,
                                   ResourceEntry resource_entry,
                                   Resource** out_resource) {
        *out_resource =
            reader(context, chunk_reader, std::move(resource_entry));
        return *out_resource != nullptr;
      });
}

template <typename ResourceType, typename FlatBufferType>
bool ResourceFileReader::RegisterResourceFlatBufferChunk(
    const ChunkType& chunk_type, int version,
    ResourceFlatBufferChunkReader<ResourceType, FlatBufferType> reader) {
  static_assert(std::is_base_of_v<Resource, ResourceType>,
                "ResourceType must be a Resource");
  return DoRegisterChunkReader(
      chunk_type, version, TypeKey::Get<ResourceType>(), nullptr,
      kMinSizeFlatBufferResourceChunk,
      [reader = std::move(reader)](Context* context, ChunkReader* chunk_reader,
                                   ResourceEntry resource_entry,
                                   Resource** out_resource) {
        auto* id = chunk_reader->GetChunkData<ResourceId>();
        if (id == nullptr) {
          return false;
        }
        *out_resource =
            reader(context, flatbuffers::GetRoot<FlatBufferType>(id + 1),
                   std::move(resource_entry));
        return *out_resource != nullptr;
      });
}

template <typename ChunkStruct>
bool ResourceFileReader::RegisterGenericChunk(const ChunkType& chunk_type,
                                              int version,
                                              GenericChunkReader reader) {
  static_assert(std::is_trivially_copyable_v<ChunkStruct>,
                "Chunk structure must be trivially copyable");
  return DoRegisterChunkReader(
      chunk_type, version, nullptr, TypeKey::Get<ChunkStruct>(),
      static_cast<int>(sizeof(ChunkStruct)),
      [reader = std::move(reader)](Context* context, ChunkReader* chunk_reader,
                                   ResourceEntry resource_entry,
                                   Resource** out_resource) {
        *out_resource = nullptr;
        return reader(context, chunk_reader);
      });
}

template <typename FlatBufferType>
bool ResourceFileReader::RegisterGenericFlatBufferChunk(
    const ChunkType& chunk_type, int version,
    GenericFlatBufferChunkReader<FlatBufferType> reader) {
  return DoRegisterChunkReader(
      chunk_type, version, nullptr, nullptr, kMinSizeFlatBufferGenericChunk,
      [reader = std::move(reader)](Context* context, ChunkReader* chunk_reader,
                                   ResourceEntry resource_entry,
                                   Resource** out_resource) {
        auto* chunk_data = chunk_reader->GetChunkData<void>();
        if (chunk_data == nullptr) {
          return false;
        }
        *out_resource = nullptr;
        return reader(context,
                      flatbuffers::GetRoot<FlatBufferType>(chunk_data));
      });
}

template <typename Type>
inline Type* ResourceFileReader::Read(std::string_view name,
                                      LoadContract contract) {
  return static_cast<Type*>(
      Read(TypeKey::Get<Type>(), name, std::move(contract)));
}

}  // namespace gb

#endif  // GB_RESOURCE_RESOURCE_FILE_READER_H_
