// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/resource/file/resource_file_writer.h"

#include "absl/memory/memory.h"
#include "gb/file/file.h"
#include "gb/file/file_system.h"
#include "gb/resource/file/resource_chunks.h"
#include "gb/resource/resource_system.h"

namespace gb {

std::unique_ptr<ResourceFileWriter> ResourceFileWriter::Create(
    CreateContract contract) {
  ValidatedContext context = std::move(contract);
  if (!context.IsValid()) {
    return nullptr;
  }
  return absl::WrapUnique(new ResourceFileWriter(std::move(context)));
}

ResourceFileWriter::ResourceFileWriter(ValidatedContext context)
    : context_(std::move(context)) {}

ResourceFileWriter::~ResourceFileWriter() {}

bool ResourceFileWriter::DoRegisterResourceWriter(const ChunkType& chunk_type,
                                                  TypeKey* type,
                                                  GenericWriter writer) {
  if (!writers_.try_emplace(type, WriterInfo{chunk_type, std::move(writer)})
           .second) {
    LOG(ERROR) << "Writer already registered";
    return false;
  }
  return true;
}

bool ResourceFileWriter::Write(std::string_view name, Resource* resource,
                               WriteContract contract) {
  ValidatedContext context(std::move(contract));

  auto writer_it = writers_.find(resource->GetResourceType());
  if (writer_it == writers_.end()) {
    LOG(ERROR) << "Resource is not a type registered with file writer when "
                  "writing file: "
               << name;
    return false;
  }
  const auto& writer_info = writer_it->second;

  auto* file_system = context_.GetPtr<FileSystem>();
  auto file = file_system->OpenFile(name, kNewFileFlags);
  if (file == nullptr) {
    LOG(ERROR) << "Failed to open resource file for writing: " << name;
    return false;
  }

  std::vector<ChunkWriter> chunk_writers;

  // Write out resource load chunk if there are any dependencies.
  ResourceDependencyList dependencies;
  resource->GetResourceDependencies(&dependencies);
  if (!dependencies.empty()) {
    ChunkWriter chunk_writer = ChunkWriter::New<ResourceLoadChunk>(
        kChunkTypeResourceLoad, 1, static_cast<int32_t>(dependencies.size()));
    auto* chunks = chunk_writer.GetChunkData<ResourceLoadChunk>();
    for (int i = 0; i < chunk_writer.GetCount(); ++i) {
      auto& chunk = chunks[i];
      Resource* resource = dependencies[i];
      chunk.id = resource->GetResourceId();
      std::string_view type_name = resource->GetResourceType()->GetTypeName();
      if (type_name.empty()) {
        LOG(ERROR) << "Resource dependency has no type name, so cannot be "
                      "written to file: "
                   << name;
        return false;
      }
      chunk.type = chunk_writer.AddString(type_name);
      chunk.name = chunk_writer.AddString(resource->GetResourceName());
    }
    chunk_writers.emplace_back(std::move(chunk_writer));
  }

  if (!writer_info.writer(context.GetContext(), resource, &chunk_writers)) {
    return false;
  }
  return WriteChunkFile(file.get(), writer_info.chunk_type, chunk_writers);
}

}  // namespace gb