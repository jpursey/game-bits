// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/resource_file_reader.h"

#include "absl/memory/memory.h"
#include "gb/base/scoped_call.h"
#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "gb/resource/resource_chunks.h"
#include "gb/resource/resource_system.h"
#include "glog/logging.h"

namespace gb {

//==============================================================================
// FileResourceChunks
//==============================================================================

ResourceFileChunks::~ResourceFileChunks() {
  for (auto ptr : chunk_memory_) {
    std::free(ptr);
  }
}

void ResourceFileChunks::AddChunk(ResourceInternal, TypeKey* struct_type,
                                  int struct_size, ChunkReader* chunk_reader) {
  uint8_t* chunk_data = chunk_reader->GetChunkData<uint8_t>();
  if (chunk_data == nullptr) {
    // If ReleaseChunkData was called, then the chunk_data will be null.
    // This is not an error (some resources may take ownership of chunk data),
    // but we cannot store the chunk data for general reference either.
    return;
  }

  ChunkValues& values = chunks_[{chunk_reader->GetType(), struct_type}];
  int& version = std::get<int>(values);
  if (version == 0) {
    version = chunk_reader->GetVersion();
  } else if (version != chunk_reader->GetVersion()) {
    LOG(ERROR) << "Multiple versions of chunk "
               << chunk_reader->GetType().ToString()
               << " found in resource chunk file. Ignoring version "
               << chunk_reader->GetVersion();
    return;
  }
  auto& chunks = std::get<std::vector<void*>>(values);
  for (int i = 0; i < chunk_reader->GetCount(); ++i) {
    chunks.emplace_back(chunk_data);
    chunk_data += struct_size;
  }
  chunk_memory_.emplace_back(chunk_reader->ReleaseChunkData<void>());
}

//==============================================================================
// FileResourceLoaader
//==============================================================================

std::unique_ptr<ResourceFileReader> ResourceFileReader::Create(
    CreateContract contract) {
  ValidatedContext context = std::move(contract);
  if (!context.IsValid()) {
    return nullptr;
  }
  return absl::WrapUnique(new ResourceFileReader(std::move(context)));
}

ResourceFileReader::ResourceFileReader(ValidatedContext context)
    : context_(std::move(context)) {}

ResourceFileReader::~ResourceFileReader() {}

bool ResourceFileReader::DoRegisterChunkReader(
    const ChunkType& chunk_type, int version, TypeKey* resource_type,
    TypeKey* struct_type, int struct_size, ChunkReaderCallback reader) {
  auto& readers = chunk_readers_[chunk_type];
  for (auto& reader_info : readers) {
    if (reader_info.version == version) {
      LOG(ERROR) << "Reader already defined for chunk " << chunk_type.ToString()
                 << " version " << version;
      return false;
    }
  }
  readers.emplace_back() = {resource_type, struct_type, struct_size, version,
                            std::move(reader)};

  if (resource_type != nullptr) {
    bool found = false;
    auto& chunk_types = resource_chunks_[resource_type];
    for (auto& known_chunk_type : chunk_types) {
      if (known_chunk_type == chunk_type) {
        found = true;
        break;
      }
    }
    if (!found) {
      chunk_types.emplace_back(chunk_type);
    }
  }
  return true;
}

Resource* ResourceFileReader::Read(TypeKey* type, std::string_view name,
                                   LoadContract contract) {
  ValidatedContext load_context(std::move(contract));

  auto chunk_types_it = resource_chunks_.find(type);
  if (chunk_types_it == resource_chunks_.end()) {
    LOG(ERROR)
        << "Unknown resource type for resource reader when loading file: "
        << name;
    return nullptr;
  }
  auto& chunk_types = chunk_types_it->second;

  auto* file_system = context_.GetPtr<FileSystem>();
  auto file = file_system->OpenFile(name, kReadFileFlags);
  if (file == nullptr) {
    LOG(ERROR) << "Could not open resource file: " << name;
    return nullptr;
  }

  ChunkType file_type;
  if (!ReadChunkFile(file.get(), &file_type, nullptr)) {
    LOG(ERROR) << "Resource file is invalid: " << name;
    return nullptr;
  }

  bool chunk_found = false;
  for (const auto& chunk_type : chunk_types) {
    if (file_type == chunk_type) {
      chunk_found = true;
      break;
    }
  }
  if (!chunk_found) {
    LOG(ERROR) << "Resource file is of unknown chunk type \""
               << file_type.ToString() << "\": " << name;
    return nullptr;
  }

  FileResources file_resources;
  ResourceFileChunks file_chunks;
  load_context.SetPtr<FileResources>(&file_resources);
  load_context.SetPtr<ResourceFileChunks>(&file_chunks);

  std::vector<Resource*> delete_resources;
  ScopedCall resource_deleter([&delete_resources] {
    for (auto* resource : delete_resources) {
      resource->MaybeDelete({});
    }
  });

  auto* resource_set = load_context.GetPtr<ResourceSet>();
  auto* resource_system = context_.GetPtr<ResourceSystem>();
  bool has_error = false;
  while (true) {
    auto chunk_reader = ChunkReader::Read(file.get(), &has_error);
    if (has_error) {
      return nullptr;
    }
    if (!chunk_reader.has_value()) {
      break;
    }

    if (chunk_reader->GetType() == kChunkTypeResourceLoad) {
      if (chunk_reader->GetVersion() != 1) {
        LOG(ERROR) << "Unknown version " << chunk_reader->GetVersion()
                   << " for chunk \"" << kChunkTypeResourceLoad.ToString()
                   << "\" in file: " << name;
        return nullptr;
      }
      ResourceSystem* resource_system = context_.GetPtr<ResourceSystem>();
      auto* chunks = chunk_reader->GetChunkData<ResourceLoadChunk>();
      for (int i = 0; i < chunk_reader->GetCount(); ++i) {
        auto& chunk = chunks[i];
        chunk_reader->ConvertToPtr(&chunk.type);
        chunk_reader->ConvertToPtr(&chunk.name);
        if (chunk.id == 0 || chunk.type.ptr == nullptr) {
          LOG(ERROR) << "Invalid resource load chunk " << i
                     << " in file: " << name;
          return nullptr;
        }
        TypeKey* type = resource_system->GetResourceType(chunk.type.ptr);
        if (type == nullptr) {
          LOG(ERROR) << "Unknown resource type " << chunk.type.ptr
                     << " in for resource system in file: " << name;
          return nullptr;
        }
        Resource* resource = resource_system->Find({}, type, chunk.id);
        if (resource == nullptr) {
          if (resource_set != nullptr && chunk.name.ptr != nullptr) {
            resource =
                resource_system->Load(resource_set, type, chunk.name.ptr);
          }
          if (resource == nullptr) {
            LOG(ERROR) << "Could not load or find resource " << chunk.type.ptr
                       << " (ID: " << chunk.id << ") of name \""
                       << chunk.name.ptr << "\"";
            return nullptr;
          }
        }
        file_resources.AddResource({}, type, chunk.id, resource);
      }
      continue;
    }

    auto chunk_reader_it = chunk_readers_.find(chunk_reader->GetType());
    if (chunk_reader_it == chunk_readers_.end()) {
      continue;
    }
    ChunkReaderInfo* reader_info = nullptr;
    for (auto& reader_info_entry : chunk_reader_it->second) {
      if (reader_info_entry.version == chunk_reader->GetVersion()) {
        reader_info = &reader_info_entry;
        break;
      }
    }
    if (reader_info == nullptr) {
      LOG(ERROR) << "Unknown version " << chunk_reader->GetVersion()
                 << " for chunk \"" << chunk_reader->GetType().ToString()
                 << "\" in file: " << name;
      return nullptr;
    }

    Resource* chunk_resource = nullptr;
    ResourceEntry chunk_resource_entry;
    ResourceId chunk_resource_id = 0;
    bool delete_chunk_resource = false;
    if (reader_info->resource_type != nullptr) {
      ResourceId* chunk = chunk_reader->GetChunkData<ResourceId>();
      if (chunk == nullptr || *chunk == 0) {
        LOG(ERROR) << "Invalid resource chunk \""
                   << chunk_reader->GetType().ToString()
                   << "\" in file: " << name;
        return nullptr;
      }
      chunk_resource_id = *chunk;
      chunk_resource = resource_system->Find({}, reader_info->resource_type,
                                             chunk_resource_id);
      if (chunk_resource == nullptr) {
        chunk_resource_entry = resource_system->NewResourceEntry(
            {}, reader_info->resource_type, chunk_resource_id);
        delete_chunk_resource = true;
      }
    }

    if (chunk_resource == nullptr) {
      if (!reader_info->reader(load_context.GetContext(), &chunk_reader.value(),
                               std::move(chunk_resource_entry),
                               &chunk_resource)) {
        return nullptr;
      }
    }

    if (chunk_resource != nullptr) {
      if (delete_chunk_resource) {
        delete_resources.emplace_back(chunk_resource);
      }
      if (chunk_reader->GetType() == file_type) {
        if (!chunk_resource->GetResourceName().empty()) {
          LOG(ERROR) << "Resource already loaded as "
                     << chunk_resource->GetResourceName();
          return nullptr;
        }
        delete_resources.clear();
        if (resource_set != nullptr) {
          resource_set->Add(chunk_resource);
        }
        return chunk_resource;
      }

      file_resources.AddResource({}, reader_info->resource_type,
                                 chunk_resource_id, chunk_resource);
    }
    if (reader_info->struct_type != nullptr) {
      file_chunks.AddChunk({}, reader_info->struct_type,
                           reader_info->struct_size, &chunk_reader.value());
    }
  }

  LOG(ERROR) << "Resource chunk \"" << file_type.ToString()
             << "\" not found in file: " << name;
  return nullptr;
}

}  // namespace gb
