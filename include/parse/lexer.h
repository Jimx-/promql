#ifndef _LEXER_H_
#define _LEXER_H_

#include "parse/token.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace promql {

class LexError : public std::runtime_error {
public:
    LexError(const std::string& message) : std::runtime_error(message) {}
};

class Lexer {
public:
    Lexer(const std::string& input);

    size_t cur_pos() const;
    void peek_begin();
    Token peek_token();
    void peek_end();

    Token get_token();

    char get_last_char() const { return last_char; }
    std::string get_last_word() const { return last_word; }
    std::string get_last_string() const { return last_string; }
    char get_last_char_lit() const { return last_char_lit; }
    std::string get_last_strnum() const { return last_strnum; }

private:
    std::string buf;
    size_t pos;

    size_t peek_start_pos;

    int radix;
    /* last word, number and char in the input */
    std::string last_word;
    char last_char;
    char last_char_lit;
    std::string last_string;
    std::string last_strnum;

    bool brace_open;
    bool bracket_open;

    /* read a char from input */
    char read_char();
    Token lookup_keyword(const std::string& key);

    char lex_char_lit();
    Token lex_number(int rad);
    Token scan_fraction_and_suffix();
    Token lex_identifier(bool force_identifier = false);
};

} // namespace promql

#endif
