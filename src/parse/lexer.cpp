#include "promql/parse/lexer.h"

#include <ctype.h>

namespace promql {

Lexer::Lexer(const std::string& input)
    : buf(input), pos(0), brace_open(false), bracket_open(false)
{
    last_char = read_char();
}

char Lexer::read_char()
{
    if (pos == buf.size()) return -1;

    return buf[pos++];
}

Token Lexer::get_token()
{
    last_word = "";

    // skip \n, \r, space, tab and NULL
    while ((last_char == '\n') || (last_char == '\r') || (last_char == ' ') ||
           (last_char == '\t') || (last_char == 0)) {
        last_char = read_char();
    }

    /* begin and end */
    if (last_char == -1) {
        return Token::EOS;
    } else if (last_char == '#') { /* comment */
        while (last_char != '\n' && last_char != '\r')
            last_char = read_char();
        return get_token();
    } else if (last_char == ',') {
        last_char = read_char();
        return Token::COMMA;
    } else if (last_char == '*') {
        last_char = read_char();
        return Token::MUL;
    } else if (last_char == '/') {
        last_char = read_char();
        return Token::DIV;
    } else if (last_char == '+') {
        last_char = read_char();
        return Token::ADD;
    } else if (last_char == '-') {
        last_char = read_char();
        return Token::SUB;
    } else if (last_char == '%') {
        last_char = read_char();
        return Token::MOD;
    } else if (last_char == '^') {
        last_char = read_char();
        return Token::POW;
    } else if (last_char == '=') {
        last_char = read_char();
        if (last_char == '=') {
            last_char = read_char();
            return Token::EQL;
        }
        return Token::ASSIGN;
    } else if (last_char == '!') {
        last_char = read_char();
        if (last_char == '=') {
            last_char = read_char();
            return Token::NEQ;
        }

        throw LexError("unexpected character after '!'");
    } else if (last_char == '>') {
        last_char = read_char();
        if (last_char == '=') {
            last_char = read_char();
            return Token::GTE;
        }
        return Token::GTR;
    } else if (last_char == '<') {
        last_char = read_char();
        if (last_char == '=') {
            last_char = read_char();
            return Token::LTE;
        }
        return Token::LSS;
    } else if (last_char == '"' || last_char == '\'') { /* string literal */
        char quote = last_char;

        last_string = "";
        last_char = read_char();
        while (last_char != quote && last_char != '\n' && last_char != '\r' &&
               last_char != -1) {
            last_string = last_string + lex_char_lit();
        }
        if (last_char == quote) {
            last_char = read_char();
            return Token::STRING;
        } else {
            last_char = read_char();
            throw LexError("unclosed string literal");
        }
    } else if (last_char == '`') { /* raw string literal */
        last_string = "";
        last_char = read_char();
        while (last_char != '`' && last_char != -1) {
            last_string = last_string + last_char;
            last_char = read_char();
        }
        if (last_char == '`') {
            last_char = read_char();
            return Token::STRING;
        } else {
            last_char = read_char();
            throw LexError("unclosed raw string literal");
        }
    } else if (::isdigit(last_char)) {
        return lex_number(10);
    } else if (last_char == '.') {
        peek_begin();
        char next = read_char();
        peek_end();

        if (::isdigit(next)) {
            return lex_number(10);
        }
    } else if (::isalpha(last_char) || last_char == '_' || last_char == ':') {
        peek_begin();
        char next = read_char();
        peek_end();

        if (bracket_open ||
            last_char == ':' &&
                !(::isalnum(next) || last_char == '_' || next == ':')) {
            last_char = read_char();

            return Token::COLON;
        }

        return lex_identifier(brace_open);
    } else if (last_char == '(') {
        last_char = read_char();
        return Token::LEFT_PAREN;
    } else if (last_char == ')') {
        last_char = read_char();
        return Token::RIGHT_PAREN;
    } else if (last_char == '[') {
        bracket_open = true;
        last_char = read_char();
        return Token::LEFT_BRACKET;
    } else if (last_char == ']') {
        bracket_open = false;
        last_char = read_char();
        return Token::RIGHT_BRACKET;
    } else if (last_char == '{') {
        last_char = read_char();
        brace_open = true;
        return Token::LEFT_BRACE;
    } else if (last_char == '}') {
        last_char = read_char();
        brace_open = false;
        return Token::RIGHT_BRACE;
    }

    throw LexError("unexpected character: " + last_char);
    return Token::ERROR;
}

char Lexer::lex_char_lit()
{
    if (last_char == '\\') { /* escape */
        last_char = read_char();
        switch (last_char) {
        case 'b':
            last_char = read_char();
            return '\b';
        case 't':
            last_char = read_char();
            return '\t';
        case 'n':
            last_char = read_char();
            return '\n';
        case 'r':
            last_char = read_char();
            return '\r';
        case '\'':
            last_char = read_char();
            return '\'';
        case '\"':
            last_char = read_char();
            return '\"';
        case '\\':
            last_char = read_char();
            return '\\';
        }
    }

    char tmp = last_char;
    last_char = read_char();
    return tmp;
}

static int char2digit(int rad, char c)
{
    if (c >= '0' && c < '0' + rad) {
        return c - '0';
    }

    if (::toupper(c) >= 'A' && ::toupper(c) < 'A' + rad - 10) {
        return c - 'A' + 10;
    }

    return -1;
}

Token Lexer::lex_number(int rad)
{
    last_strnum = "";
    radix = rad;

    while (char2digit(rad, last_char) >= 0) {
        last_strnum = last_strnum + last_char;
        last_char = read_char();
    }

    if (rad <= 10 && last_char == '.') {
        last_strnum = last_strnum + last_char;
        last_char = read_char();
        return scan_fraction_and_suffix();
    } else if (radix <= 10 && (last_char == 'e' || last_char == 'E')) {
        return scan_fraction_and_suffix();
    } else if (last_char == 's' || last_char == 'm' || last_char == 'h' ||
               last_char == 'd' || last_char == 'w' || last_char == 'y') {
        return scan_fraction_and_suffix();
    }

    return Token::NUMBER;
}

Token Lexer::scan_fraction_and_suffix()
{
    while (char2digit(10, last_char) >= 0) {
        last_strnum = last_strnum + last_char;
        last_char = read_char();
    }
    if (last_char == 'e' || last_char == 'E') {
        last_strnum = last_strnum + last_char;
        last_char = read_char();
        if (last_char == '+' || last_char == '-') {
            last_strnum = last_strnum + last_char;
            last_char = read_char();
        }
        if (::isdigit(last_char)) {
            do {
                last_strnum = last_strnum + last_char;
                last_char = read_char();
            } while (::isdigit(last_char));
        } else {
            throw LexError("malformed floating point literal");
        }
    }

    if (last_char == 's' || last_char == 'm' || last_char == 'h' ||
        last_char == 'd' || last_char == 'w' || last_char == 'y') {
        last_strnum = last_strnum + last_char;
        last_char = read_char();
        return Token::DURATION;
    }

    return Token::NUMBER;
}

Token Lexer::lex_identifier(bool force_identifier)
{
    bool has_colon = false;

    last_word = "";
    do {
        if (last_char == ':') has_colon = true;
        last_word = last_word + last_char;
        last_char = read_char();
        if (last_char == -1) break;
    } while (::isalnum(last_char) || last_char == '_' || last_char == ':');

    if (has_colon) {
        return Token::METRIC_IDENTIFIER;
    }

    if (force_identifier) {
        return Token::IDENTIFIER;
    }

    return lookup_keyword(last_word);
}

Token Lexer::lookup_keyword(const std::string& key)
{
    static const std::unordered_map<std::string, Token> keywords = {
        {"and", Token::LAND},
        {"or", Token::LOR},
        {"unless", Token::LUNLESS},
        {"sum", Token::SUM},
        {"avg", Token::AVG},
        {"count", Token::COUNT},
        {"min", Token::MIN},
        {"max", Token::MAX},
        {"stddev", Token::STDDEV},
        {"stdvar", Token::STDVAR},
        {"topk", Token::TOP_K},
        {"bottomk", Token::BOTTOM_K},
        {"count_values", Token::COUNT_VALUES},
        {"quantile", Token::QUANTILE},
        {"offset", Token::OFFSET},
        {"by", Token::BY},
        {"without", Token::WITHOUT},
        {"on", Token::ON},
        {"ignoring", Token::IGNORING},
        {"group_left", Token::GROUP_LEFT},
        {"group_right", Token::GROUP_RIGHT},
        {"bool", Token::BOOL},
    };

    auto it = keywords.find(key);

    return (it == keywords.end()) ? Token::IDENTIFIER : it->second;
}

size_t Lexer::cur_pos() const { return pos; }

void Lexer::peek_begin() { peek_start_pos = pos; }

Token Lexer::peek_token() { return get_token(); }

void Lexer::peek_end() { pos = peek_start_pos; }

} // namespace promql
