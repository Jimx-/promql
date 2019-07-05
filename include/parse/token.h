#ifndef _TOKEN_H_
#define _TOKEN_H_

#include "labels.h"

#include <string>

namespace promql {

enum class Token {
    ERROR,
    EOS, /* end of stream */
    COMMENT,
    IDENTIFIER,
    METRIC_IDENTIFIER,
    LEFT_PAREN,
    RIGHT_PAREN,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_BRACKET,
    RIGHT_BRACKET,
    COMMA,
    ASSIGN,
    COLON,
    SEMICOLON,
    STRING,
    NUMBER,
    DURATION,
    BLANK,
    TIMES,
    SPACE,

    // operators
    SUB,
    ADD,
    MUL,
    MOD,
    DIV,
    LAND,
    LOR,
    LUNLESS,
    EQL,
    NEQ,
    LTE,
    LSS,
    GTE,
    GTR,
    EQL_REGEX,
    NEQ_REGEX,
    POW,

    // aggregators
    AVG,
    COUNT,
    SUM,
    MIN,
    MAX,
    STDDEV,
    STDVAR,
    TOP_K,
    BOTTOM_K,
    COUNT_VALUES,
    QUANTILE,

    // keywords
    OFFSET,
    BY,
    WITHOUT,
    ON,
    IGNORING,
    GROUP_LEFT,
    GROUP_RIGHT,
    BOOL,
};

static std::string tok2str(Token tok)
{
    switch (tok) {
    case Token::ERROR:
        return "ERROR";
    case Token::EOS:
        return "EOS";
    case Token::COMMENT:
        return "COMMENT";
    case Token::IDENTIFIER:
        return "IDENTIFIER";
    case Token::METRIC_IDENTIFIER:
        return "METRIC_IDENTIFIER";
    case Token::LEFT_PAREN:
        return "LEFT_PAREN";
    case Token::RIGHT_PAREN:
        return "RIGHT_PAREN";
    case Token::LEFT_BRACE:
        return "LEFT_BRACE";
    case Token::RIGHT_BRACE:
        return "RIGHT_BRACE";
    case Token::LEFT_BRACKET:
        return "LEFT_BRACKET";
    case Token::RIGHT_BRACKET:
        return "RIGHT_BRACKET";
    case Token::COMMA:
        return "COMMA";
    case Token::ASSIGN:
        return "ASSIGN";
    case Token::COLON:
        return "COLON";
    case Token::SEMICOLON:
        return "SEMICOLON";
    case Token::STRING:
        return "STRING";
    case Token::NUMBER:
        return "NUMBER";
    case Token::DURATION:
        return "DURATION";
    case Token::BLANK:
        return "BLANK";
    case Token::TIMES:
        return "TIMES";
    case Token::SPACE:
        return "SPACE";
    case Token::SUB:
        return "SUB";
    case Token::ADD:
        return "ADD";
    case Token::MUL:
        return "MUL";
    case Token::MOD:
        return "MOD";
    case Token::DIV:
        return "DIV";
    case Token::LAND:
        return "LAND";
    case Token::LOR:
        return "LOR";
    case Token::LUNLESS:
        return "LUNLESS";
    case Token::EQL:
        return "EQL";
    case Token::NEQ:
        return "NEQ";
    case Token::LTE:
        return "LTE";
    case Token::LSS:
        return "LSS";
    case Token::GTE:
        return "GTE";
    case Token::GTR:
        return "GTR";
    case Token::EQL_REGEX:
        return "EQL_REGEX";
    case Token::NEQ_REGEX:
        return "NEQ_REGEX";
    case Token::POW:
        return "POW";
    case Token::AVG:
        return "AVG";
    case Token::COUNT:
        return "COUNT";
    case Token::SUM:
        return "SUM";
    case Token::MIN:
        return "MIN";
    case Token::MAX:
        return "MAX";
    case Token::STDDEV:
        return "STDDEV";
    case Token::STDVAR:
        return "STDVAR";
    case Token::TOP_K:
        return "TOP_K";
    case Token::BOTTOM_K:
        return "BOTTOM_K";
    case Token::COUNT_VALUES:
        return "COUNT_VALUES";
    case Token::QUANTILE:
        return "QUANTILE";
    case Token::OFFSET:
        return "OFFSET";
    case Token::BY:
        return "BY";
    case Token::WITHOUT:
        return "WITHOUT";
    case Token::ON:
        return "ON";
    case Token::IGNORING:
        return "IGNORING";
    case Token::GROUP_LEFT:
        return "GROUP_LEFT";
    case Token::GROUP_RIGHT:
        return "GROUP_RIGHT";
    case Token::BOOL:
        return "BOOL";
    }

    return "ERROR";
}

static MatchOp tok2mop(Token tok)
{
    switch (tok) {
    case Token::EQL:
        return MatchOp::EQL;
    case Token::NEQ:
        return MatchOp::NEQ;
    case Token::LSS:
        return MatchOp::LSS;
    case Token::GTR:
        return MatchOp::GTR;
    case Token::LTE:
        return MatchOp::LTE;
    case Token::GTE:
        return MatchOp::GTE;
    default:
        break;
    }

    return MatchOp::ERROR;
}

static bool is_comparison_op(Token op)
{
    switch (op) {
    case Token::EQL:
    case Token::NEQ:
    case Token::LTE:
    case Token::LSS:
    case Token::GTE:
    case Token::GTR:
    case Token::EQL_REGEX:
    case Token::NEQ_REGEX:
        return true;
    }
    return false;
}

} // namespace promql

#endif
