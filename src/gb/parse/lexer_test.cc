// Copyright (c) 2024 John Pursey
//
// Use of this source code is governed by an MIT-style License that can be found
// in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "gb/parse/lexer.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace gb {
namespace {

TEST(LexerTest, DefaultConfigIsValid) {
  LexerConfig config;
  std::string error;
  auto lexer = Lexer::Create(config, &error);
  EXPECT_NE(lexer, nullptr);
  EXPECT_EQ(error, "");
}

TEST(LexerTest, AddContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("test content");
  EXPECT_NE(content, kNoLexerContent);
  EXPECT_EQ(lexer->GetFileContentId(""), kNoLexerContent);
  EXPECT_EQ(lexer->GetContentFilename(content), "");
  EXPECT_EQ(lexer->GetContentText(content), "test content");
  EXPECT_EQ(lexer->GetLineCount(content), 1);
}

TEST(LexerTest, AddFileContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddFileContent("test.txt", "test content");
  EXPECT_NE(content, kNoLexerContent);
  EXPECT_EQ(lexer->GetFileContentId("test.txt"), content);
  EXPECT_EQ(lexer->GetContentFilename(content), "test.txt");
  EXPECT_THAT(lexer->GetContentText(content), "test content");
  EXPECT_EQ(lexer->GetLineCount(content), 1);
}

TEST(LexerTest, GetInvalidContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("test content");
  EXPECT_EQ(lexer->GetContentFilename(kNoLexerContent), "");
  EXPECT_EQ(lexer->GetContentText(kNoLexerContent), "");
  EXPECT_EQ(lexer->GetLineCount(kNoLexerContent), 0);
  EXPECT_EQ(lexer->GetLineText(kNoLexerContent, 0), "");
  EXPECT_EQ(lexer->GetContentFilename(content + 1), "");
  EXPECT_EQ(lexer->GetContentText(content + 1), "");
  EXPECT_EQ(lexer->GetLineCount(content + 1), 0);
  EXPECT_EQ(lexer->GetLineText(content + 1, 0), "");
}

TEST(LexerTest, GetInvalidLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("test content");
  EXPECT_EQ(lexer->GetLineText(content, -1), "");
  EXPECT_EQ(lexer->GetLineText(content, 1), "");
}

TEST(LexerTest, EmptyContentHasOneLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("");
  EXPECT_EQ(lexer->GetLineCount(content), 1);
  EXPECT_EQ(lexer->GetLineText(content, 0), "");
}

TEST(LexerTest, ContentWithNoTrailingNewlineHasOneLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2");
  EXPECT_EQ(lexer->GetLineCount(content), 2);
  EXPECT_EQ(lexer->GetLineText(content, 0), "line 1");
  EXPECT_EQ(lexer->GetLineText(content, 1), "line 2");
}

TEST(LexerTest, ContentWithTrailingNewlineHasOneLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_EQ(lexer->GetLineCount(content), 2);
  EXPECT_EQ(lexer->GetLineText(content, 0), "line 1");
  EXPECT_EQ(lexer->GetLineText(content, 1), "line 2");
}

TEST(LexerTest, ContentWithEmptyLines) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddContent("\nline 1\n\nline 2\n\n\nline 3\n\n");
  EXPECT_EQ(lexer->GetLineCount(content), 8);
  EXPECT_EQ(lexer->GetLineText(content, 0), "");
  EXPECT_EQ(lexer->GetLineText(content, 1), "line 1");
  EXPECT_EQ(lexer->GetLineText(content, 2), "");
  EXPECT_EQ(lexer->GetLineText(content, 3), "line 2");
  EXPECT_EQ(lexer->GetLineText(content, 4), "");
  EXPECT_EQ(lexer->GetLineText(content, 5), "");
  EXPECT_EQ(lexer->GetLineText(content, 6), "line 3");
  EXPECT_EQ(lexer->GetLineText(content, 7), "");
}

TEST(LexerTest, GetLineLocation) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetLineLocation(content, 0),
            (LexerLocation{.id = content, .line = 0, .column = 0}));
  EXPECT_EQ(lexer->GetLineLocation(content, 1),
            (LexerLocation{.id = content, .line = 1, .column = 0}));
  EXPECT_EQ(lexer->GetLineLocation(content, 2),
            (LexerLocation{.id = content, .line = 2, .column = 0}));
}

TEST(LexerTest, GetLineLocationWithFilename) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content =
      lexer->AddFileContent("test.txt", "line 1\nline 2\nline 3\n");
  EXPECT_EQ(
      lexer->GetLineLocation(content, 0),
      (LexerLocation{
          .id = content, .filename = "test.txt", .line = 0, .column = 0}));
  EXPECT_EQ(
      lexer->GetLineLocation(content, 1),
      (LexerLocation{
          .id = content, .filename = "test.txt", .line = 1, .column = 0}));
  EXPECT_EQ(
      lexer->GetLineLocation(content, 2),
      (LexerLocation{
          .id = content, .filename = "test.txt", .line = 2, .column = 0}));
}

TEST(LexerTest, GetLineLocationForInvalidLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetLineLocation(content, -2),
            (LexerLocation{.id = kNoLexerContent}));
  EXPECT_EQ(lexer->GetLineLocation(content, 3),
            (LexerLocation{.id = kNoLexerContent}));
}

TEST(LexerTest, GetLineLocationForInvalidContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetLineLocation(kNoLexerContent, 0),
            (LexerLocation{.id = kNoLexerContent}));
  EXPECT_EQ(lexer->GetLineLocation(content + 1, 0),
            (LexerLocation{.id = kNoLexerContent}));
}

TEST(LexerTest, NextLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->GetCurrentLine(content), 1);
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_EQ(lexer->GetCurrentLine(content), 2);
  EXPECT_EQ(lexer->NextLine(content), "line 3");
  EXPECT_EQ(lexer->GetCurrentLine(content), 3);
  EXPECT_EQ(lexer->NextLine(content), "");
  EXPECT_EQ(lexer->GetCurrentLine(content), 3);
  EXPECT_EQ(lexer->NextLine(content), "");
}

TEST(LexerTest, NextLineForInvalidContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\nline 3\n");
  EXPECT_EQ(lexer->GetCurrentLine(kNoLexerContent), -1);
  EXPECT_EQ(lexer->NextLine(kNoLexerContent), "");
  EXPECT_EQ(lexer->GetCurrentLine(content + 1), -1);
  EXPECT_EQ(lexer->NextLine(content + 1), "");
}

TEST(LexerTest, RewindLine) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_EQ(lexer->GetCurrentLine(content), 2);
  EXPECT_TRUE(lexer->RewindLine(content));
  EXPECT_EQ(lexer->GetCurrentLine(content), 1);
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_TRUE(lexer->RewindLine(content));
  EXPECT_TRUE(lexer->RewindLine(content));
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
  EXPECT_FALSE(lexer->RewindLine(content));
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
}

TEST(LexerTest, RewindLineForInvalidContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_FALSE(lexer->RewindLine(kNoLexerContent));
  EXPECT_FALSE(lexer->RewindLine(content + 1));
}

TEST(LexerTest, RewindContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->NextLine(content), "line 2");
  EXPECT_EQ(lexer->GetCurrentLine(content), 2);
  lexer->RewindContent(content);
  EXPECT_EQ(lexer->GetCurrentLine(content), 0);
  EXPECT_EQ(lexer->NextLine(content), "line 1");
  EXPECT_EQ(lexer->GetCurrentLine(content), 1);
}

TEST(LexerTest, RewindContentForInvalidContent) {
  auto lexer = Lexer::Create({});
  ASSERT_NE(lexer, nullptr);
  LexerContentId content = lexer->AddContent("line 1\nline 2\n");
  lexer->RewindContent(kNoLexerContent);
  lexer->RewindContent(content + 1);
}

}  // namespace
}  // namespace gb