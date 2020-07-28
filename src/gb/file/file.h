// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef GB_BASE_FILE_FILE_H_
#define GB_BASE_FILE_FILE_H_

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gb/file/file_types.h"

namespace gb {

// This class represents an open file returned from a FileSystem.
//
// This class is thread-compatible.
class File final {
 public:
  File(const File&) = delete;
  File& operator=(const File&) = delete;

  // Closes the file.
  ~File();

  //---------------------------------------------------------------------------
  // General attributes

  // Returns the flags this file was opened with.
  FileFlags GetFlags() const { return flags_; }

  // Returns true if the file is still valid.
  //
  // Files are always valid when they are first opened, but may experience
  // errors as operations are performed on the file. If an error occurs, the
  // file will become invalid and all further operations will fail. In these
  // case, the file will need to be closed and a new one acquired from the file
  // system.
  bool IsValid() const { return position_ >= 0; }

  //---------------------------------------------------------------------------
  // Position in the file.

  // Returns the current position in the file.
  //
  // If the file is invalid, this will return -1.
  int64_t GetPosition() const { return position_; }

  // Seek functions seek to another position within the file.
  //
  // On success, these functions return the actual position of the file (which
  // may be different, when seeking past the end of the file). On error, these
  // functions return -1.
  int64_t SeekBegin() { return SeekTo(0); }
  int64_t SeekEnd();
  int64_t SeekTo(int64_t position);
  int64_t SeekBy(int64_t delta) { return SeekTo(position_ + delta); }

  //---------------------------------------------------------------------------
  // Raw buffer read/write

  // Reads the requested number of bytes into a pre-allocated untyped buffer.
  //
  // Returns the number of bytes actually read. If the number of bytes read is
  // less than 'count', it usually means end-of-file was reached.
  int64_t Read(void* buffer, int64_t count);

  // Writes an untyped buffer of bytes into the file.
  //
  // Returns the number of bytes actually written.
  int64_t Write(const void* buffer, int64_t count);

  //---------------------------------------------------------------------------
  // Typed buffer read/write

  // Reads the requested number of trivially-copyable values from the file.
  //
  // Returns the number of values actually read. If the number of values read is
  // less than 'count', it usually means the end-of-file was reached.
  template <typename Type>
  int64_t Read(Type* buffer, int64_t count = 1);
  template <typename Type>
  int64_t Read(std::vector<Type>* buffer, int64_t count);

  // Reads the requested number of trivially-copyable values from the file.
  //
  // Returns the values actually read. If the number of values read is
  // less than 'count', it usually means the end-of-file was reached.
  template <typename Type>
  std::vector<Type> Read(int64_t count);

  // Reads the remaining trivially-copyable values from the file.
  //
  // Only whole values are read, so if the file contains a partial value at the
  // end, it will not be read and the position will not be at the end of the
  // file.
  template <typename Type>
  void ReadRemaining(std::vector<Type>* buffer);

  // Reads the remaining trivially-copyable values from the file.
  //
  // Only whole values are read, so if the file contains a partial value at the
  // end, it will not be read and the position will not be at the end of the
  // file.
  //
  // Returns the buffer of values successfully read.
  template <typename Type>
  std::vector<Type> ReadRemaining();

  // Writes a buffer of trivially-copyable values to the file.
  //
  // Returns the number of values actually written.
  template <typename Type>
  int64_t Write(const Type* buffer, int64_t count = 1);
  template <typename Type>
  int64_t Write(const std::vector<Type>& buffer);

  //---------------------------------------------------------------------------
  // String read/write

  // Reads a string of test of the specified max length from the file.
  //
  // This reads raw bytes into the string and does not perform line ending
  // conversion. Use ReadLine to read lines.
  //
  // This returns the number of bytes actually read. If the number of bytes
  // read is less than 'count', it usually means the end-of-file was reached.
  int64_t ReadString(std::string* buffer, int64_t count);

  // Reads a string of test of the specified max length from the file.
  //
  // This reads raw bytes into the string and does not perform line ending
  // conversion. Use ReadLine to read lines.
  //
  // This returns the string actually read. If the size of the string is less
  // than 'count', it usually means the end-of-file was reached.
  std::string ReadString(int64_t count);

  // Reads the remaining bytes in the file to the provided string.
  //
  // This reads raw bytes into the string and does not perform line ending
  // conversion. Use ReadRemainingLines to read lines.
  void ReadRemainingString(std::string* buffer);

  // Reads the remaining bytes of the file into a string and returns it.
  //
  // This reads raw bytes into the string and does not perform line ending
  // conversion. Use ReadRemainingLines to read lines.
  std::string ReadRemainingString();

  // Writes a string of text to the file.
  //
  // Returns the number of values actually written.
  int64_t WriteString(std::string_view text);

  //---------------------------------------------------------------------------
  // Line read/write

  // Lines in the file are terminated by "\r", "\n", "\r\n", or end-of-file. No
  // line ending will be in the returned strings read from the file. If
  // end-of-file occurs immediately after a line ending, it is not considered an
  // additional blank line.
  //
  // These functions do not validate that the file is valid ASCII or UTF-8, and
  // will read all byte values, only taking into account line endings. This is
  // meaningless for binary files, and will produce invalid results on other
  // unicode encodings.
  //
  // These functions also do not account for the 0xEF 0xBB 0xBF byte order mark
  // allowed in UTF-8 files. If they exist, they will appear as the first three
  // bytes of the first line read from the file.

  // Reads a line of ASCII or UTF-8 text from the file into the provided string.
  //
  // This will return true if a line was successfully read. If this returns
  // false, then no bytes were read into the string, and most likely the
  // end-of-file was reached.
  bool ReadLine(std::string* line);

  // Reads a line of ASCII or UTF-8 text from the file and returns it.
  std::string ReadLine();

  // Reads up to 'count' lines of ASCII or UTF-8 text from the file.
  //
  // This returns the number of lines actually read.
  int64_t ReadLines(int64_t count, std::vector<std::string>* lines);

  // Reads up to 'count' lines of ASCII or UTF-8 text from the file.
  //
  // Returns the lines read.
  std::vector<std::string> ReadLines(int64_t count);

  // Reads the remaining lines of ASCII or UTF-8 text from the file.
  //
  // This returns the number of lines read.
  int64_t ReadRemainingLines(std::vector<std::string>* lines);

  // Reads the remaining lines of ASCII or UTF-8 text from the file.
  //
  // This returns the lines read.
  std::vector<std::string> ReadRemainingLines();

  // Writes a line to the file with the specified line ending.
  //
  // Returns true if the line was written completely. On failure, it is possible
  // only part of the line was written.
  bool WriteLine(std::string_view line, std::string_view line_end = "\n");

  // Writes multiple lines to the file with the specified line ending.
  //
  // Returns the number of complete lines written. On failure, it is possible
  // only some part of the lines were written, or even a partial line was
  // written.
  template <typename Container>
  int64_t WriteLines(const Container& lines, std::string_view line_end = "\n");

  // Number of bytes that are buffered when reading lines from the file. This is
  // an internal constant made public for unit tests only. It is not meaningful
  // for general use.
  inline static constexpr int64_t kLineBufferSize = 256;

 private:
  friend class FileSystem;

  struct ReadLineState {
    ReadLineState() = default;
    std::string buffer;
    std::string::size_type pos = 0;
  };

  File(std::unique_ptr<RawFile> file, FileFlags flags);

  int64_t CalculateRemaining();
  int64_t DoWrite(const void* buffer, int64_t size);
  int64_t DoRead(void* buffer, int64_t size);
  bool DoReadLine(ReadLineState* state, std::string* line);

  const std::unique_ptr<RawFile> file_;
  const FileFlags flags_;
  int64_t position_ = 0;
};

inline int64_t File::Read(void* buffer, int64_t size) {
  return DoRead(buffer, size);
}

inline int64_t File::Write(const void* buffer, int64_t size) {
  return DoWrite(buffer, size);
}

template <typename Type>
inline int64_t File::Read(Type* buffer, int64_t count) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Buffer cannot be written into as raw bytes");
  const int64_t type_size = static_cast<int64_t>(sizeof(Type));
  return DoRead(buffer, static_cast<int64_t>(count) * type_size) / type_size;
}

template <typename Type>
inline int64_t File::Read(std::vector<Type>* buffer, int64_t count) {
  buffer->resize(count);
  buffer->resize(Read(buffer->data(), count));
  return static_cast<int64_t>(buffer->size());
}

template <typename Type>
inline std::vector<Type> File::Read(int64_t count) {
  std::vector<Type> buffer;
  Read(&buffer, count);
  return buffer;
}

template <typename Type>
void File::ReadRemaining(std::vector<Type>* buffer) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Buffer cannot be written into as raw bytes");
  if (!flags_.IsSet(FileFlag::kRead)) {
    buffer->resize(0);
    return;
  }
  int64_t remaining = CalculateRemaining();
  if (remaining < 0) {
    buffer->resize(0);
    return;
  }
  const int64_t type_size = static_cast<int64_t>(sizeof(Type));
  const int64_t count = remaining / type_size;
  remaining = count * type_size;
  buffer->resize(static_cast<std::vector<Type>::size_type>(count));
  if (remaining == 0) {
    return;
  }
  const int64_t bytes_read = DoRead(buffer->data(), remaining);
  buffer->resize(bytes_read / type_size);
  return;
}

template <typename Type>
inline std::vector<Type> File::ReadRemaining() {
  std::vector<Type> buffer;
  ReadRemaining(&buffer);
  return buffer;
}

template <typename Type>
inline int64_t File::Write(const Type* buffer, int64_t count) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Buffer cannot be written as raw bytes");
  const int64_t type_size = static_cast<int64_t>(sizeof(Type));
  return static_cast<int64_t>(
      DoWrite(buffer, static_cast<int64_t>(count) * type_size) / type_size);
}

template <typename Type>
inline int64_t File::Write(const std::vector<Type>& buffer) {
  return Write(buffer.data(), static_cast<int64_t>(buffer.size()));
}

inline int64_t File::ReadString(std::string* buffer, int64_t count) {
  buffer->resize(count);
  buffer->resize(Read(&(*buffer)[0], count));
  return static_cast<int64_t>(buffer->size());
}

inline std::string File::ReadString(int64_t count) {
  std::string buffer;
  ReadString(&buffer, count);
  return buffer;
}

inline std::string File::ReadRemainingString() {
  std::string buffer;
  ReadRemainingString(&buffer);
  return buffer;
}

inline int64_t File::WriteString(std::string_view text) {
  return static_cast<int64_t>(Write(text.data(), text.size()));
}

inline std::string File::ReadLine() {
  std::string line;
  ReadLine(&line);
  return line;
}

inline std::vector<std::string> File::ReadLines(int64_t count) {
  std::vector<std::string> lines;
  ReadLines(count, &lines);
  return lines;
}

inline std::vector<std::string> File::ReadRemainingLines() {
  std::vector<std::string> lines;
  ReadRemainingLines(&lines);
  return lines;
}

template <typename Container>
int64_t File::WriteLines(const Container& lines, std::string_view line_end) {
  Container::size_type count = 0;
  for (const auto& line : lines) {
    if (!WriteLine(line, line_end)) {
      break;
    }
    ++count;
  }
  return count;
}

}  // namespace gb

#endif  // GB_BASE_FILE_FILE_H_
