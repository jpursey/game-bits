// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "absl/types/span.h"
#include "gb/file/chunk_reader.h"
#include "gb/file/chunk_types.h"
#include "gb/file/chunk_writer.h"
#include "gb/file/file_system.h"
#include "gb/file/memory_file_protocol.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

inline static constexpr ChunkType kChunkTypeExample = {'X', 'M', 'P', 'L'};
inline static constexpr ChunkType kChunkTypeBar = {'B', 'A', 'R', 0};

struct Foo {
  int32_t x, y, z;
};

struct Bar {
  float a, b, c;
};

struct Example {
  ChunkPtr<const char> name;
  float value;
  int32_t foo_count;
  ChunkPtr<const Foo> foos;
};

struct StringExample {
  ChunkPtr<const char> strings[9];  // One of each length.
};

class ChunkFileTest : public ::testing::Test {
 public:
  void SetUp() {
    file_system_ = std::make_unique<FileSystem>();
    file_system_->Register(std::make_unique<MemoryFileProtocol>());
  }

  template <typename Type>
  Type* ReadSingleChunkFile(std::string_view path,
                            const ChunkWriter& chunk_writer) {
    EXPECT_TRUE(file_system_->ReadFile(path, &file_contents_));
    EXPECT_EQ(file_contents_.size(),
              sizeof(ChunkHeader) + chunk_writer.GetSize());
    ChunkHeader* header = reinterpret_cast<ChunkHeader*>(file_contents_.data());
    EXPECT_EQ(header->type, chunk_writer.GetType());
    EXPECT_EQ(header->version, chunk_writer.GetVersion());
    EXPECT_EQ(header->size, chunk_writer.GetSize());
    EXPECT_EQ(header->count, chunk_writer.GetCount());
    if (HasFailure() || header->size == 0) {
      return nullptr;
    }
    return reinterpret_cast<Type*>(header + 1);
  }

  std::unique_ptr<FileSystem> file_system_;
  std::vector<uint8_t> file_contents_;
};

TEST_F(ChunkFileTest, ChunkTypeStringComparison) {
  EXPECT_EQ(kChunkTypeNone, "");
  EXPECT_EQ("", kChunkTypeNone);
  EXPECT_NE(kChunkTypeNone, "X");
  EXPECT_NE("X", kChunkTypeNone);
  EXPECT_EQ(kChunkTypeNone.ToString(), "");

  EXPECT_EQ(kChunkTypeFile, "GBFI");
  EXPECT_EQ("GBFI", kChunkTypeFile);
  EXPECT_NE(kChunkTypeFile, "GBFJ");
  EXPECT_NE("GBFJ", kChunkTypeFile);
  EXPECT_EQ(kChunkTypeFile.ToString(), "GBFI");
}

TEST_F(ChunkFileTest, ChunkTypeOperators) {
  EXPECT_TRUE(kChunkTypeNone == kChunkTypeNone);
  EXPECT_FALSE(kChunkTypeNone == kChunkTypeFile);
  EXPECT_TRUE(kChunkTypeFile == kChunkTypeFile);

  EXPECT_FALSE(kChunkTypeNone != kChunkTypeNone);
  EXPECT_TRUE(kChunkTypeNone != kChunkTypeFile);
  EXPECT_FALSE(kChunkTypeFile != kChunkTypeFile);

  EXPECT_TRUE(kChunkTypeNone < kChunkTypeFile);
  EXPECT_FALSE(kChunkTypeFile < kChunkTypeNone);
  EXPECT_FALSE(kChunkTypeFile < kChunkTypeFile);

  EXPECT_TRUE(kChunkTypeNone <= kChunkTypeFile);
  EXPECT_FALSE(kChunkTypeFile <= kChunkTypeNone);
  EXPECT_TRUE(kChunkTypeFile <= kChunkTypeFile);

  EXPECT_FALSE(kChunkTypeNone > kChunkTypeFile);
  EXPECT_TRUE(kChunkTypeFile > kChunkTypeNone);
  EXPECT_FALSE(kChunkTypeFile > kChunkTypeFile);

  EXPECT_FALSE(kChunkTypeNone >= kChunkTypeFile);
  EXPECT_TRUE(kChunkTypeFile >= kChunkTypeNone);
  EXPECT_TRUE(kChunkTypeFile >= kChunkTypeFile);
}

TEST_F(ChunkFileTest, ChunkWriterNewAlignedSingleChunk) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);

  EXPECT_EQ(chunk_writer.GetType(), kChunkTypeExample);
  EXPECT_EQ(chunk_writer.GetVersion(), 1);
  EXPECT_EQ(chunk_writer.GetSize(), sizeof(Example));
  EXPECT_EQ(chunk_writer.GetCount(), 1);
  auto* chunk = chunk_writer.GetChunkData<Example>();
  ASSERT_NE(chunk, nullptr);
  EXPECT_EQ(chunk->name.offset, 0);
  EXPECT_EQ(chunk->value, 0);
  EXPECT_EQ(chunk->foo_count, 0);
  EXPECT_EQ(chunk->foos.offset, 0);

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunk = ReadSingleChunkFile<Example>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunk, nullptr);
  EXPECT_EQ(read_chunk->name.offset, 0);
  EXPECT_EQ(read_chunk->value, 0);
  EXPECT_EQ(read_chunk->foo_count, 0);
  EXPECT_EQ(read_chunk->foos.offset, 0);
}

TEST_F(ChunkFileTest, ChunkWriterNewUnalignedSingleChunk) {
  auto chunk_writer = ChunkWriter::New<Bar>(kChunkTypeBar, 1);
  EXPECT_EQ(chunk_writer.GetType(), kChunkTypeBar);
  EXPECT_EQ(chunk_writer.GetVersion(), 1);
  EXPECT_EQ(chunk_writer.GetSize(), sizeof(Bar) + 4);
  EXPECT_EQ(chunk_writer.GetCount(), 1);
  auto* chunk = chunk_writer.GetChunkData<Bar>();
  ASSERT_NE(chunk, nullptr);
  EXPECT_EQ(chunk->a, 0);
  EXPECT_EQ(chunk->b, 0);
  EXPECT_EQ(chunk->c, 0);

  *chunk = {1, 2, 3};

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunk = ReadSingleChunkFile<Bar>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunk, nullptr);
  EXPECT_EQ(read_chunk->a, 1);
  EXPECT_EQ(read_chunk->b, 2);
  EXPECT_EQ(read_chunk->c, 3);
}

TEST_F(ChunkFileTest, ChunkWriterNewEmptyListChunk) {
  auto chunk_writer = ChunkWriter::New<Bar>(kChunkTypeBar, 2, 0);
  EXPECT_EQ(chunk_writer.GetType(), kChunkTypeBar);
  EXPECT_EQ(chunk_writer.GetVersion(), 2);
  EXPECT_EQ(chunk_writer.GetSize(), 0);
  EXPECT_EQ(chunk_writer.GetCount(), 0);
  auto* chunks = chunk_writer.GetChunkData<Bar>();
  EXPECT_EQ(chunks, nullptr);

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunks = ReadSingleChunkFile<Bar>("mem:/test", chunk_writer);
  ASSERT_EQ(read_chunks, nullptr);
}

TEST_F(ChunkFileTest, ChunkWriterNewAlignedListChunk) {
  auto chunk_writer = ChunkWriter::New<Bar>(kChunkTypeBar, 3, 2);
  EXPECT_EQ(chunk_writer.GetType(), kChunkTypeBar);
  EXPECT_EQ(chunk_writer.GetVersion(), 3);
  EXPECT_EQ(chunk_writer.GetSize(), sizeof(Bar) * 2);
  EXPECT_EQ(chunk_writer.GetCount(), 2);
  auto* chunks = chunk_writer.GetChunkData<Bar>();
  ASSERT_NE(chunks, nullptr);
  EXPECT_EQ(chunks[0].a, 0);
  EXPECT_EQ(chunks[0].b, 0);
  EXPECT_EQ(chunks[0].c, 0);
  EXPECT_EQ(chunks[1].a, 0);
  EXPECT_EQ(chunks[1].b, 0);
  EXPECT_EQ(chunks[1].c, 0);

  chunks[0] = {1, 2, 3};
  chunks[1] = {4, 5, 6};

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunks = ReadSingleChunkFile<Bar>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunks, nullptr);
  EXPECT_EQ(read_chunks[0].a, 1);
  EXPECT_EQ(read_chunks[0].b, 2);
  EXPECT_EQ(read_chunks[0].c, 3);
  EXPECT_EQ(read_chunks[1].a, 4);
  EXPECT_EQ(read_chunks[1].b, 5);
  EXPECT_EQ(read_chunks[1].c, 6);
}

TEST_F(ChunkFileTest, ChunkWriterNewUnalignedListChunk) {
  auto chunk_writer = ChunkWriter::New<Bar>(kChunkTypeBar, 4, 3);
  EXPECT_EQ(chunk_writer.GetType(), kChunkTypeBar);
  EXPECT_EQ(chunk_writer.GetVersion(), 4);
  EXPECT_EQ(chunk_writer.GetSize(), sizeof(Bar) * 3 + 4);
  EXPECT_EQ(chunk_writer.GetCount(), 3);
  auto* chunks = chunk_writer.GetChunkData<Bar>();
  ASSERT_NE(chunks, nullptr);
  EXPECT_EQ(chunks[0].a, 0);
  EXPECT_EQ(chunks[0].b, 0);
  EXPECT_EQ(chunks[0].c, 0);
  EXPECT_EQ(chunks[1].a, 0);
  EXPECT_EQ(chunks[1].b, 0);
  EXPECT_EQ(chunks[1].c, 0);
  EXPECT_EQ(chunks[2].a, 0);
  EXPECT_EQ(chunks[2].b, 0);
  EXPECT_EQ(chunks[2].c, 0);

  chunks[0] = {1, 2, 3};
  chunks[1] = {4, 5, 6};
  chunks[2] = {7, 8, 9};

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunks = ReadSingleChunkFile<Bar>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunks, nullptr);
  EXPECT_EQ(read_chunks[0].a, 1);
  EXPECT_EQ(read_chunks[0].b, 2);
  EXPECT_EQ(read_chunks[0].c, 3);
  EXPECT_EQ(read_chunks[1].a, 4);
  EXPECT_EQ(read_chunks[1].b, 5);
  EXPECT_EQ(read_chunks[1].c, 6);
  EXPECT_EQ(read_chunks[2].a, 7);
  EXPECT_EQ(read_chunks[2].b, 8);
  EXPECT_EQ(read_chunks[2].c, 9);
}

TEST_F(ChunkFileTest, ChunkWriterAddData) {
  auto chunk_writer = ChunkWriter::New<StringExample>(kChunkTypeExample, 1);
  auto* chunk = chunk_writer.GetChunkData<StringExample>();
  ASSERT_NE(chunk, nullptr);

  int32_t size = static_cast<int32_t>(sizeof(StringExample));
  chunk->strings[0] = chunk_writer.AddString("");
  EXPECT_EQ(chunk->strings[0].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[1] = chunk_writer.AddString("1");
  EXPECT_EQ(chunk->strings[1].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[2] = chunk_writer.AddString("12");
  EXPECT_EQ(chunk->strings[2].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[3] = chunk_writer.AddString("123");
  EXPECT_EQ(chunk->strings[3].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[4] = chunk_writer.AddString("1234");
  EXPECT_EQ(chunk->strings[4].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[5] = chunk_writer.AddString("12345");
  EXPECT_EQ(chunk->strings[5].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[6] = chunk_writer.AddString("123456");
  EXPECT_EQ(chunk->strings[6].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[7] = chunk_writer.AddString("1234567");
  EXPECT_EQ(chunk->strings[7].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  chunk->strings[8] = chunk_writer.AddString("12345678");
  EXPECT_EQ(chunk->strings[8].offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 16);

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunk =
      ReadSingleChunkFile<StringExample>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunk, nullptr);
  const char* read_chunk_data = reinterpret_cast<char*>(read_chunk);

  size = static_cast<int32_t>(sizeof(StringExample));
  EXPECT_STREQ(read_chunk_data + chunk->strings[0].offset, "");
  EXPECT_STREQ(read_chunk_data + chunk->strings[1].offset, "1");
  EXPECT_STREQ(read_chunk_data + chunk->strings[2].offset, "12");
  EXPECT_STREQ(read_chunk_data + chunk->strings[3].offset, "123");
  EXPECT_STREQ(read_chunk_data + chunk->strings[4].offset, "1234");
  EXPECT_STREQ(read_chunk_data + chunk->strings[5].offset, "12345");
  EXPECT_STREQ(read_chunk_data + chunk->strings[6].offset, "123456");
  EXPECT_STREQ(read_chunk_data + chunk->strings[7].offset, "1234567");
  EXPECT_STREQ(read_chunk_data + chunk->strings[8].offset, "12345678");
}

TEST_F(ChunkFileTest, ChunkWriterAddEmptyArray) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);
  auto* chunk = chunk_writer.GetChunkData<Example>();
  ASSERT_NE(chunk, nullptr);

  int32_t size = static_cast<int32_t>(sizeof(Example));
  chunk->foos = chunk_writer.AddData<const Foo>(nullptr);
  EXPECT_EQ(chunk->foos.offset, 0);
  EXPECT_EQ(chunk_writer.GetSize(), size);

  chunk->foos = chunk_writer.AddData<const Foo>(nullptr, 100);
  EXPECT_EQ(chunk->foos.offset, 0);
  EXPECT_EQ(chunk_writer.GetSize(), size);

  Foo foo = {1, 2, 3};
  chunk->foos = chunk_writer.AddData<const Foo>(&foo, 0);
  EXPECT_EQ(chunk->foos.offset, 0);
  EXPECT_EQ(chunk_writer.GetSize(), size);

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunk = ReadSingleChunkFile<Example>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunk, nullptr);

  EXPECT_EQ(read_chunk->foos.offset, 0);
}

TEST_F(ChunkFileTest, ChunkWriterAddOneFoo) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);
  auto* chunk = chunk_writer.GetChunkData<Example>();
  ASSERT_NE(chunk, nullptr);

  int32_t size = static_cast<int32_t>(sizeof(Example));
  Foo foo = {1, 2, 3};
  chunk->foos = chunk_writer.AddData<const Foo>(&foo, 1);
  EXPECT_EQ(chunk->foos.offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 16);

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunk = ReadSingleChunkFile<Example>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunk, nullptr);
  const char* read_chunk_data = reinterpret_cast<char*>(read_chunk);

  const Foo* read_foo =
      reinterpret_cast<const Foo*>(read_chunk_data + read_chunk->foos.offset);
  EXPECT_EQ(read_foo->x, 1);
  EXPECT_EQ(read_foo->y, 2);
  EXPECT_EQ(read_foo->z, 3);
}

TEST_F(ChunkFileTest, ChunkWriterAddTwoFoosAndName) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);
  auto* chunk = chunk_writer.GetChunkData<Example>();
  ASSERT_NE(chunk, nullptr);

  int32_t size = static_cast<int32_t>(sizeof(Example));
  chunk->name = chunk_writer.AddString("1234");
  EXPECT_EQ(chunk->name.offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 8);
  size += 8;

  Foo foos[2] = {{1, 2, 3}, {4, 5, 6}};
  chunk->foos = chunk_writer.AddData<const Foo>(foos, 2);
  EXPECT_EQ(chunk->foos.offset, size);
  EXPECT_EQ(chunk_writer.GetSize(), size + 24);

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  auto* read_chunk = ReadSingleChunkFile<Example>("mem:/test", chunk_writer);
  ASSERT_NE(read_chunk, nullptr);
  const char* read_chunk_data = reinterpret_cast<char*>(read_chunk);

  EXPECT_STREQ(read_chunk_data + read_chunk->name.offset, "1234");

  const Foo* read_foos =
      reinterpret_cast<const Foo*>(read_chunk_data + read_chunk->foos.offset);
  EXPECT_EQ(read_foos[0].x, 1);
  EXPECT_EQ(read_foos[0].y, 2);
  EXPECT_EQ(read_foos[0].z, 3);
  EXPECT_EQ(read_foos[1].x, 4);
  EXPECT_EQ(read_foos[1].y, 5);
  EXPECT_EQ(read_foos[1].z, 6);
}

TEST_F(ChunkFileTest, ChunkReaderFileTooSmallForHeader) {
  ChunkHeader header;
  header.type = kChunkTypeExample;
  header.size = 0;
  header.version = 1;
  header.count = 0;
  ASSERT_TRUE(
      file_system_->WriteFile("mem:/test", &header, sizeof(header) - 1));
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
    ASSERT_NE(file, nullptr);
    EXPECT_FALSE(ChunkReader::Read(file.get(), has_error_ptr));
    if (has_error_ptr == nullptr) {
      break;
    }
    EXPECT_TRUE(has_error);
  }
}

TEST_F(ChunkFileTest, ChunkReaderFileTooSmallForSize) {
  ChunkHeader header;
  header.type = kChunkTypeExample;
  header.size = 8;
  header.version = 1;
  header.count = 0;
  std::vector<uint8_t> file_contents(sizeof(header) + 7);
  std::memcpy(file_contents.data(), &header, sizeof(header));
  ASSERT_TRUE(file_system_->WriteFile("mem:/test", file_contents));
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
    ASSERT_NE(file, nullptr);
    EXPECT_FALSE(ChunkReader::Read(file.get(), has_error_ptr));
    if (has_error_ptr == nullptr) {
      break;
    }
    EXPECT_TRUE(has_error);
  }
}

TEST_F(ChunkFileTest, ChunkReaderFileSizeInvalid) {
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    for (int size = -1; size < 8; ++size) {
      if (size == 0) {
        continue;
      }
      ChunkHeader header;
      header.type = kChunkTypeExample;
      header.size = size;
      header.version = 1;
      header.count = 0;
      std::vector<uint8_t> file_contents(sizeof(header) + std::max(size, 0));
      std::memcpy(file_contents.data(), &header, sizeof(header));
      ASSERT_TRUE(file_system_->WriteFile("mem:/test", file_contents));
      auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
      ASSERT_NE(file, nullptr);
      has_error = false;
      EXPECT_FALSE(ChunkReader::Read(file.get(), has_error_ptr))
          << "Size=" << size;
      EXPECT_TRUE(has_error_ptr == nullptr || has_error) << "Size=" << size;
    }
    if (has_error_ptr == nullptr) {
      break;
    }
  }
}

TEST_F(ChunkFileTest, ChunkReaderInvalidVersion) {
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    for (int version = -1; version < 1; ++version) {
      ChunkHeader header;
      header.type = kChunkTypeExample;
      header.size = 0;
      header.version = version;
      header.count = 0;
      ASSERT_TRUE(
          file_system_->WriteFile("mem:/test", &header, sizeof(header)));
      auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
      ASSERT_NE(file, nullptr);
      has_error = false;
      EXPECT_FALSE(ChunkReader::Read(file.get(), has_error_ptr))
          << "Version=" << version;
      EXPECT_TRUE(has_error_ptr == nullptr || has_error)
          << "Version=" << version;
    }
    if (has_error_ptr == nullptr) {
      break;
    }
  }
}

TEST_F(ChunkFileTest, ChunkReaderInvalidCount) {
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    for (int count = -1; count < 2; ++count) {
      if (count == 0) {
        continue;
      }
      ChunkHeader header;
      header.type = kChunkTypeExample;
      header.size = 0;
      header.version = 1;
      header.count = count;
      ASSERT_TRUE(
          file_system_->WriteFile("mem:/test", &header, sizeof(header)));
      auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
      ASSERT_NE(file, nullptr);
      has_error = false;
      EXPECT_FALSE(ChunkReader::Read(file.get(), has_error_ptr))
          << "Count=" << count;
      EXPECT_TRUE(has_error_ptr == nullptr || has_error) << "Count=" << count;
    }
    if (has_error_ptr == nullptr) {
      break;
    }
  }
}

TEST_F(ChunkFileTest, ChunkReaderEmptyFile) {
  ASSERT_TRUE(file_system_->WriteFile("mem:/test", nullptr, 0));
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
    ASSERT_NE(file, nullptr);
    has_error = true;
    EXPECT_FALSE(ChunkReader::Read(file.get(), has_error_ptr));
    if (has_error_ptr == nullptr) {
      break;
    }
    EXPECT_FALSE(has_error);
  }
}

TEST_F(ChunkFileTest, ChunkReaderEmptyChunk) {
  ChunkHeader header;
  header.type = kChunkTypeExample;
  header.size = 0;
  header.version = 1;
  header.count = 0;
  ASSERT_TRUE(file_system_->WriteFile("mem:/test", &header, sizeof(header)));
  bool has_error = false;
  for (bool* has_error_ptr = &has_error; true; has_error_ptr = nullptr) {
    auto file = file_system_->OpenFile("mem:/test", kReadFileFlags);
    ASSERT_NE(file, nullptr);
    has_error = true;
    auto chunk_reader = ChunkReader::Read(file.get(), has_error_ptr);
    EXPECT_EQ(file->GetPosition(), sizeof(header));
    ASSERT_TRUE(chunk_reader);
    EXPECT_EQ(chunk_reader->GetType(), kChunkTypeExample);
    EXPECT_EQ(chunk_reader->GetSize(), 0);
    EXPECT_EQ(chunk_reader->GetVersion(), 1);
    EXPECT_EQ(chunk_reader->GetCount(), 0);
    EXPECT_EQ(chunk_reader->GetChunkData<Example>(), nullptr);
    if (has_error_ptr == nullptr) {
      break;
    }
    EXPECT_FALSE(has_error);
  }
}

TEST_F(ChunkFileTest, ChunkReaderTwoFoosAndName) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);
  auto* chunk = chunk_writer.GetChunkData<Example>();
  ASSERT_NE(chunk, nullptr);
  chunk->name = chunk_writer.AddString("1234");
  chunk->foos = chunk_writer.AddData<const Foo>({Foo{1, 2, 3}, Foo{4, 5, 6}});
  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  bool has_error = true;
  auto chunk_reader = ChunkReader::Read(file.get(), &has_error);
  EXPECT_EQ(file->GetPosition(), sizeof(ChunkHeader) + chunk_writer.GetSize());
  ASSERT_TRUE(chunk_reader);
  EXPECT_FALSE(has_error);
  EXPECT_EQ(chunk_reader->GetType(), kChunkTypeExample);
  EXPECT_EQ(chunk_reader->GetSize(), chunk_writer.GetSize());
  EXPECT_EQ(chunk_reader->GetVersion(), 1);
  EXPECT_EQ(chunk_reader->GetCount(), 1);

  auto* read_chunk = chunk_reader->GetChunkData<Example>();
  ASSERT_NE(read_chunk, nullptr);
  chunk_reader->ConvertToPtr(&read_chunk->name);
  chunk_reader->ConvertToPtr(&read_chunk->foos);
  EXPECT_STREQ(read_chunk->name.ptr, "1234");
  EXPECT_EQ(read_chunk->foos.ptr[0].x, 1);
  EXPECT_EQ(read_chunk->foos.ptr[0].y, 2);
  EXPECT_EQ(read_chunk->foos.ptr[0].z, 3);
  EXPECT_EQ(read_chunk->foos.ptr[1].x, 4);
  EXPECT_EQ(read_chunk->foos.ptr[1].y, 5);
  EXPECT_EQ(read_chunk->foos.ptr[1].z, 6);
}

TEST_F(ChunkFileTest, ChunkReaderNullPtrs) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);
  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  bool has_error = true;
  auto chunk_reader = ChunkReader::Read(file.get(), &has_error);
  EXPECT_EQ(file->GetPosition(), sizeof(ChunkHeader) + chunk_writer.GetSize());
  ASSERT_TRUE(chunk_reader);
  EXPECT_FALSE(has_error);

  auto* read_chunk = chunk_reader->GetChunkData<Example>();
  ASSERT_NE(read_chunk, nullptr);
  chunk_reader->ConvertToPtr(&read_chunk->name);
  chunk_reader->ConvertToPtr(&read_chunk->foos);
  EXPECT_EQ(read_chunk->name.ptr, nullptr);
  EXPECT_EQ(read_chunk->foos.ptr, nullptr);
}

TEST_F(ChunkFileTest, ChunkReaderListChunk) {
  auto chunk_writer = ChunkWriter::New<Bar>(kChunkTypeBar, 4, 3);
  auto* chunks = chunk_writer.GetChunkData<Bar>();
  ASSERT_NE(chunks, nullptr);
  chunks[0] = {1, 2, 3};
  chunks[1] = {4, 5, 6};
  chunks[2] = {7, 8, 9};
  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  bool has_error = true;
  auto chunk_reader = ChunkReader::Read(file.get(), &has_error);
  EXPECT_EQ(file->GetPosition(), sizeof(ChunkHeader) + chunk_writer.GetSize());
  ASSERT_TRUE(chunk_reader);
  EXPECT_FALSE(has_error);
  EXPECT_EQ(chunk_reader->GetType(), kChunkTypeBar);
  EXPECT_EQ(chunk_reader->GetSize(), chunk_writer.GetSize());
  EXPECT_EQ(chunk_reader->GetVersion(), 4);
  EXPECT_EQ(chunk_reader->GetCount(), 3);

  auto* read_chunks = chunk_reader->GetChunkData<Bar>();
  ASSERT_NE(read_chunks, nullptr);
  EXPECT_EQ(read_chunks[0].a, 1);
  EXPECT_EQ(read_chunks[0].b, 2);
  EXPECT_EQ(read_chunks[0].c, 3);
  EXPECT_EQ(read_chunks[1].a, 4);
  EXPECT_EQ(read_chunks[1].b, 5);
  EXPECT_EQ(read_chunks[1].c, 6);
  EXPECT_EQ(read_chunks[2].a, 7);
  EXPECT_EQ(read_chunks[2].b, 8);
  EXPECT_EQ(read_chunks[2].c, 9);
}

TEST_F(ChunkFileTest, ChunkReaderRelease) {
  auto chunk_writer = ChunkWriter::New<Example>(kChunkTypeExample, 1);
  auto* chunk = chunk_writer.GetChunkData<Example>();
  ASSERT_NE(chunk, nullptr);
  chunk->name = chunk_writer.AddString("1234");
  chunk->foos = chunk_writer.AddData<const Foo>({Foo{1, 2, 3}, Foo{4, 5, 6}});
  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_NE(file, nullptr);
  ASSERT_TRUE(chunk_writer.Write(file.get()));
  file.reset();

  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_NE(file, nullptr);
  auto chunk_reader = ChunkReader::Read(file.get());
  EXPECT_EQ(file->GetPosition(), sizeof(ChunkHeader) + chunk_writer.GetSize());
  ASSERT_TRUE(chunk_reader);
  auto* read_chunk = chunk_reader->GetChunkData<Example>();
  ASSERT_NE(read_chunk, nullptr);
  chunk_reader->ConvertToPtr(&read_chunk->name);
  chunk_reader->ConvertToPtr(&read_chunk->foos);

  auto* released_chunk = chunk_reader->ReleaseChunkData<Example>();
  ASSERT_EQ(released_chunk, read_chunk);
  EXPECT_EQ(chunk_reader->GetChunkData<Example>(), nullptr);
  EXPECT_EQ(chunk_reader->ReleaseChunkData<Example>(), nullptr);
  EXPECT_EQ(chunk_reader->GetType(), kChunkTypeExample);
  EXPECT_EQ(chunk_reader->GetSize(), chunk_writer.GetSize());
  EXPECT_EQ(chunk_reader->GetVersion(), 1);
  EXPECT_EQ(chunk_reader->GetCount(), 1);
  chunk_reader = {};

  EXPECT_STREQ(released_chunk->name.ptr, "1234");
  EXPECT_EQ(released_chunk->foos.ptr[0].x, 1);
  EXPECT_EQ(released_chunk->foos.ptr[0].y, 2);
  EXPECT_EQ(released_chunk->foos.ptr[0].z, 3);
  EXPECT_EQ(released_chunk->foos.ptr[1].x, 4);
  EXPECT_EQ(released_chunk->foos.ptr[1].y, 5);
  EXPECT_EQ(released_chunk->foos.ptr[1].z, 6);
  std::free(released_chunk);
}

TEST_F(ChunkFileTest, ReadWriteMultiChunkFile) {
  std::vector<ChunkWriter> write_chunks;

  write_chunks.emplace_back(ChunkWriter::New<Bar>(kChunkTypeBar, 4, 3));
  auto* bar_chunks = write_chunks.back().GetChunkData<Bar>();
  ASSERT_NE(bar_chunks, nullptr);
  bar_chunks[0] = {1, 2, 3};
  bar_chunks[1] = {4, 5, 6};
  bar_chunks[2] = {7, 8, 9};

  write_chunks.emplace_back(ChunkWriter::New<Example>(kChunkTypeExample, 1));
  auto* example_chunk = write_chunks.back().GetChunkData<Example>();
  ASSERT_NE(example_chunk, nullptr);
  example_chunk->name = write_chunks.back().AddString("1234");
  example_chunk->value = 42;
  example_chunk->foo_count = 2;
  example_chunk->foos =
      write_chunks.back().AddData<const Foo>({Foo{1, 2, 3}, Foo{4, 5, 6}});

  auto file = file_system_->OpenFile("mem:/test", kNewFileFlags);
  ASSERT_TRUE(WriteChunkFile(file.get(), kChunkTypeExample, write_chunks));
  file.reset();

  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_TRUE(ReadChunkFile(file.get(), nullptr, nullptr));
  file.reset();

  ChunkType file_type;
  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_TRUE(ReadChunkFile(file.get(), &file_type, nullptr));
  file.reset();
  EXPECT_EQ(file_type, kChunkTypeExample);

  std::vector<ChunkReader> read_chunks;
  file = file_system_->OpenFile("mem:/test", kReadFileFlags);
  ASSERT_TRUE(ReadChunkFile(file.get(), &file_type, &read_chunks));
  file.reset();

  ASSERT_EQ(read_chunks.size(), 2);

  EXPECT_EQ(read_chunks[0].GetType(), write_chunks[0].GetType());
  EXPECT_EQ(read_chunks[0].GetSize(), write_chunks[0].GetSize());
  EXPECT_EQ(read_chunks[0].GetVersion(), write_chunks[0].GetVersion());
  EXPECT_EQ(read_chunks[0].GetCount(), write_chunks[0].GetCount());
  bar_chunks = read_chunks[0].GetChunkData<Bar>();
  ASSERT_NE(bar_chunks, nullptr);
  EXPECT_EQ(bar_chunks[0].a, 1);
  EXPECT_EQ(bar_chunks[0].b, 2);
  EXPECT_EQ(bar_chunks[0].c, 3);
  EXPECT_EQ(bar_chunks[1].a, 4);
  EXPECT_EQ(bar_chunks[1].b, 5);
  EXPECT_EQ(bar_chunks[1].c, 6);
  EXPECT_EQ(bar_chunks[2].a, 7);
  EXPECT_EQ(bar_chunks[2].b, 8);
  EXPECT_EQ(bar_chunks[2].c, 9);

  EXPECT_EQ(read_chunks[1].GetType(), write_chunks[1].GetType());
  EXPECT_EQ(read_chunks[1].GetSize(), write_chunks[1].GetSize());
  EXPECT_EQ(read_chunks[1].GetVersion(), write_chunks[1].GetVersion());
  EXPECT_EQ(read_chunks[1].GetCount(), write_chunks[1].GetCount());
  example_chunk = read_chunks[1].GetChunkData<Example>();
  ASSERT_NE(example_chunk, nullptr);
  read_chunks[1].ConvertToPtr(&example_chunk->name);
  read_chunks[1].ConvertToPtr(&example_chunk->foos);
  EXPECT_STREQ(example_chunk->name.ptr, "1234");
  EXPECT_EQ(example_chunk->value, 42);
  EXPECT_EQ(example_chunk->foo_count, 2);
  EXPECT_EQ(example_chunk->foos.ptr[0].x, 1);
  EXPECT_EQ(example_chunk->foos.ptr[0].y, 2);
  EXPECT_EQ(example_chunk->foos.ptr[0].z, 3);
  EXPECT_EQ(example_chunk->foos.ptr[1].x, 4);
  EXPECT_EQ(example_chunk->foos.ptr[1].y, 5);
  EXPECT_EQ(example_chunk->foos.ptr[1].z, 6);
}

}  // namespace
}  // namespace gb
