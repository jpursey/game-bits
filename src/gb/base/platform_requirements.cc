// Copyright (c) 2020 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

// C++ supports an incredibly broad set of platforms, including esoteric
// character encodings, integer sizes, and such. The Game Bits libraries do not
// support platforms with more exotic features. This file will fail to compile
// on any platform that does not conform to the more restrictive requirements of
// Game Bits.

#include "gb/base/platform_requirements.h"

#include <limits>

#include "stdint.h"

// It is required that all fixed sized integers up to 64-bit are supported, and
// all integer types are packed into the expected minimum number of bytes.
static_assert(sizeof(int8_t) == 1, "int8_t is required");
static_assert(sizeof(int16_t) == 2, "int16_t is required");
static_assert(sizeof(int32_t) == 4, "int32_t is required");
static_assert(sizeof(int64_t) == 8, "int64_t is required");
static_assert(sizeof(uint8_t) == 1, "uint8_t is required");
static_assert(sizeof(uint16_t) == 2, "uint16_t is required");
static_assert(sizeof(uint32_t) == 4, "uint32_t is required");
static_assert(sizeof(uint64_t) == 8, "uint64_t is required");

// It is required that char16_t be exactly 2 bytes, as it is used for
// representing UTF-16 strings that may be serialized to/from disk as well as in
// memory (byte ordering is not assumed, however).
static_assert(sizeof(char16_t) == 2, "char16_t must be 2 bytes");

// Float types must be of a defined size in bytes.
static_assert(sizeof(float) == 4, "float must be 4 bytes");
static_assert(sizeof(double) == 8, "double must be 8 bytes");

// Alignment requirements must not be greater than what is expected. Where
// alignment matters for binary compatibility (for instance serialization), Game
// Bits ensures these minimum alignments for the associated types.
static_assert(alignof(int8_t) <= 1, "int8_t alignment must be <= its size");
static_assert(alignof(int16_t) <= 2, "int16_t alignment must be <= its size");
static_assert(alignof(int32_t) <= 4, "int32_t alignment must be <= its size");
static_assert(alignof(int64_t) <= 8, "int64_t alignment must be <= its size");
static_assert(alignof(uint8_t) <= 1, "uint8_t alignment must be <= its size");
static_assert(alignof(uint16_t) <= 2, "uint16_t alignment must be <= its size");
static_assert(alignof(uint32_t) <= 4, "uint32_t alignment must be <= its size");
static_assert(alignof(uint64_t) <= 8, "uint64_t alignment must be <= its size");
static_assert(alignof(float) <= 4, "float alignment must be <= its size");
static_assert(alignof(double) <= 8, "double alignment must be <= its size");

// C++ only guarantees that the "int" type is in the range -32,768...32,767.
// "int" is used extensively in Game Bits for general indexing, which is assumed
// to be more than a 16-bit signed int could support.
static_assert(
    static_cast<int64_t>(std::numeric_limits<int>::min()) <=
            static_cast<int64_t>(std::numeric_limits<int32_t>::min()) &&
        static_cast<int64_t>(std::numeric_limits<int>::max()) >=
            static_cast<int64_t>(std::numeric_limits<int32_t>::max()),
    "int must support the full range of 32-bit integer values.");

// The above enforce that char is a byte (as it must have a size of 1, and
// int8_t exists and is of size 1). However, there is no guarantee in C++ that
// the encoding is ASCII compatible. This enforces that, which allows for
// simpler locale-agnostic ASCII and UTF-8 neutral processing of the 'char' type
// (and without requiring C++20 char8_t everwhere), and further allow use of the
// absl/strings/ascii.h. Since all (or nearly all?) gaming platforms support
// ASCII, we make the assertion here.
static_assert('\t' == 0x09, "ASCII encoding required.");
static_assert('\n' == 0x0A, "ASCII encoding required.");
static_assert('\r' == 0x0D, "ASCII encoding required.");
static_assert(' ' == 0x20, "ASCII encoding required.");
static_assert('!' == 0x21, "ASCII encoding required.");
static_assert('"' == 0x22, "ASCII encoding required.");
static_assert('#' == 0x23, "ASCII encoding required.");
static_assert('$' == 0x24, "ASCII encoding required.");
static_assert('%' == 0x25, "ASCII encoding required.");
static_assert('&' == 0x26, "ASCII encoding required.");
static_assert('\'' == 0x27, "ASCII encoding required.");
static_assert('(' == 0x28, "ASCII encoding required.");
static_assert(')' == 0x29, "ASCII encoding required.");
static_assert('*' == 0x2A, "ASCII encoding required.");
static_assert('+' == 0x2B, "ASCII encoding required.");
static_assert(',' == 0x2C, "ASCII encoding required.");
static_assert('-' == 0x2D, "ASCII encoding required.");
static_assert('.' == 0x2E, "ASCII encoding required.");
static_assert('/' == 0x2F, "ASCII encoding required.");
static_assert('0' == 0x30, "ASCII encoding required.");
static_assert('1' == 0x31, "ASCII encoding required.");
static_assert('2' == 0x32, "ASCII encoding required.");
static_assert('3' == 0x33, "ASCII encoding required.");
static_assert('4' == 0x34, "ASCII encoding required.");
static_assert('5' == 0x35, "ASCII encoding required.");
static_assert('6' == 0x36, "ASCII encoding required.");
static_assert('7' == 0x37, "ASCII encoding required.");
static_assert('8' == 0x38, "ASCII encoding required.");
static_assert('9' == 0x39, "ASCII encoding required.");
static_assert(':' == 0x3A, "ASCII encoding required.");
static_assert(';' == 0x3B, "ASCII encoding required.");
static_assert('<' == 0x3C, "ASCII encoding required.");
static_assert('=' == 0x3D, "ASCII encoding required.");
static_assert('>' == 0x3E, "ASCII encoding required.");
static_assert('?' == 0x3F, "ASCII encoding required.");
static_assert('@' == 0x40, "ASCII encoding required.");
static_assert('A' == 0x41, "ASCII encoding required.");
static_assert('B' == 0x42, "ASCII encoding required.");
static_assert('C' == 0x43, "ASCII encoding required.");
static_assert('D' == 0x44, "ASCII encoding required.");
static_assert('E' == 0x45, "ASCII encoding required.");
static_assert('F' == 0x46, "ASCII encoding required.");
static_assert('G' == 0x47, "ASCII encoding required.");
static_assert('H' == 0x48, "ASCII encoding required.");
static_assert('I' == 0x49, "ASCII encoding required.");
static_assert('J' == 0x4A, "ASCII encoding required.");
static_assert('K' == 0x4B, "ASCII encoding required.");
static_assert('L' == 0x4C, "ASCII encoding required.");
static_assert('M' == 0x4D, "ASCII encoding required.");
static_assert('N' == 0x4E, "ASCII encoding required.");
static_assert('O' == 0x4F, "ASCII encoding required.");
static_assert('P' == 0x50, "ASCII encoding required.");
static_assert('Q' == 0x51, "ASCII encoding required.");
static_assert('R' == 0x52, "ASCII encoding required.");
static_assert('S' == 0x53, "ASCII encoding required.");
static_assert('T' == 0x54, "ASCII encoding required.");
static_assert('U' == 0x55, "ASCII encoding required.");
static_assert('V' == 0x56, "ASCII encoding required.");
static_assert('W' == 0x57, "ASCII encoding required.");
static_assert('X' == 0x58, "ASCII encoding required.");
static_assert('Y' == 0x59, "ASCII encoding required.");
static_assert('Z' == 0x5A, "ASCII encoding required.");
static_assert('[' == 0x5B, "ASCII encoding required.");
static_assert('\\' == 0x5C, "ASCII encoding required.");
static_assert(']' == 0x5D, "ASCII encoding required.");
static_assert('^' == 0x5E, "ASCII encoding required.");
static_assert('_' == 0x5F, "ASCII encoding required.");
static_assert('`' == 0x60, "ASCII encoding required.");
static_assert('a' == 0x61, "ASCII encoding required.");
static_assert('b' == 0x62, "ASCII encoding required.");
static_assert('c' == 0x63, "ASCII encoding required.");
static_assert('d' == 0x64, "ASCII encoding required.");
static_assert('e' == 0x65, "ASCII encoding required.");
static_assert('f' == 0x66, "ASCII encoding required.");
static_assert('g' == 0x67, "ASCII encoding required.");
static_assert('h' == 0x68, "ASCII encoding required.");
static_assert('i' == 0x69, "ASCII encoding required.");
static_assert('j' == 0x6A, "ASCII encoding required.");
static_assert('k' == 0x6B, "ASCII encoding required.");
static_assert('l' == 0x6C, "ASCII encoding required.");
static_assert('m' == 0x6D, "ASCII encoding required.");
static_assert('n' == 0x6E, "ASCII encoding required.");
static_assert('o' == 0x6F, "ASCII encoding required.");
static_assert('p' == 0x70, "ASCII encoding required.");
static_assert('q' == 0x71, "ASCII encoding required.");
static_assert('r' == 0x72, "ASCII encoding required.");
static_assert('s' == 0x73, "ASCII encoding required.");
static_assert('t' == 0x74, "ASCII encoding required.");
static_assert('u' == 0x75, "ASCII encoding required.");
static_assert('v' == 0x76, "ASCII encoding required.");
static_assert('w' == 0x77, "ASCII encoding required.");
static_assert('x' == 0x78, "ASCII encoding required.");
static_assert('y' == 0x79, "ASCII encoding required.");
static_assert('z' == 0x7A, "ASCII encoding required.");
static_assert('{' == 0x7B, "ASCII encoding required.");
static_assert('|' == 0x7C, "ASCII encoding required.");
static_assert('}' == 0x7D, "ASCII encoding required.");
static_assert('~' == 0x7E, "ASCII encoding required.");

bool IsPlatformSupported() {
  // Always true, as there are only compile time dependencies.
  return true;
}
