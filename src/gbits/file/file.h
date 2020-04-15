#ifndef GBITS_BASE_FILE_FILE_H_
#define GBITS_BASE_FILE_FILE_H_

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "gbits/file/file_types.h"

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

  // Writes a buffer of trivially-copiable values to the file.
  //
  // Returns the number of values actually written. If the number of bytes
  // written is less than 'count', call IsValid() to determine whether the
  // failure is recoverable. If IsValid() returns true in this case, the file is
  // at max capacity.
  template <typename Type, typename Count = size_t>
  Count Write(const Type* buffer, Count count = 1);

  template <typename Count>
  Count Write(const void* buffer, Count count);

  std::string_view::size_type Write(std::string_view text);

  template <typename Type>
  typename std::vector<Type>::size_type Write(const std::vector<Type>& buffer);

  // Reads a buffer of trivially-copiable values from the file.
  //
  // Returns the number of values actually read. If the number of bytes read is
  // less than 'count', it usually means the end-of-file was reached. If there
  // is another error, IsValid() will return false.
  template <typename Type, typename Count = size_t>
  Count Read(Type* buffer, Count count = 1);

  template <typename Count>
  Count Read(void* buffer, Count count);

  // Reads the remaining bytes in the file to the provided string.
  //
  // This returns true only if all remaining bytes were successfully read. Even
  // on failure, any bytes actually read will be put in the provided buffer. On
  // failure, IsValid() will indicate whether the error is recoverable or not.
  bool ReadRemaining(std::string* buffer);

  // Reads the remaining bytes in the file to the provided buffer.
  //
  // This returns true only if all remaining bytes were successfully read. Even
  // on failure, any values actually read will be put in the provided buffer. If
  // the number of bytes remaining is not a multiple of sizeof(Type), this will
  // return false (as not all bytes can be read). On failure, IsValid() will
  // indicate whether the error is recoverable or not.
  template <typename Type>
  bool ReadRemaining(std::vector<Type>* buffer);

  // Reads a line of ASCII or UTF-8 text from the file into the provided string.
  //
  // Lines in the file are terminated by "\r", "\n", "\r\n", or end-of-file. No
  // line ending will be in the returned string. If end-of-file occurs
  // immediately after a line ending, it is not considered an additional blank
  // line.
  //
  // This will return true if a line was successfully read.
  bool ReadLine(std::string* line);

  // Like ReadLine above, except returns the string by value.
  std::string ReadLine() {
    std::string line;
    ReadLine(&line);
    return line;
  }

  // Reads up to 'count' lines of ASCII or UTF-8 text from the file.
  //
  // Lines in the file are terminated by "\r", "\n", "\r\n", or end-of-file. No
  // line endings will be in the returned strings. If end-of-file occurs
  // immediately after a line ending, it is not considered an additional blank
  // line.
  //
  // This returns the number of lines actually read.
  int64_t ReadLines(int64_t count, std::vector<std::string>* lines);

  // Like ReadLines above, except returns the lines by value.
  std::vector<std::string> ReadLines(int64_t count) {
    std::vector<std::string> lines;
    ReadLines(count, &lines);
    return lines;
  }

  // Reads the remaining lines of ASCII or UTF-8 text from the file.
  //
  // Lines in the file are terminated by "\r", "\n", "\r\n", or end-of-file. No
  // line endings will be in the returned strings. If end-of-file occurs
  // immediately after a line ending, it is not considered an additional blank
  // line.
  int64_t ReadRemainingLines(std::vector<std::string>* lines);

  // Like ReadRemainingLines, except returns the lines by value.
  std::vector<std::string> ReadRemainingLines() {
    std::vector<std::string> lines;
    ReadRemainingLines(&lines);
    return lines;
  }

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
  typename Container::size_type WriteLines(const Container& lines,
                                           std::string_view line_end = "\n");

  // Number of bytes that are buffered when reading lines from the file.
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

template <typename Type, typename Count>
inline Count File::Write(const Type* buffer, Count count) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Buffer cannot be written as raw bytes");
  static_assert(!std::is_same_v<Count, bool> && std::is_integral_v<Count>,
                "Count is not an integer type");
  const int64_t type_size = static_cast<int64_t>(sizeof(Type));
  return static_cast<Count>(
      DoWrite(buffer, static_cast<int64_t>(count) * type_size) / type_size);
}

template <typename Count>
inline Count File::Write(const void* buffer, Count count) {
  return Write(static_cast<const uint8_t*>(buffer), count);
}

inline std::string_view::size_type File::Write(std::string_view text) {
  return Write(text.data(), text.size());
}

template <typename Type>
inline typename std::vector<Type>::size_type File::Write(
    const std::vector<Type>& buffer) {
  return Write(buffer.data(), buffer.size());
}

template <typename Type, typename Count>
inline Count File::Read(Type* buffer, Count count) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Buffer cannot be written into as raw bytes");
  static_assert(!std::is_same_v<Count, bool> && std::is_integral_v<Count>,
                "Count is not an integer type");
  const int64_t type_size = static_cast<int64_t>(sizeof(Type));
  return static_cast<Count>(
      DoRead(buffer, static_cast<int64_t>(count) * type_size) / type_size);
}

template <typename Count>
inline Count File::Read(void* buffer, Count count) {
  return Read(static_cast<uint8_t*>(buffer), count);
}

template <typename Type>
bool File::ReadRemaining(std::vector<Type>* buffer) {
  static_assert(std::is_trivially_copyable_v<Type>,
                "Buffer cannot be written into as raw bytes");
  if (!flags_.IsSet(FileFlag::kRead)) {
    buffer->resize(0);
    return false;
  }
  int64_t remaining = CalculateRemaining();
  if (remaining < 0) {
    buffer->resize(0);
    return false;
  }
  const int64_t type_size = static_cast<int64_t>(sizeof(Type));
  const int64_t count = remaining / type_size;
  remaining = count * type_size;
  buffer->resize(static_cast<std::vector<Type>::size_type>(count));
  if (remaining == 0) {
    return true;
  }
  const int64_t bytes_read = DoRead(buffer->data(), remaining);
  buffer->resize(bytes_read / type_size);
  return bytes_read == remaining;
}

template <typename Container>
typename Container::size_type File::WriteLines(const Container& lines,
                                               std::string_view line_end) {
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

#endif  // GBITS_BASE_FILE_FILE_H_
