#include "parse/lexer.h"

#include <gtest/gtest.h>

using namespace promql;

TEST(LexerTest, HandleEmptyStream)
{
    Lexer lexer("");

    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleOperators)
{
    Lexer lexer("+");

    EXPECT_EQ(Token::ADD, lexer.get_token());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleEqualAndAssignment)
{
    Lexer lexer("= ==");

    EXPECT_EQ(Token::ASSIGN, lexer.get_token());
    EXPECT_EQ(Token::EQL, lexer.get_token());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleStringLiteral)
{
    Lexer lexer("'hello\\n'");

    EXPECT_EQ(Token::STRING, lexer.get_token());
    EXPECT_EQ("hello\n", lexer.get_last_string());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleRawStringLiteral)
{
    Lexer lexer("`hello\\n`");

    EXPECT_EQ(Token::STRING, lexer.get_token());
    EXPECT_EQ("hello\\n", lexer.get_last_string());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleNumberLiteral)
{
    Lexer lexer("12345.678e+10 .1234");

    EXPECT_EQ(Token::NUMBER, lexer.get_token());
    EXPECT_EQ("12345.678e+10", lexer.get_last_strnum());
    EXPECT_EQ(Token::NUMBER, lexer.get_token());
    EXPECT_EQ(".1234", lexer.get_last_strnum());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleDuration)
{
    Lexer lexer("12345.678e+10m");

    EXPECT_EQ(Token::DURATION, lexer.get_token());
    EXPECT_EQ("12345.678e+10m", lexer.get_last_strnum());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}

TEST(LexerTest, HandleKeywordAndIdentifier)
{
    Lexer lexer("hello hello:world offset :");

    EXPECT_EQ(Token::IDENTIFIER, lexer.get_token());
    EXPECT_EQ("hello", lexer.get_last_word());
    EXPECT_EQ(Token::METRIC_IDENTIFIER, lexer.get_token());
    EXPECT_EQ("hello:world", lexer.get_last_word());
    EXPECT_EQ(Token::OFFSET, lexer.get_token());
    EXPECT_EQ(Token::COLON, lexer.get_token());
    EXPECT_EQ(Token::EOS, lexer.get_token());
}
