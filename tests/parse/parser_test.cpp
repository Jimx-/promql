#include "parse/parser.h"
#include "parse/printer.h"

#include <gtest/gtest.h>

using namespace promql;

TEST(ParserTest, HandleScalar)
{
    Parser parser("+5.5e-3");
    EXPECT_NO_THROW(parser.parse());
}

TEST(ParserTest, HandleScalarToScalarExpression)
{
    Parser parser("1 + 2/(3*1)");
    EXPECT_NO_THROW(parser.parse());
}

TEST(ParserTest, HandleUnaryExpression)
{
    Parser parser("-some_metric");
    EXPECT_NO_THROW(parser.parse());
}

TEST(ParserTest, OffsetMustBePrecededByRangeSelector)
{
    Parser parser("1 offset 1d");
    EXPECT_THROW(parser.parse(), ParseError);
}

TEST(ParserTest, UnaryOnlyAllowedForScalarAndVector)
{
    Parser parser("-'string'");
    EXPECT_THROW(parser.parse(), TypeCheckError);
}
