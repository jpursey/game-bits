// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include <map>

#include "gb/base/context_builder.h"
#include "gb/file/file_system.h"
#include "gb/file/memory_file_protocol.h"
#include "gb/resource/file/resource_chunks.h"
#include "gb/resource/file/resource_file_reader.h"
#include "gb/resource/file/resource_file_test_generated.h"
#include "gb/resource/file/resource_file_writer.h"
#include "gb/resource/resource_manager.h"
#include "gb/resource/resource_system.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;

using ::flatbuffers::FlatBufferBuilder;
namespace fb = ::flatbuffers;

constexpr ChunkType kChunkTypeKeyValue = {'G', 'T', 'K', 'V'};
constexpr ChunkType kChunkTypeResourceA = {'G', 'T', 'R', 'A'};
constexpr ChunkType kChunkTypeResourceB = {'G', 'T', 'R', 'B'};
constexpr ChunkType kChunkTypeResourceC = {'G', 'T', 'R', 'C'};

using KeyValueMap = std::map<std::string, float>;

struct Point {
  int x;
  int y;
  int z;
};

struct KeyValueChunk {
  ChunkPtr<const char> key;
  float value;
};

struct ResourceAChunk {
  ResourceId id;
  ChunkPtr<const char> name;
};

struct ResourceBChunk {
  ResourceId id;
  int32_t point_count;
  ChunkPtr<Point> points;
};

struct ResourceCChunk {
  ResourceId id;
  ResourceId a_id;
  ResourceId b_id;
};

class ResourceA : public Resource {
 public:
  ResourceA(ResourceEntry entry, std::string_view name)
      : Resource(std::move(entry)), name_(name.data(), name.size()) {}

  const std::string& GetName() const { return name_; }

 private:
  std::string name_;
};

class NoNameResourceA : public ResourceA {
 public:
  NoNameResourceA(ResourceEntry entry)
      : ResourceA(std::move(entry), "NoName") {}
};

class ResourceB : public Resource {
 public:
  ResourceB(ResourceEntry entry, absl::Span<const Point> points,
            KeyValueMap values)
      : Resource(std::move(entry)),
        points_(points.begin(), points.end()),
        values_(std::move(values)) {}

  absl::Span<const Point> GetPoints() { return points_; }
  const KeyValueMap& GetValues() { return values_; }

 private:
  std::vector<Point> points_;
  KeyValueMap values_;
};

class ResourceC : public Resource {
 public:
  ResourceC(ResourceEntry entry, ResourceA* a, ResourceB* b)
      : Resource(std::move(entry)), a_(a), b_(b) {}

  ResourceA* GetA() const { return a_; }
  ResourceB* GetB() const { return b_; }

  void GetResourceDependencies(
      ResourceDependencyList* dependencies) const override {
    if (a_ != nullptr) {
      dependencies->push_back(a_);
    }
    if (b_ != nullptr) {
      dependencies->push_back(b_);
    }
  }

 private:
  ResourceA* a_ = nullptr;
  ResourceB* b_ = nullptr;
};

class ResourceFileTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    TypeKey::Get<ResourceA>()->SetTypeName("ResourceA");
    TypeKey::Get<ResourceB>()->SetTypeName("ResourceB");
    TypeKey::Get<ResourceC>()->SetTypeName("ResourceC");
  }

  void SetUp() override {
    file_system_ = std::make_unique<FileSystem>();
    ASSERT_NE(file_system_, nullptr);
    ASSERT_TRUE(file_system_->Register(std::make_unique<MemoryFileProtocol>()));

    resource_system_ = ResourceSystem::Create();
    ASSERT_NE(resource_system_, nullptr);

    reader_ = ResourceFileReader::Create(
        ContextBuilder()
            .SetPtr<FileSystem>(file_system_.get())
            .SetPtr<ResourceSystem>(resource_system_.get())
            .Build());
    ASSERT_NE(reader_, nullptr);

    writer_ = ResourceFileWriter::Create(
        ContextBuilder()
            .SetPtr<FileSystem>(file_system_.get())
            .SetPtr<ResourceSystem>(resource_system_.get())
            .Build());
    ASSERT_NE(writer_, nullptr);

    resource_manager_.InitLoader<ResourceC>(
        [this](Context* context, std::string_view name) {
          return reader_->Read<ResourceC>(name, context);
        });
    resource_manager_.InitGenericLoader(
        [this](Context* context, TypeKey* type, std::string_view name) {
          return reader_->Read(type, name, context);
        });
    resource_system_
        ->Register<ResourceA, ResourceB, ResourceC, NoNameResourceA>(
            &resource_manager_);
  }

  void TearDown() override { std::free(key_value_chunk_); }

  ResourceFileReader::GenericChunkReader GetKeyValueLoader() {
    return [this](Context* context, ChunkReader* chunk_reader) {
      auto* chunk = chunk_reader->GetChunkData<KeyValueChunk>();
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return false;
      }
      key_values_.clear();
      for (int i = 0; i < chunk_reader->GetCount(); ++i) {
        chunk_reader->ConvertToPtr(&chunk[i].key);
        key_values_[chunk[i].key.ptr] = chunk[i].value;
      }
      if (acquire_key_values_) {
        key_value_chunk_ = chunk_reader->ReleaseChunkData<KeyValueChunk>();
      }
      return true;
    };
  }

  ResourceFileReader::GenericFlatBufferChunkReader<fbs::KeyValueChunk>
  GetKeyValueFlatBufferLoader() {
    return [](Context* context, const fbs::KeyValueChunk* chunk) {
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return false;
      }

      KeyValueMap values;
      if (chunk->values() != nullptr) {
        for (auto value : *chunk->values()) {
          EXPECT_NE(value, nullptr);
          if (value != nullptr) {
            std::string key;
            if (value->key() != nullptr) {
              key = value->key()->c_str();
            }
            values[key] = value->value();
          }
        }
      }
      context->SetValue<KeyValueMap>(std::move(values));

      return true;
    };
  }

  ResourceFileReader::ResourceChunkReader<ResourceA> GetResourceALoader() {
    return [](Context* context, ChunkReader* chunk_reader,
              ResourceEntry entry) -> ResourceA* {
      auto* chunk = chunk_reader->GetChunkData<ResourceAChunk>();
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }
      EXPECT_EQ(entry.GetType(), TypeKey::Get<ResourceA>());
      EXPECT_EQ(entry.GetId(), chunk->id);
      chunk_reader->ConvertToPtr(&chunk->name);
      return new ResourceA(std::move(entry), chunk->name.ptr);
    };
  }
  ResourceFileWriter::ResourceWriter<ResourceA> GetResourceAWriter() {
    return [](Context* context, ResourceA* resource,
              std::vector<ChunkWriter>* out_chunks) {
      auto chunk_writer =
          ChunkWriter::New<ResourceAChunk>(kChunkTypeResourceA, 1);
      auto* chunk = chunk_writer.GetChunkData<ResourceAChunk>();
      chunk->id = resource->GetResourceId();
      chunk->name = chunk_writer.AddString(resource->GetName());
      out_chunks->emplace_back(std::move(chunk_writer));
      return true;
    };
  }

  ResourceFileReader::ResourceChunkReader<ResourceB> GetResourceBLoader() {
    return [](Context* context, ChunkReader* chunk_reader,
              ResourceEntry entry) -> ResourceB* {
      auto* chunk = chunk_reader->GetChunkData<ResourceBChunk>();
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }
      EXPECT_EQ(entry.GetType(), TypeKey::Get<ResourceB>());
      EXPECT_EQ(entry.GetId(), chunk->id);
      chunk_reader->ConvertToPtr(&chunk->points);

      auto* chunks = context->GetPtr<ResourceFileChunks>();
      KeyValueMap values;
      int i = 0;
      for (auto* key_value :
           chunks->GetChunks<KeyValueChunk>(kChunkTypeKeyValue)) {
        values[key_value->key.ptr] = key_value->value;
        if (i == 0) {
          EXPECT_EQ(chunks->GetChunk<KeyValueChunk>(kChunkTypeKeyValue),
                    key_value);
        } else {
          EXPECT_EQ(chunks->GetChunk<KeyValueChunk>(i, kChunkTypeKeyValue),
                    key_value);
        }
        ++i;
      }
      return new ResourceB(
          std::move(entry),
          absl::MakeSpan(chunk->points.ptr, chunk->point_count), values);
    };
  }
  ResourceFileWriter::ResourceWriter<ResourceB> GetResourceBWriter() {
    return [](Context* context, ResourceB* resource,
              std::vector<ChunkWriter>* out_chunks) {
      const auto& values = resource->GetValues();
      if (!values.empty()) {
        auto value_writer = ChunkWriter::New<KeyValueChunk>(
            kChunkTypeKeyValue, 1, static_cast<int32_t>(values.size()));
        auto* value_chunks = value_writer.GetChunkData<KeyValueChunk>();
        int i = 0;
        for (const auto& value : values) {
          value_chunks[i].key = value_writer.AddString(value.first);
          value_chunks[i].value = value.second;
          ++i;
        }
        out_chunks->emplace_back(std::move(value_writer));
      }
      auto chunk_writer =
          ChunkWriter::New<ResourceBChunk>(kChunkTypeResourceB, 1);
      auto* chunk = chunk_writer.GetChunkData<ResourceBChunk>();
      chunk->id = resource->GetResourceId();
      chunk->point_count = static_cast<int32_t>(resource->GetPoints().size());
      chunk->points = chunk_writer.AddData(resource->GetPoints());
      out_chunks->emplace_back(std::move(chunk_writer));
      return true;
    };
  }

  ResourceFileReader::ResourceChunkReader<ResourceC> GetResourceCLoader() {
    return [](Context* context, ChunkReader* chunk_reader,
              ResourceEntry entry) -> ResourceC* {
      auto* chunk = chunk_reader->GetChunkData<ResourceCChunk>();
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }
      EXPECT_EQ(entry.GetType(), TypeKey::Get<ResourceC>());
      EXPECT_EQ(entry.GetId(), chunk->id);
      auto resources = context->GetPtr<FileResources>();
      EXPECT_NE(resources, nullptr);
      if (resources == nullptr) {
        return nullptr;
      }
      return new ResourceC(std::move(entry),
                           resources->GetResource<ResourceA>(chunk->a_id),
                           resources->GetResource<ResourceB>(chunk->b_id));
    };
  }
  ResourceFileWriter::ResourceWriter<ResourceC> GetResourceCWriter() {
    return [](Context* context, ResourceC* resource,
              std::vector<ChunkWriter>* out_chunks) {
      auto chunk_writer =
          ChunkWriter::New<ResourceCChunk>(kChunkTypeResourceC, 1);
      auto* chunk = chunk_writer.GetChunkData<ResourceCChunk>();
      chunk->id = resource->GetResourceId();
      if (resource->GetA() != nullptr) {
        chunk->a_id = resource->GetA()->GetResourceId();
      }
      if (resource->GetB() != nullptr) {
        chunk->b_id = resource->GetB()->GetResourceId();
      }
      out_chunks->emplace_back(std::move(chunk_writer));
      return true;
    };
  }

  ResourceFileReader::ResourceChunkReader<ResourceB>
  GetResourceBHybridLoader() {
    return [](Context* context, ChunkReader* chunk_reader,
              ResourceEntry entry) -> ResourceB* {
      auto* chunk = chunk_reader->GetChunkData<ResourceBChunk>();
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }
      EXPECT_EQ(entry.GetType(), TypeKey::Get<ResourceB>());
      EXPECT_EQ(entry.GetId(), chunk->id);
      chunk_reader->ConvertToPtr(&chunk->points);

      auto values = context->GetValue<KeyValueMap>();
      return new ResourceB(
          std::move(entry),
          absl::MakeSpan(chunk->points.ptr, chunk->point_count), values);
    };
  }
  ResourceFileWriter::ResourceWriter<ResourceB> GetResourceBHybridWriter() {
    return [](Context* context, ResourceB* resource,
              std::vector<ChunkWriter>* out_chunks) {
      const auto& values = resource->GetValues();
      if (!values.empty()) {
        FlatBufferBuilder builder;
        std::vector<fb::Offset<fbs::KeyValue>> fb_value_offsets;
        for (const auto& [key, value] : values) {
          const auto fb_key = builder.CreateString(key);
          fb_value_offsets.emplace_back(
              fbs::CreateKeyValue(builder, fb_key, value));
        }
        const auto fb_values = builder.CreateVector(fb_value_offsets);
        const auto fb_values_chunk =
            fbs::CreateKeyValueChunk(builder, fb_values);
        builder.Finish(fb_values_chunk);
        out_chunks->emplace_back(ChunkWriter::New(kChunkTypeKeyValue, 1,
                                                  builder.GetBufferPointer(),
                                                  builder.GetSize()));
        EXPECT_FALSE(context->Exists<FlatBufferBuilder>());
        context->SetNew<FlatBufferBuilder>(std::move(builder));
      }
      auto chunk_writer =
          ChunkWriter::New<ResourceBChunk>(kChunkTypeResourceB, 1);
      auto* chunk = chunk_writer.GetChunkData<ResourceBChunk>();
      chunk->id = resource->GetResourceId();
      chunk->point_count = static_cast<int32_t>(resource->GetPoints().size());
      chunk->points = chunk_writer.AddData(resource->GetPoints());
      out_chunks->emplace_back(std::move(chunk_writer));
      return true;
    };
  }

  ResourceFileReader::ResourceFlatBufferChunkReader<ResourceA,
                                                    fbs::ResourceAChunk>
  GetResourceAFlatBufferLoader() {
    return [](Context* context, const fbs::ResourceAChunk* chunk,
              ResourceEntry entry) -> ResourceA* {
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }
      std::string_view name;
      if (chunk->name() != nullptr) {
        name = chunk->name()->c_str();
      }
      return new ResourceA(std::move(entry), name);
    };
  }
  ResourceFileWriter::ResourceFlatBufferWriter<ResourceA>
  GetResourceAFlatBufferWriter() {
    return
        [](Context* context, ResourceA* resource, FlatBufferBuilder* builder) {
          const auto fb_name = builder->CreateString(resource->GetName());
          const auto fb_resource = fbs::CreateResourceAChunk(*builder, fb_name);
          builder->Finish(fb_resource);
          return true;
        };
  }

  ResourceFileReader::ResourceFlatBufferChunkReader<ResourceB,
                                                    fbs::ResourceBChunk>
  GetResourceBFlatBufferLoader() {
    return [](Context* context, const fbs::ResourceBChunk* chunk,
              ResourceEntry entry) -> ResourceB* {
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }

      absl::Span<const Point> points;
      if (chunk->points() != nullptr) {
        points = absl::MakeSpan(chunk->points()->GetAs<Point>(0),
                                chunk->points()->size());
      }

      KeyValueMap values;
      if (chunk->values() != nullptr) {
        for (auto value : *chunk->values()) {
          EXPECT_NE(value, nullptr);
          if (value != nullptr) {
            std::string key;
            if (value->key() != nullptr) {
              key = value->key()->c_str();
            }
            values[key] = value->value();
          }
        }
      }

      return new ResourceB(std::move(entry), points, values);
    };
  }
  ResourceFileWriter::ResourceFlatBufferWriter<ResourceB>
  GetResourceBFlatBufferWriter() {
    return [](Context* context, ResourceB* resource,
              FlatBufferBuilder* builder) {
      const auto fb_points = builder->CreateVectorOfNativeStructs<fbs::Point>(
          resource->GetPoints().data(), resource->GetPoints().size());

      std::vector<fb::Offset<fbs::KeyValue>> values;
      for (const auto& [key, value] : resource->GetValues()) {
        auto fb_key = builder->CreateString(key);
        values.emplace_back(fbs::CreateKeyValue(*builder, fb_key, value));
      }
      const auto fb_values = builder->CreateVector(values);

      const auto fb_resource =
          fbs::CreateResourceBChunk(*builder, fb_points, fb_values);
      builder->Finish(fb_resource);
      return true;
    };
  }

  ResourceFileReader::ResourceFlatBufferChunkReader<ResourceC,
                                                    fbs::ResourceCChunk>
  GetResourceCFlatBufferLoader() {
    return [](Context* context, const fbs::ResourceCChunk* chunk,
              ResourceEntry entry) -> ResourceC* {
      EXPECT_NE(chunk, nullptr);
      if (chunk == nullptr) {
        return nullptr;
      }

      auto resources = context->GetPtr<FileResources>();
      EXPECT_NE(resources, nullptr);
      if (resources == nullptr) {
        return nullptr;
      }
      return new ResourceC(std::move(entry),
                           resources->GetResource<ResourceA>(chunk->a_id()),
                           resources->GetResource<ResourceB>(chunk->b_id()));
    };
  }
  ResourceFileWriter::ResourceFlatBufferWriter<ResourceC>
  GetResourceCFlatBufferWriter() {
    return
        [](Context* context, ResourceC* resource, FlatBufferBuilder* builder) {
          fbs::ResourceCChunkBuilder fb_resource_builder(*builder);
          if (resource->GetA() != nullptr) {
            fb_resource_builder.add_a_id(resource->GetA()->GetResourceId());
          }
          if (resource->GetB() != nullptr) {
            fb_resource_builder.add_b_id(resource->GetB()->GetResourceId());
          }
          const auto fb_resource = fb_resource_builder.Finish();
          builder->Finish(fb_resource);
          return true;
        };
  }

  std::unique_ptr<FileSystem> file_system_;
  std::unique_ptr<ResourceSystem> resource_system_;
  ResourceManager resource_manager_;
  std::unique_ptr<ResourceFileReader> reader_;
  std::unique_ptr<ResourceFileWriter> writer_;
  KeyValueMap key_values_;
  bool acquire_key_values_ = false;
  KeyValueChunk* key_value_chunk_ = nullptr;
};

TEST_F(ResourceFileTest, CreateWriterInvalidContext) {
  EXPECT_EQ(ResourceFileWriter::Create(Context()), nullptr);
}

TEST_F(ResourceFileTest, RegisterWriterDuplicateResource) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ASSERT_FALSE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
}

TEST_F(ResourceFileTest, RegisterDifferentWriterDuplicateResource) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ASSERT_FALSE((writer_->RegisterResourceFlatBufferWriter<ResourceA>(
      kChunkTypeResourceA, 1, GetResourceAFlatBufferWriter())));
}

TEST_F(ResourceFileTest, WriteUnregisteredResource) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_FALSE(writer_->Write("mem:/file", resource.Get()));
}

TEST_F(ResourceFileTest, WriteToInvalidFile) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_FALSE(writer_->Write("invalid:/file", resource.Get()));
}

TEST_F(ResourceFileTest, WriteCallbackFailure) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA,
      [](Context*, ResourceA*, std::vector<ChunkWriter>*) { return false; })));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_FALSE(writer_->Write("mem:/file", resource.Get()));
}

TEST_F(ResourceFileTest, WriteResource) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));

  auto file = file_system_->OpenFile("mem:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::vector<ChunkReader> chunks;
  ChunkType file_type;
  EXPECT_TRUE(ReadChunkFile(file.get(), &file_type, &chunks));
  EXPECT_EQ(file_type, kChunkTypeResourceA);
  ASSERT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0].GetType(), kChunkTypeResourceA);
  EXPECT_EQ(chunks[0].GetVersion(), 1);
  ASSERT_EQ(chunks[0].GetCount(), 1);
  auto* a_chunk = chunks[0].GetChunkData<ResourceAChunk>();
  chunks[0].ConvertToPtr(&a_chunk->name);
  EXPECT_EQ(a_chunk->id, resource->GetResourceId());
  EXPECT_STREQ(a_chunk->name.ptr, "Name");
}

TEST_F(ResourceFileTest, WriteFlatBufferResource) {
  ASSERT_TRUE((writer_->RegisterResourceFlatBufferWriter<ResourceA>(
      kChunkTypeResourceA, 1, GetResourceAFlatBufferWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));

  auto file = file_system_->OpenFile("mem:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::vector<ChunkReader> chunks;
  ChunkType file_type;
  EXPECT_TRUE(ReadChunkFile(file.get(), &file_type, &chunks));
  EXPECT_EQ(file_type, kChunkTypeResourceA);
  ASSERT_EQ(chunks.size(), 1);
  EXPECT_EQ(chunks[0].GetType(), kChunkTypeResourceA);
  EXPECT_EQ(chunks[0].GetVersion(), 1);
  ASSERT_EQ(chunks[0].GetCount(), 1);
  ResourceId* a_chunk_id = chunks[0].GetChunkData<ResourceId>();
  ASSERT_NE(a_chunk_id, nullptr);
  EXPECT_EQ(*a_chunk_id, resource->GetResourceId());
  auto* a_chunk = fb::GetRoot<fbs::ResourceAChunk>(a_chunk_id + 1);
  ASSERT_NE(a_chunk, nullptr);
  ASSERT_NE(a_chunk->name(), nullptr);
  EXPECT_STREQ(a_chunk->name()->c_str(), "Name");
}

TEST_F(ResourceFileTest, WriteGenericChunk) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBWriter())));
  ResourcePtr<ResourceB> resource = new ResourceB(
      resource_manager_.NewResourceEntry<ResourceB>(),
      {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {{"alpha", 42.0f}, {"beta", 24.0f}});
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));

  auto file = file_system_->OpenFile("mem:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::vector<ChunkReader> chunks;
  ChunkType file_type;
  EXPECT_TRUE(ReadChunkFile(file.get(), &file_type, &chunks));
  EXPECT_EQ(file_type, kChunkTypeResourceB);
  ASSERT_EQ(chunks.size(), 2);

  ASSERT_EQ(chunks[0].GetType(), kChunkTypeKeyValue);
  EXPECT_EQ(chunks[0].GetVersion(), 1);
  ASSERT_EQ(chunks[0].GetCount(), 2);
  auto* key_value_chunks = chunks[0].GetChunkData<KeyValueChunk>();
  chunks[0].ConvertToPtr(&key_value_chunks[0].key);
  chunks[0].ConvertToPtr(&key_value_chunks[1].key);
  EXPECT_STREQ(key_value_chunks[0].key.ptr, "alpha");
  EXPECT_EQ(key_value_chunks[0].value, 42.0f);
  EXPECT_STREQ(key_value_chunks[1].key.ptr, "beta");
  EXPECT_EQ(key_value_chunks[1].value, 24.0f);

  ASSERT_EQ(chunks[1].GetType(), kChunkTypeResourceB);
  EXPECT_EQ(chunks[1].GetVersion(), 1);
  ASSERT_EQ(chunks[1].GetCount(), 1);
  auto* b_chunk = chunks[1].GetChunkData<ResourceBChunk>();
  chunks[1].ConvertToPtr(&b_chunk->points);
  EXPECT_EQ(b_chunk->id, resource->GetResourceId());
  ASSERT_EQ(b_chunk->point_count, 3);
  ASSERT_EQ(b_chunk->points.ptr[0].x, 1);
  ASSERT_EQ(b_chunk->points.ptr[0].y, 2);
  ASSERT_EQ(b_chunk->points.ptr[0].z, 3);
  ASSERT_EQ(b_chunk->points.ptr[1].x, 4);
  ASSERT_EQ(b_chunk->points.ptr[1].y, 5);
  ASSERT_EQ(b_chunk->points.ptr[1].z, 6);
  ASSERT_EQ(b_chunk->points.ptr[2].x, 7);
  ASSERT_EQ(b_chunk->points.ptr[2].y, 8);
  ASSERT_EQ(b_chunk->points.ptr[2].z, 9);
}

TEST_F(ResourceFileTest, WriteGenericFlatBufferChunk) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBHybridWriter())));
  ResourcePtr<ResourceB> resource = new ResourceB(
      resource_manager_.NewResourceEntry<ResourceB>(),
      {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {{"alpha", 42.0f}, {"beta", 24.0f}});
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));

  auto file = file_system_->OpenFile("mem:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::vector<ChunkReader> chunks;
  ChunkType file_type;
  EXPECT_TRUE(ReadChunkFile(file.get(), &file_type, &chunks));
  EXPECT_EQ(file_type, kChunkTypeResourceB);
  ASSERT_EQ(chunks.size(), 2);

  ASSERT_EQ(chunks[0].GetType(), kChunkTypeKeyValue);
  EXPECT_EQ(chunks[0].GetVersion(), 1);
  ASSERT_EQ(chunks[0].GetCount(), 1);
  auto* key_value_chunk =
      fb::GetRoot<fbs::KeyValueChunk>(chunks[0].GetChunkData<void>());
  ASSERT_NE(key_value_chunk, nullptr);
  ASSERT_NE(key_value_chunk->values(), nullptr);
  ASSERT_EQ(key_value_chunk->values()->size(), 2);
  ASSERT_NE(key_value_chunk->values()->Get(0), nullptr);
  ASSERT_NE(key_value_chunk->values()->Get(0)->key(), nullptr);
  EXPECT_STREQ(key_value_chunk->values()->Get(0)->key()->c_str(), "alpha");
  EXPECT_EQ(key_value_chunk->values()->Get(0)->value(), 42.0f);
  ASSERT_NE(key_value_chunk->values()->Get(1), nullptr);
  ASSERT_NE(key_value_chunk->values()->Get(1)->key(), nullptr);
  EXPECT_STREQ(key_value_chunk->values()->Get(1)->key()->c_str(), "beta");
  EXPECT_EQ(key_value_chunk->values()->Get(1)->value(), 24.0f);

  ASSERT_EQ(chunks[1].GetType(), kChunkTypeResourceB);
  EXPECT_EQ(chunks[1].GetVersion(), 1);
  ASSERT_EQ(chunks[1].GetCount(), 1);
  auto* b_chunk = chunks[1].GetChunkData<ResourceBChunk>();
  chunks[1].ConvertToPtr(&b_chunk->points);
  EXPECT_EQ(b_chunk->id, resource->GetResourceId());
  ASSERT_EQ(b_chunk->point_count, 3);
  ASSERT_EQ(b_chunk->points.ptr[0].x, 1);
  ASSERT_EQ(b_chunk->points.ptr[0].y, 2);
  ASSERT_EQ(b_chunk->points.ptr[0].z, 3);
  ASSERT_EQ(b_chunk->points.ptr[1].x, 4);
  ASSERT_EQ(b_chunk->points.ptr[1].y, 5);
  ASSERT_EQ(b_chunk->points.ptr[1].z, 6);
  ASSERT_EQ(b_chunk->points.ptr[2].x, 7);
  ASSERT_EQ(b_chunk->points.ptr[2].y, 8);
  ASSERT_EQ(b_chunk->points.ptr[2].z, 9);
}

TEST_F(ResourceFileTest, WriteInvalidResourceDependency) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceC>(
      kChunkTypeResourceC, GetResourceCWriter())));
  ResourcePtr<NoNameResourceA> resource_a = new NoNameResourceA(
      resource_manager_.NewResourceEntry<NoNameResourceA>());
  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), nullptr);
  EXPECT_FALSE(writer_->Write("mem:/file", resource.Get()));
}

TEST_F(ResourceFileTest, WriteResourceDependencies) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceC>(
      kChunkTypeResourceC, GetResourceCWriter())));
  ResourcePtr<ResourceA> resource_a =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});
  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), resource_b.Get());
  EXPECT_TRUE(writer_->Write(
      "mem:/file", resource.Get(),
      ContextBuilder()
          .SetValue<bool>(ResourceFileWriter::kKeyAllowUnnamedDependencies,
                          true)
          .Build()));

  auto file = file_system_->OpenFile("mem:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::vector<ChunkReader> chunks;
  ChunkType file_type;
  EXPECT_TRUE(ReadChunkFile(file.get(), &file_type, &chunks));
  EXPECT_EQ(file_type, kChunkTypeResourceC);
  ASSERT_EQ(chunks.size(), 2);

  ASSERT_EQ(chunks[0].GetType(), kChunkTypeResourceLoad);
  EXPECT_EQ(chunks[0].GetVersion(), 1);
  ASSERT_EQ(chunks[0].GetCount(), 2);
  auto* resource_load_chunks = chunks[0].GetChunkData<ResourceLoadChunk>();
  chunks[0].ConvertToPtr(&resource_load_chunks[0].type);
  chunks[0].ConvertToPtr(&resource_load_chunks[0].name);
  chunks[0].ConvertToPtr(&resource_load_chunks[1].type);
  chunks[0].ConvertToPtr(&resource_load_chunks[1].name);
  EXPECT_EQ(resource_load_chunks[0].id, resource_a->GetResourceId());
  EXPECT_STREQ(resource_load_chunks[0].type.ptr,
               resource_a->GetResourceType()->GetTypeName());
  EXPECT_EQ(resource_load_chunks[0].name.ptr, resource_a->GetResourceName());
  EXPECT_EQ(resource_load_chunks[1].id, resource_b->GetResourceId());
  EXPECT_STREQ(resource_load_chunks[1].type.ptr,
               resource_b->GetResourceType()->GetTypeName());
  EXPECT_EQ(resource_load_chunks[1].name.ptr, resource_b->GetResourceName());

  ASSERT_EQ(chunks[1].GetType(), kChunkTypeResourceC);
  EXPECT_EQ(chunks[1].GetVersion(), 1);
  ASSERT_EQ(chunks[1].GetCount(), 1);
  auto* chunk = chunks[1].GetChunkData<ResourceCChunk>();
  EXPECT_EQ(chunk->id, resource->GetResourceId());
  EXPECT_EQ(chunk->a_id, resource_a->GetResourceId());
  EXPECT_EQ(chunk->b_id, resource_b->GetResourceId());
}

TEST_F(ResourceFileTest, WriteFlatBufferResourceDependencies) {
  ASSERT_TRUE((writer_->RegisterResourceFlatBufferWriter<ResourceC>(
      kChunkTypeResourceC, 1, GetResourceCFlatBufferWriter())));
  ResourcePtr<ResourceA> resource_a =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});
  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), resource_b.Get());
  EXPECT_TRUE(writer_->Write(
      "mem:/file", resource.Get(),
      ContextBuilder()
          .SetValue<bool>(ResourceFileWriter::kKeyAllowUnnamedDependencies,
                          true)
          .Build()));

  auto file = file_system_->OpenFile("mem:/file", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  std::vector<ChunkReader> chunks;
  ChunkType file_type;
  EXPECT_TRUE(ReadChunkFile(file.get(), &file_type, &chunks));
  EXPECT_EQ(file_type, kChunkTypeResourceC);
  ASSERT_EQ(chunks.size(), 2);

  ASSERT_EQ(chunks[0].GetType(), kChunkTypeResourceLoad);
  EXPECT_EQ(chunks[0].GetVersion(), 1);
  ASSERT_EQ(chunks[0].GetCount(), 2);
  auto* resource_load_chunks = chunks[0].GetChunkData<ResourceLoadChunk>();
  chunks[0].ConvertToPtr(&resource_load_chunks[0].type);
  chunks[0].ConvertToPtr(&resource_load_chunks[0].name);
  chunks[0].ConvertToPtr(&resource_load_chunks[1].type);
  chunks[0].ConvertToPtr(&resource_load_chunks[1].name);
  EXPECT_EQ(resource_load_chunks[0].id, resource_a->GetResourceId());
  EXPECT_STREQ(resource_load_chunks[0].type.ptr,
               resource_a->GetResourceType()->GetTypeName());
  EXPECT_EQ(resource_load_chunks[0].name.ptr, resource_a->GetResourceName());
  EXPECT_EQ(resource_load_chunks[1].id, resource_b->GetResourceId());
  EXPECT_STREQ(resource_load_chunks[1].type.ptr,
               resource_b->GetResourceType()->GetTypeName());
  EXPECT_EQ(resource_load_chunks[1].name.ptr, resource_b->GetResourceName());

  ASSERT_EQ(chunks[1].GetType(), kChunkTypeResourceC);
  EXPECT_EQ(chunks[1].GetVersion(), 1);
  ASSERT_EQ(chunks[1].GetCount(), 1);
  auto* chunk_id = chunks[1].GetChunkData<ResourceId>();
  ASSERT_NE(chunk_id, nullptr);
  EXPECT_EQ(*chunk_id, resource->GetResourceId());
  auto* chunk = fb::GetRoot<fbs::ResourceCChunk>(chunk_id + 1);
  ASSERT_NE(chunk, nullptr);
  EXPECT_EQ(chunk->a_id(), resource_a->GetResourceId());
  EXPECT_EQ(chunk->b_id(), resource_b->GetResourceId());
}

TEST_F(ResourceFileTest, InvalidLoaderCreateContext) {
  EXPECT_EQ(ResourceFileReader::Create(Context{}), nullptr);
}

TEST_F(ResourceFileTest, DuplicateLoader) {
  EXPECT_TRUE(
      (reader_->template RegisterResourceChunk<ResourceA, ResourceAChunk>(
          kChunkTypeResourceA, 1, GetResourceALoader())));
  // New version is ok
  EXPECT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 2, GetResourceALoader())));
  // Duplicatate chunk and version is not.
  EXPECT_FALSE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
}

TEST_F(ResourceFileTest, DuplicateDifferentLoaders) {
  EXPECT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  // New version is ok
  EXPECT_TRUE(
      (reader_->RegisterResourceFlatBufferChunk<ResourceA, fbs::ResourceAChunk>(
          kChunkTypeResourceA, 2, GetResourceAFlatBufferLoader())));
  // Duplicatate chunk and version is not.
  EXPECT_FALSE(
      (reader_->RegisterResourceFlatBufferChunk<ResourceA, fbs::ResourceAChunk>(
          kChunkTypeResourceA, 1, GetResourceAFlatBufferLoader())));
}

TEST_F(ResourceFileTest, ReadUnknownResourceType) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceA>(resource_id), nullptr);

  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadMissingFile) {
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadInvalidFile) {
  file_system_->WriteFile("mem:/file", "hello");
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadInvalidChunk) {
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceA, {}));
  file->WriteString("hello");
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadWrongFileType) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceA>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  EXPECT_EQ(reader_->Read<ResourceB>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, DeleteEmbeddedResource) {
  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceBChunk>(kChunkTypeResourceB, 1));
  chunk_writers[0].GetChunkData<ResourceBChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceA, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadInvalidResourceLoadVersion) {
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});

  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceLoadChunk>(kChunkTypeResourceLoad, 2));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceAChunk>(kChunkTypeResourceA, 1));
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->id =
      resource_b->GetResourceId();
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->type =
      chunk_writers[0].AddString("ResourceB");
  chunk_writers[1].GetChunkData<ResourceAChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceA, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadInvalidResourceLoadResourceId) {
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});

  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceLoadChunk>(kChunkTypeResourceLoad, 1));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceAChunk>(kChunkTypeResourceA, 1));
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->id = 0;
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->type =
      chunk_writers[0].AddString("ResourceB");
  chunk_writers[1].GetChunkData<ResourceAChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceA, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadInvalidResourceLoadType) {
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});

  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceLoadChunk>(kChunkTypeResourceLoad, 1));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceAChunk>(kChunkTypeResourceA, 1));
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->id =
      resource_b->GetResourceId();
  chunk_writers[1].GetChunkData<ResourceAChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceA, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadUnknownResourceLoadType) {
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});

  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceLoadChunk>(kChunkTypeResourceLoad, 1));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceAChunk>(kChunkTypeResourceA, 1));
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->id =
      resource_b->GetResourceId();
  chunk_writers[0].GetChunkData<ResourceLoadChunk>()->type =
      chunk_writers[0].AddString("ResourceBB");
  chunk_writers[1].GetChunkData<ResourceAChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceA, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  EXPECT_EQ(reader_->Read<ResourceA>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, ReadResource) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceA>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  auto* loaded_resource = reader_->Read<ResourceA>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  EXPECT_EQ(resource->GetName(), "Name");
}

TEST_F(ResourceFileTest, ReadFlatBufferResource) {
  ASSERT_TRUE((writer_->RegisterResourceFlatBufferWriter<ResourceA>(
      kChunkTypeResourceA, 1, GetResourceAFlatBufferWriter())));
  ResourcePtr<ResourceA> resource =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceA>(resource_id), nullptr);

  ASSERT_TRUE(
      (reader_->RegisterResourceFlatBufferChunk<ResourceA, fbs::ResourceAChunk>(
          kChunkTypeResourceA, 1, GetResourceAFlatBufferLoader())));
  auto* loaded_resource = reader_->Read<ResourceA>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  EXPECT_EQ(resource->GetName(), "Name");
}

TEST_F(ResourceFileTest, ReadGenericChunk) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBWriter())));
  ResourcePtr<ResourceB> resource = new ResourceB(
      resource_manager_.NewResourceEntry<ResourceB>(),
      {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {{"alpha", 42.0f}, {"beta", 24.0f}});
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceB>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterGenericChunk<KeyValueChunk>(
      kChunkTypeKeyValue, 1, GetKeyValueLoader())));
  auto* loaded_resource = reader_->Read<ResourceB>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  auto points = resource->GetPoints();
  ASSERT_EQ(points.size(), 3);
  EXPECT_EQ(points[0].x, 1);
  EXPECT_EQ(points[0].y, 2);
  EXPECT_EQ(points[0].z, 3);
  EXPECT_EQ(points[1].x, 4);
  EXPECT_EQ(points[1].y, 5);
  EXPECT_EQ(points[1].z, 6);
  EXPECT_EQ(points[2].x, 7);
  EXPECT_EQ(points[2].y, 8);
  EXPECT_EQ(points[2].z, 9);
  const auto& values = resource->GetValues();
  EXPECT_THAT(values, Contains(Pair("alpha", 42.0f)));
  EXPECT_THAT(values, Contains(Pair("beta", 24.0f)));
}

TEST_F(ResourceFileTest, ReadGenericFlatBufferChunk) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBHybridWriter())));
  ResourcePtr<ResourceB> resource = new ResourceB(
      resource_manager_.NewResourceEntry<ResourceB>(),
      {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}}, {{"alpha", 42.0f}, {"beta", 24.0f}});
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceB>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBHybridLoader())));
  ASSERT_TRUE((reader_->RegisterGenericFlatBufferChunk<fbs::KeyValueChunk>(
      kChunkTypeKeyValue, 1, GetKeyValueFlatBufferLoader())));
  auto* loaded_resource = reader_->Read<ResourceB>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  auto points = resource->GetPoints();
  ASSERT_EQ(points.size(), 3);
  EXPECT_EQ(points[0].x, 1);
  EXPECT_EQ(points[0].y, 2);
  EXPECT_EQ(points[0].z, 3);
  EXPECT_EQ(points[1].x, 4);
  EXPECT_EQ(points[1].y, 5);
  EXPECT_EQ(points[1].z, 6);
  EXPECT_EQ(points[2].x, 7);
  EXPECT_EQ(points[2].y, 8);
  EXPECT_EQ(points[2].z, 9);
  const auto& values = resource->GetValues();
  EXPECT_THAT(values, Contains(Pair("alpha", 42.0f)));
  EXPECT_THAT(values, Contains(Pair("beta", 24.0f)));
}

TEST_F(ResourceFileTest, ReadUnknownChunkVersion) {
  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<KeyValueChunk>(kChunkTypeKeyValue, 2));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceBChunk>(kChunkTypeResourceB, 1));
  chunk_writers[0].GetChunkData<KeyValueChunk>()->key =
      chunk_writers[0].AddString("alpha");
  chunk_writers[0].GetChunkData<KeyValueChunk>()->value = 42.0f;
  chunk_writers[1].GetChunkData<ResourceBChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceB, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterGenericChunk<KeyValueChunk>(
      kChunkTypeKeyValue, 1, GetKeyValueLoader())));
  auto* loaded_resource = reader_->Read<ResourceB>("mem:/file");
  ASSERT_EQ(loaded_resource, nullptr);
}

TEST_F(ResourceFileTest, ReadMultipleGenericChunks) {
  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<KeyValueChunk>(kChunkTypeKeyValue, 1));
  chunk_writers.emplace_back(
      ChunkWriter::New<KeyValueChunk>(kChunkTypeKeyValue, 1));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceBChunk>(kChunkTypeResourceB, 1));
  chunk_writers[0].GetChunkData<KeyValueChunk>()->key =
      chunk_writers[0].AddString("alpha");
  chunk_writers[0].GetChunkData<KeyValueChunk>()->value = 42.0f;
  chunk_writers[1].GetChunkData<KeyValueChunk>()->key =
      chunk_writers[1].AddString("beta");
  chunk_writers[1].GetChunkData<KeyValueChunk>()->value = 24.0f;
  chunk_writers[2].GetChunkData<ResourceBChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceB, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterGenericChunk<KeyValueChunk>(
      kChunkTypeKeyValue, 1, GetKeyValueLoader())));
  auto* loaded_resource = reader_->Read<ResourceB>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), 1);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  ResourcePtr<ResourceB> resource = loaded_resource;
  EXPECT_THAT(resource->GetPoints(), IsEmpty());
  const auto& values = resource->GetValues();
  EXPECT_THAT(values, Contains(Pair("alpha", 42.0f)));
  EXPECT_THAT(values, Contains(Pair("beta", 24.0f)));
}

TEST_F(ResourceFileTest, ReadMultipleGenericChunksMultipleVersions) {
  std::vector<ChunkWriter> chunk_writers;
  chunk_writers.emplace_back(
      ChunkWriter::New<KeyValueChunk>(kChunkTypeKeyValue, 1));
  chunk_writers.emplace_back(
      ChunkWriter::New<KeyValueChunk>(kChunkTypeKeyValue, 2));
  chunk_writers.emplace_back(
      ChunkWriter::New<ResourceBChunk>(kChunkTypeResourceB, 1));
  chunk_writers[0].GetChunkData<KeyValueChunk>()->key =
      chunk_writers[0].AddString("alpha");
  chunk_writers[0].GetChunkData<KeyValueChunk>()->value = 42.0f;
  chunk_writers[1].GetChunkData<KeyValueChunk>()->key =
      chunk_writers[1].AddString("beta");
  chunk_writers[1].GetChunkData<KeyValueChunk>()->value = 24.0f;
  chunk_writers[2].GetChunkData<ResourceBChunk>()->id = 1;
  auto file = file_system_->OpenFile("mem:/file", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeResourceB, chunk_writers));
  file.reset();

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterGenericChunk<KeyValueChunk>(
      kChunkTypeKeyValue, 1, GetKeyValueLoader())));
  ASSERT_TRUE((reader_->RegisterGenericChunk<KeyValueChunk>(
      kChunkTypeKeyValue, 2, GetKeyValueLoader())));
  auto* loaded_resource = reader_->Read<ResourceB>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), 1);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  ResourcePtr<ResourceB> resource = loaded_resource;
  EXPECT_THAT(resource->GetPoints(), IsEmpty());
  const auto& values = resource->GetValues();
  EXPECT_THAT(values, Contains(Pair("alpha", 42.0f)));
  EXPECT_THAT(values, Not(Contains(Pair("beta", 24.0f))));
}

TEST_F(ResourceFileTest, ReadGenericChunkWithAcquire) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBWriter())));
  ResourcePtr<ResourceB> resource =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {},
                    {{"alpha", 42.0f}, {"beta", 24.0f}});
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceB>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterGenericChunk<KeyValueChunk>(
      kChunkTypeKeyValue, 1, GetKeyValueLoader())));
  acquire_key_values_ = true;
  auto* loaded_resource = reader_->Read<ResourceB>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  EXPECT_THAT(resource->GetPoints(), IsEmpty());
  EXPECT_THAT(resource->GetValues(), IsEmpty());
  EXPECT_NE(key_value_chunk_, nullptr);
  EXPECT_STREQ(key_value_chunk_[0].key.ptr, "alpha");
  EXPECT_EQ(key_value_chunk_[0].value, 42.0f);
  EXPECT_STREQ(key_value_chunk_[1].key.ptr, "beta");
  EXPECT_EQ(key_value_chunk_[1].value, 24.0f);
}

TEST_F(ResourceFileTest, ReadResourceDependencies) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceC>(
      kChunkTypeResourceC, GetResourceCWriter())));
  ResourcePtr<ResourceA> resource_a =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});
  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), resource_b.Get());
  EXPECT_TRUE(writer_->Write(
      "mem:/file", resource.Get(),
      ContextBuilder()
          .SetValue<bool>(ResourceFileWriter::kKeyAllowUnnamedDependencies,
                          true)
          .Build()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceC>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceC, ResourceCChunk>(
      kChunkTypeResourceC, 1, GetResourceCLoader())));
  auto* loaded_resource = reader_->Read<ResourceC>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  EXPECT_EQ(resource->GetA(), resource_a.Get());
  EXPECT_EQ(resource->GetB(), resource_b.Get());
}

TEST_F(ResourceFileTest, ReadFlatBufferResourceDependencies) {
  ASSERT_TRUE((writer_->RegisterResourceFlatBufferWriter<ResourceC>(
      kChunkTypeResourceC, 1, GetResourceCFlatBufferWriter())));
  ResourcePtr<ResourceA> resource_a =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});
  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), resource_b.Get());
  EXPECT_TRUE(writer_->Write(
      "mem:/file", resource.Get(),
      ContextBuilder()
          .SetValue<bool>(ResourceFileWriter::kKeyAllowUnnamedDependencies,
                          true)
          .Build()));
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceC>(resource_id), nullptr);

  ASSERT_TRUE(
      (reader_->RegisterResourceFlatBufferChunk<ResourceC, fbs::ResourceCChunk>(
          kChunkTypeResourceC, 1, GetResourceCFlatBufferLoader())));
  auto* loaded_resource = reader_->Read<ResourceC>("mem:/file");
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_FALSE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  EXPECT_EQ(resource->GetA(), resource_a.Get());
  EXPECT_EQ(resource->GetB(), resource_b.Get());
}

TEST_F(ResourceFileTest, LoadResourceDependenciesNoResourceSet) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBWriter())));
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceC>(
      kChunkTypeResourceC, GetResourceCWriter())));

  ResourcePtr<ResourceA> resource_a =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  auto reservation_a = resource_manager_.ReserveResourceName<ResourceA>(
      resource_a->GetResourceId(), "mem:/a");
  ASSERT_TRUE(reservation_a);
  ASSERT_TRUE(writer_->Write(reservation_a.GetName(), resource_a.Get()));
  reservation_a.Apply();

  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});
  auto reservation_b = resource_manager_.ReserveResourceName<ResourceB>(
      resource_b->GetResourceId(), "mem:/b");
  ASSERT_TRUE(reservation_b);
  ASSERT_TRUE(writer_->Write(reservation_b.GetName(), resource_b.Get()));
  reservation_b.Apply();

  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), resource_b.Get());
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_a_id = resource_a->GetResourceId();
  ResourceId resource_b_id = resource_b->GetResourceId();
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  resource_a.Reset();
  resource_b.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceA>(resource_a_id), nullptr);
  ASSERT_EQ(resource_system_->Get<ResourceB>(resource_b_id), nullptr);
  ASSERT_EQ(resource_system_->Get<ResourceC>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceC, ResourceCChunk>(
      kChunkTypeResourceC, 1, GetResourceCLoader())));

  ASSERT_EQ(reader_->Read<ResourceC>("mem:/file"), nullptr);
}

TEST_F(ResourceFileTest, LoadResourceDependencies) {
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceA>(
      kChunkTypeResourceA, GetResourceAWriter())));
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceB>(
      kChunkTypeResourceB, GetResourceBWriter())));
  ASSERT_TRUE((writer_->RegisterResourceWriter<ResourceC>(
      kChunkTypeResourceC, GetResourceCWriter())));

  ResourcePtr<ResourceA> resource_a =
      new ResourceA(resource_manager_.NewResourceEntry<ResourceA>(), "Name");
  auto reservation_a = resource_manager_.ReserveResourceName<ResourceA>(
      resource_a->GetResourceId(), "mem:/a");
  ASSERT_TRUE(reservation_a);
  ASSERT_TRUE(writer_->Write(reservation_a.GetName(), resource_a.Get()));
  reservation_a.Apply();

  ResourcePtr<ResourceB> resource_b =
      new ResourceB(resource_manager_.NewResourceEntry<ResourceB>(), {}, {});
  auto reservation_b = resource_manager_.ReserveResourceName<ResourceB>(
      resource_b->GetResourceId(), "mem:/b");
  ASSERT_TRUE(reservation_b);
  ASSERT_TRUE(writer_->Write(reservation_b.GetName(), resource_b.Get()));
  reservation_b.Apply();

  ResourcePtr<ResourceC> resource =
      new ResourceC(resource_manager_.NewResourceEntry<ResourceC>(),
                    resource_a.Get(), resource_b.Get());
  EXPECT_TRUE(writer_->Write("mem:/file", resource.Get()));
  ResourceId resource_a_id = resource_a->GetResourceId();
  ResourceId resource_b_id = resource_b->GetResourceId();
  ResourceId resource_id = resource->GetResourceId();
  resource.Reset();
  resource_a.Reset();
  resource_b.Reset();
  ASSERT_EQ(resource_system_->Get<ResourceA>(resource_a_id), nullptr);
  ASSERT_EQ(resource_system_->Get<ResourceB>(resource_b_id), nullptr);
  ASSERT_EQ(resource_system_->Get<ResourceC>(resource_id), nullptr);

  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceA, ResourceAChunk>(
      kChunkTypeResourceA, 1, GetResourceALoader())));
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceB, ResourceBChunk>(
      kChunkTypeResourceB, 1, GetResourceBLoader())));
  ASSERT_TRUE((reader_->RegisterResourceChunk<ResourceC, ResourceCChunk>(
      kChunkTypeResourceC, 1, GetResourceCLoader())));

  ResourceSet resource_set;
  auto* loaded_resource = reader_->Read<ResourceC>(
      "mem:/file", ContextBuilder().SetPtr<ResourceSet>(&resource_set).Build());
  ASSERT_NE(loaded_resource, nullptr);
  EXPECT_EQ(loaded_resource->GetResourceId(), resource_id);
  EXPECT_TRUE(loaded_resource->IsResourceReferenced());
  resource = loaded_resource;
  ASSERT_NE(resource->GetA(), nullptr);
  EXPECT_TRUE(resource->GetA()->IsResourceReferenced());
  EXPECT_EQ(resource->GetA()->GetResourceId(), resource_a_id);
  EXPECT_EQ(resource->GetA()->GetResourceName(), "mem:/a");
  ASSERT_NE(resource->GetB(), nullptr);
  EXPECT_TRUE(resource->GetB()->IsResourceReferenced());
  EXPECT_EQ(resource->GetB()->GetResourceId(), resource_b_id);
  EXPECT_EQ(resource->GetB()->GetResourceName(), "mem:/b");
}

}  // namespace gb
