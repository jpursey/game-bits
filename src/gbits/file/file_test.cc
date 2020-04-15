#include "gbits/file/file.h"

#include <cstring>

#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "gbits/file/file_system.h"
#include "gbits/file/test_protocol.h"
#include "gbits/test/test_util.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

struct Item {
  char name[20];
  int32_t value;
};

bool operator==(const Item& a, const Item& b) {
  return std::string_view(a.name) == b.name && a.value == b.value;
}
std::ostream& operator<<(std::ostream& out, const Item& item) {
  return out << absl::StrCat("{ name=\"", item.name, "\", value=", item.value,
                             " }");
}

TEST(FileTest, OpenAndClose) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  auto* file_state = state.paths["/file"].GetFile();

  auto file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);
  EXPECT_NE(file_state->file, nullptr);
  EXPECT_EQ(file_state->flags, FileFlag::kRead);
  EXPECT_EQ(file_state->position, 0);
  EXPECT_EQ(file_state->contents, "1234567890");
  EXPECT_EQ(file_state->seek_end_count, 0);
  EXPECT_EQ(file_state->seek_to_count, 0);
  EXPECT_EQ(file_state->write_count, 0);
  EXPECT_EQ(file_state->read_count, 0);

  file.reset();
  EXPECT_EQ(file_state->file, nullptr);
  EXPECT_EQ(file_state->seek_end_count, 0);
  EXPECT_EQ(file_state->seek_to_count, 0);
  EXPECT_EQ(file_state->write_count, 0);
  EXPECT_EQ(file_state->read_count, 0);
  EXPECT_EQ(file_state->invalid_call_count, 0);
}

TEST(FileTest, Seek) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  auto* file_state = state.paths["/file"].GetFile();
  auto file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);

  EXPECT_EQ(file->SeekEnd(), 10);
  EXPECT_EQ(file_state->seek_end_count, 1);
  EXPECT_EQ(file->GetPosition(), 10);
  EXPECT_EQ(file_state->position, 10);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekBegin(), 0);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file->GetPosition(), 0);
  EXPECT_EQ(file_state->position, 0);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekTo(5), 5);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file->GetPosition(), 5);
  EXPECT_EQ(file_state->position, 5);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekBy(2), 7);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file->GetPosition(), 7);
  EXPECT_EQ(file_state->position, 7);
  file_state->ResetCounts();

  EXPECT_EQ(file->SeekBy(-4), 3);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file->GetPosition(), 3);
  EXPECT_EQ(file_state->position, 3);
  file_state->ResetCounts();

  file_state->position = -1;
  EXPECT_EQ(file->SeekTo(5), -1);
  EXPECT_EQ(file_state->seek_to_count, 1);
  EXPECT_EQ(file->GetPosition(), -1);
  EXPECT_EQ(file_state->position, -1);
  file_state->ResetCounts();
}

TEST(FileTest, Write) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  auto* file_state = state.paths["/file"].GetFile();
  auto file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  ASSERT_NE(file, nullptr);
  const char buffer[] = "abcdefghijklmnopqrstuvwxyz";
  const size_t buffer_length = strlen(buffer);

  EXPECT_EQ(file->Write(buffer, 5), 5);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 5);
  EXPECT_EQ(file_state->contents, "abcde67890");

  EXPECT_EQ(file->Write(buffer, 5), 5);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 10);
  EXPECT_EQ(file_state->contents, "abcdeabcde");

  char ch = 'Z';
  EXPECT_EQ(file->Write(&ch), 1);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 11);
  EXPECT_EQ(file_state->contents, "abcdeabcdeZ");

  const void* void_buffer = buffer;
  EXPECT_EQ(file->SeekBegin(), 0);
  EXPECT_EQ(file->Write(void_buffer, buffer_length), buffer_length);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, buffer_length);
  EXPECT_EQ(file_state->contents, buffer);

  const std::string text("hello");
  EXPECT_EQ(file->Write(text), text.size());
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, buffer_length + text.size());
  EXPECT_TRUE(absl::EndsWith(file_state->contents, text));

  const std::string_view text_view("good-bye");
  EXPECT_EQ(file->Write(text_view), text_view.size());
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position,
            buffer_length + text.size() + text_view.size());
  EXPECT_TRUE(absl::EndsWith(file_state->contents, text_view));

  file = nullptr;
  file_state->contents.clear();
  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  ASSERT_NE(file, nullptr);

  Item items[2] = {
      {"hello", 42},
      {"goodbye", 24},
  };
  EXPECT_EQ(file->Write(items, 2), 2);
  EXPECT_EQ(file_state->position, sizeof(items));
  EXPECT_EQ(file_state->contents.size(), sizeof(items));
  const Item* written_items =
      reinterpret_cast<const Item*>(file_state->contents.data());
  EXPECT_STREQ(items[0].name, written_items[0].name);
  EXPECT_EQ(items[0].value, written_items[0].value);
  EXPECT_STREQ(items[1].name, written_items[1].name);
  EXPECT_EQ(items[1].value, written_items[1].value);

  // Deliberately reverse so the file contents will be different.
  const std::vector<Item> vector_items = {items[1], items[0]};
  file->SeekBegin();
  EXPECT_EQ(file->Write(vector_items), 2);
  EXPECT_EQ(file_state->position, sizeof(items));
  EXPECT_EQ(file_state->contents.size(), sizeof(items));
  written_items = reinterpret_cast<const Item*>(file_state->contents.data());
  EXPECT_STREQ(vector_items[0].name, written_items[0].name);
  EXPECT_EQ(vector_items[0].value, written_items[0].value);
  EXPECT_STREQ(vector_items[1].name, written_items[1].name);
  EXPECT_EQ(vector_items[1].value, written_items[1].value);

  file_state->position = -1;
  EXPECT_EQ(file->SeekTo(5), -1);
  EXPECT_FALSE(file->IsValid());
  EXPECT_EQ(file->Write(buffer, 5), 0);

  file.reset();
  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->GetPosition(), 0);
  EXPECT_EQ(file->Write(buffer, 5), 0);
  EXPECT_EQ(file->GetPosition(), 0);
}

TEST(FileTest, Read) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  auto* file_state = state.paths["/file"].GetFile();
  auto file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);
  char buffer[20];

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, 5), 5);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 5);
  EXPECT_STREQ(buffer, "12345");

  std::memset(buffer, 0, sizeof(buffer));
  EXPECT_EQ(file->Read(buffer, sizeof(buffer)), 5);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 10);
  EXPECT_STREQ(buffer, "67890");

  char ch = '\0';
  EXPECT_EQ(file->SeekBegin(), 0);
  EXPECT_EQ(file->Read(&ch), 1);
  EXPECT_EQ(ch, '1');
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 1);

  void* void_buffer = buffer;
  EXPECT_EQ(file->Read(void_buffer, sizeof(buffer)), 9);
  EXPECT_EQ(file->GetPosition(), file_state->position);
  EXPECT_EQ(file_state->position, 10);
  EXPECT_STREQ(buffer, "234567890");

  Item items[2] = {
      {"hello", 42},
      {"goodbye", 24},
  };
  file_state->contents.resize(sizeof(items));
  std::memcpy(file_state->contents.data(), items, sizeof(items));
  EXPECT_EQ(file->SeekBegin(), 0);
  Item read_items[2] = {};
  EXPECT_EQ(file->Read(read_items, 2), 2);
  EXPECT_STREQ(items[0].name, read_items[0].name);
  EXPECT_EQ(items[0].value, read_items[0].value);
  EXPECT_STREQ(items[1].name, read_items[1].name);
  EXPECT_EQ(items[1].value, read_items[1].value);

  file.reset();
  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  ASSERT_NE(file, nullptr);
  EXPECT_EQ(file->GetPosition(), 0);
  int value = 42;
  EXPECT_EQ(file->Read(&value), 0);
  EXPECT_EQ(file->GetPosition(), 0);
}

TEST(FileTest, ReadRemainingString) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  auto* file_state = state.paths["/file"].GetFile();
  auto file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);

  std::string buffer;
  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_EQ(buffer, file_state->contents);

  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());

  EXPECT_EQ(file->SeekTo(5), 5);
  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_EQ(buffer, "67890");

  buffer = "not empty";
  file_state->position = -1;
  EXPECT_TRUE(file->IsValid());
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());

  buffer = "not empty";
  EXPECT_FALSE(file->IsValid());
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());

  file.reset();
  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);
  file_state->fail_read_after = 5;
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_EQ(buffer, "12345");
  file_state->fail_read_after = -1;

  file.reset();
  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  ASSERT_NE(file, nullptr);
  buffer = "not empty";
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());
}

TEST(FileTest, ReadRemainingVector) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  const Item items[4] = {
      {"hello", 42},
      {"goodbye", 24},
      {"big number!", 12345678},
      {"negative...", -12345678},
  };
  std::string items_as_bytes;
  items_as_bytes.resize(sizeof(items));
  std::memcpy(&items_as_bytes[0], items, sizeof(items));

  state.paths["/file"] = TestProtocol::PathState::NewFile(items_as_bytes);
  auto* file_state = state.paths["/file"].GetFile();
  auto file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);

  std::vector<Item> buffer;
  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, ElementsAreArray(items));

  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());

  EXPECT_EQ(file->SeekTo(sizeof(Item) * 2), sizeof(Item) * 2);
  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, ElementsAre(items[2], items[3]));

  buffer.push_back(items[0]);
  file_state->position = -1;
  EXPECT_TRUE(file->IsValid());
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());

  buffer.push_back(items[0]);
  EXPECT_FALSE(file->IsValid());
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());

  file.reset();
  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);
  file_state->fail_read_after = sizeof(Item) * 2;
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, ElementsAre(items[0], items[1]));
  file_state->fail_read_after = -1;

  file.reset();
  file_state->contents.pop_back();  // Delete one byte!
  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  ASSERT_NE(file, nullptr);
  EXPECT_TRUE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, ElementsAre(items[0], items[1], items[2]));
  EXPECT_EQ(file->GetPosition(), sizeof(Item) * 3);

  file.reset();
  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  ASSERT_NE(file, nullptr);
  buffer.push_back(items[0]);
  EXPECT_FALSE(file->ReadRemaining(&buffer));
  EXPECT_THAT(buffer, IsEmpty());
}

TEST(FileTest, ReadLineFails) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile("1234567890");
  auto* file_state = state.paths["/file"].GetFile();
  std::string line;
  std::vector<std::string> lines;

  auto file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  line = "not empty";
  EXPECT_FALSE(file->ReadLine(&line));
  EXPECT_THAT(line, IsEmpty());
  EXPECT_EQ(file->ReadLines(1, &lines), 0);
  EXPECT_THAT(lines, IsEmpty());
  EXPECT_EQ(file->ReadRemainingLines(&lines), 0);
  EXPECT_THAT(lines, IsEmpty());
  EXPECT_EQ(file->GetPosition(), 0);
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  file_state->fail_seek = true;
  file->SeekBegin();
  line = "not empty";
  EXPECT_FALSE(file->ReadLine(&line));
  EXPECT_THAT(line, IsEmpty());
  EXPECT_EQ(file->ReadLines(1, &lines), 0);
  EXPECT_THAT(lines, IsEmpty());
  EXPECT_EQ(file->ReadRemainingLines(&lines), 0);
  EXPECT_THAT(lines, IsEmpty());
  EXPECT_EQ(file->GetPosition(), -1);
  file_state->fail_seek = false;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  file_state->fail_read_after = 5;
  line = "not_empty";
  EXPECT_TRUE(file->ReadLine(&line));
  EXPECT_EQ(line, "12345");
  file_state->fail_read_after = -1;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  file_state->fail_read_after = 5;
  EXPECT_EQ(file->ReadLines(1, &lines), 1);
  EXPECT_THAT(lines, ElementsAre("12345"));
  file_state->fail_read_after = -1;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kRead);
  file_state->fail_read_after = 5;
  EXPECT_EQ(file->ReadRemainingLines(&lines), 1);
  EXPECT_THAT(lines, ElementsAre("12345"));
  file_state->fail_read_after = -1;
  file.reset();
}

TEST(FileTest, WriteLineFails) {
  TestProtocol::State state;
  FileSystem file_system;
  file_system.Register(std::make_unique<TestProtocol>(&state), "test");

  state.paths["/file"] = TestProtocol::PathState::NewFile();
  auto* file_state = state.paths["/file"].GetFile();

  auto file = file_system.OpenFile("test:/file", FileFlag::kRead);
  EXPECT_FALSE(file->WriteLine("1234567890"));
  EXPECT_EQ(file->GetPosition(), 0);
  EXPECT_EQ(file->WriteLines(std::vector<const char*>{"1234567890"}), 0);
  EXPECT_EQ(file->GetPosition(), 0);
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  file_state->fail_seek = true;
  file->SeekEnd();
  EXPECT_FALSE(file->WriteLine("1234567890"));
  EXPECT_EQ(file->WriteLines(std::vector<const char*>{"1234567890"}), 0);
  file_state->fail_seek = false;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  file_state->fail_write_after = 5;
  EXPECT_FALSE(file->WriteLine("1234567890"));
  EXPECT_EQ(file_state->contents, "12345");
  EXPECT_EQ(file->GetPosition(), 5);
  file_state->fail_write_after = -1;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  file_state->fail_write_after = 5;
  EXPECT_EQ(file->WriteLines(std::vector<const char*>{"1234567890"}), 0);
  EXPECT_EQ(file_state->contents, "12345");
  EXPECT_EQ(file->GetPosition(), 5);
  file_state->fail_write_after = -1;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  file_state->fail_write_after = 10;
  EXPECT_FALSE(file->WriteLine("1234567890"));
  EXPECT_EQ(file_state->contents, "1234567890");
  EXPECT_EQ(file->GetPosition(), 10);
  file_state->fail_write_after = -1;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  file_state->fail_write_after = 10;
  EXPECT_EQ(file->WriteLines(std::vector<const char*>{"1234567890"}), 0);
  EXPECT_EQ(file_state->contents, "1234567890");
  EXPECT_EQ(file->GetPosition(), 10);
  file_state->fail_write_after = -1;
  file.reset();

  file = file_system.OpenFile("test:/file", FileFlag::kWrite);
  file_state->fail_write_after = 15;
  EXPECT_EQ(
      file->WriteLines(std::vector<const char*>{"1234567890", "abcdefghij"}),
      1);
  EXPECT_EQ(file_state->contents, "1234567890\nabcd");
  EXPECT_EQ(file->GetPosition(), 15);
  file_state->fail_write_after = -1;
  file.reset();
}

struct ReadLineParam {
  const char* line_end;
  std::initializer_list<int64_t> lengths;
};

std::ostream& operator<<(std::ostream& out, const ReadLineParam& param) {
  return out << "{ line_end=" << absl::CEscape(param.line_end) << ", lengths={"
             << absl::StrJoin(param.lengths, ",") << "} }";
}

const ReadLineParam kReadLineParams[] = {
    {"\n", {0}},
    {"\r", {0}},
    {"\r\n", {0}},
    {"\n", {1, 10, 2, 9, 3, 8, 4, 7, 5, 6}},
    {"\r", {1, 10, 2, 9, 3, 8, 4, 7, 5, 6}},
    {"\r\n", {1, 10, 2, 9, 3, 8, 4, 7, 5, 6}},
    {"\n", {File::kLineBufferSize}},
    {"\r", {File::kLineBufferSize}},
    {"\r\n", {File::kLineBufferSize}},
    {"\n", {File::kLineBufferSize - 1}},
    {"\r", {File::kLineBufferSize - 1}},
    {"\r\n", {File::kLineBufferSize - 1}},
    {"\n", {File::kLineBufferSize - 2}},
    {"\r", {File::kLineBufferSize - 2}},
    {"\r\n", {File::kLineBufferSize - 2}},
    {"\n", {File::kLineBufferSize - 3}},
    {"\r", {File::kLineBufferSize - 3}},
    {"\r\n", {File::kLineBufferSize - 3}},
    {"\n", {File::kLineBufferSize * 2 + 1}},
};

class LineTest : public TestWithParam<ReadLineParam> {
 protected:
  LineTest() {
    file_system.Register(std::make_unique<TestProtocol>(&state), "test");
    state.paths["/file"] = TestProtocol::PathState::NewFile();
    file_state = state.paths["/file"].GetFile();
    line_end = GetParam().line_end;
    std::vector<int64_t> lengths(GetParam().lengths);
    for (size_t i = 0; i < 10; ++i) {
      file_lines.emplace_back(GenerateTestString(lengths[i % lengths.size()]));
    }
  }

  TestProtocol::State state;
  FileSystem file_system;
  TestProtocol::FileState* file_state;
  std::unique_ptr<File> file;
  std::string line_end;
  std::vector<std::string> file_lines;
  std::vector<std::string> lines;
  std::string line;
};

INSTANTIATE_TEST_SUITE_P(FileTest, LineTest, ValuesIn(kReadLineParams));

TEST_P(LineTest, ReadOne) {
  file_state->contents = file_lines.front();
  file = file_system.OpenFile("test:/file", FileFlag::kRead);

  if (file_state->contents.empty()) {
    line = "not empty";
    EXPECT_EQ(file->ReadLine(&line), !file_lines[0].empty());
    EXPECT_THAT(line, IsEmpty());
    EXPECT_EQ(file->GetPosition(), file_state->contents.size());

    EXPECT_EQ(file->ReadLines(1, &lines), 0);
    EXPECT_THAT(lines, IsEmpty());
    EXPECT_EQ(file->GetPosition(), file_state->contents.size());

    EXPECT_EQ(file->ReadRemainingLines(&lines), 0);
    EXPECT_THAT(lines, IsEmpty());
    EXPECT_EQ(file->GetPosition(), file_state->contents.size());
  } else {
    line = "not empty";
    EXPECT_EQ(file->ReadLine(&line), !file_lines[0].empty());
    EXPECT_EQ(line, file_lines.front());
    EXPECT_EQ(file->GetPosition(), file_state->contents.size());

    file->SeekBegin();
    EXPECT_EQ(file->ReadLines(2, &lines), 1);
    EXPECT_THAT(lines, ElementsAre(file_lines.front()));
    EXPECT_EQ(file->GetPosition(), file_state->contents.size());

    file->SeekBegin();
    EXPECT_EQ(file->ReadRemainingLines(&lines), 1);
    EXPECT_THAT(lines, ElementsAre(file_lines.front()));
    EXPECT_EQ(file->GetPosition(), file_state->contents.size());
  }

  file_state->contents.append(line_end);

  file->SeekBegin();
  line = "not empty";
  EXPECT_TRUE(file->ReadLine(&line));
  EXPECT_EQ(line, file_lines.front());
  EXPECT_EQ(file->GetPosition(), file_state->contents.size());

  file->SeekBegin();
  EXPECT_EQ(file->ReadLines(2, &lines), 1);
  EXPECT_THAT(lines, ElementsAre(file_lines.front()));
  EXPECT_EQ(file->GetPosition(), file_state->contents.size());

  file->SeekBegin();
  EXPECT_EQ(file->ReadRemainingLines(&lines), 1);
  EXPECT_THAT(lines, ElementsAre(file_lines.front()));
  EXPECT_EQ(file->GetPosition(), file_state->contents.size());
}

TEST_P(LineTest, ReadTwo) {
  if (file_lines.front().empty()) {
    return;
  }

  file_state->contents = absl::StrCat(file_lines[0], line_end, file_lines[1]);
  file = file_system.OpenFile("test:/file", FileFlag::kRead);

  line = "not empty";
  EXPECT_TRUE(file->ReadLine(&line));
  EXPECT_EQ(line, file_lines.front());
  EXPECT_EQ(file->GetPosition(),
            file_state->contents.size() - file_lines[1].size());

  file->SeekBegin();
  EXPECT_EQ(file->ReadLines(3, &lines), 2);
  EXPECT_THAT(lines, ElementsAre(file_lines[0], file_lines[1]));
  EXPECT_EQ(file->GetPosition(), file_state->contents.size());

  file->SeekBegin();
  EXPECT_EQ(file->ReadLines(1, &lines), 1);
  EXPECT_THAT(lines, ElementsAre(file_lines[0]));
  EXPECT_EQ(file->GetPosition(),
            file_state->contents.size() - file_lines[1].size());

  file->SeekBegin();
  EXPECT_EQ(file->ReadRemainingLines(&lines), 2);
  EXPECT_THAT(lines, ElementsAre(file_lines[0], file_lines[1]));
  EXPECT_EQ(file->GetPosition(), file_state->contents.size());
}

TEST_P(LineTest, ReadMiddle) {
  ASSERT_GT(file_lines.size(), 8);
  file_state->contents =
      absl::StrCat(absl::StrJoin(file_lines, line_end), line_end);
  int start_pos = 0;
  for (int i = 0; i < 4; ++i) {
    start_pos += static_cast<int>(file_lines[i].size() + line_end.size());
  }
  int end_pos = start_pos;
  for (int i = 4; i < 8; ++i) {
    end_pos += static_cast<int>(file_lines[i].size() + line_end.size());
  }
  file = file_system.OpenFile("test:/file", FileFlag::kRead);

  file->SeekTo(start_pos);
  EXPECT_EQ(file->ReadLines(4, &lines), 4);
  EXPECT_THAT(lines, ElementsAre(file_lines[4], file_lines[5], file_lines[6],
                                 file_lines[7]));
  EXPECT_EQ(file->GetPosition(), end_pos);

  file->SeekTo(start_pos);
  EXPECT_EQ(file->ReadRemainingLines(&lines), file_lines.size() - 4);
  EXPECT_THAT(lines,
              ElementsAreArray(file_lines.begin() + 4, file_lines.end()));
  EXPECT_EQ(file->GetPosition(), file_state->contents.size());
}

TEST_P(LineTest, WriteLine) {
  ASSERT_GE(file_lines.size(), 8);
  file = file_system.OpenFile("test:/file", FileFlag::kWrite);

  if (line_end == "\n") {
    EXPECT_TRUE(file->WriteLine(file_lines[0]));
  } else {
    EXPECT_TRUE(file->WriteLine(file_lines[0], line_end));
  }
  std::string expected_contents = absl::StrCat(file_lines[0], line_end);
  EXPECT_EQ(file_state->contents, expected_contents);

  EXPECT_TRUE(file->WriteLine(file_lines[1], line_end));
  expected_contents += absl::StrCat(file_lines[1], line_end);
  EXPECT_EQ(file_state->contents, expected_contents);

  std::vector<std::string> string_lines = {file_lines[2], file_lines[3]};
  if (line_end == "\n") {
    EXPECT_EQ(file->WriteLines(string_lines), 2);
  } else {
    EXPECT_EQ(file->WriteLines(string_lines, line_end), 2);
  }
  expected_contents +=
      absl::StrCat(file_lines[2], line_end, file_lines[3], line_end);
  EXPECT_EQ(file_state->contents, expected_contents);

  std::vector<std::string_view> view_lines = {file_lines[4], file_lines[5]};
  EXPECT_EQ(file->WriteLines(view_lines, line_end), 2);
  expected_contents +=
      absl::StrCat(file_lines[4], line_end, file_lines[5], line_end);
  EXPECT_EQ(file_state->contents, expected_contents);

  std::vector<const char*> ptr_lines = {file_lines[6].c_str(),
                                        file_lines[7].c_str()};
  EXPECT_EQ(file->WriteLines(ptr_lines, line_end), 2);
  expected_contents +=
      absl::StrCat(file_lines[6], line_end, file_lines[7], line_end);
  EXPECT_EQ(file_state->contents, expected_contents);

  std::set<std::string_view> set_lines;
  set_lines.insert(file_lines.begin(), file_lines.end());
  file->SeekBegin();
  file_state->contents.clear();
  EXPECT_EQ(file->WriteLines(set_lines, line_end), set_lines.size());
  int64_t size = 0;
  for (const auto& line : set_lines) {
    size += line.size() + line_end.size();
  }
  EXPECT_EQ(file->GetPosition(), size);
}

}  // namespace
}  // namespace gb
