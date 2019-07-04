#ifndef _PARSER_H_
#define _PARSER_H_

#include "common.h"
#include "parse/ast.h"
#include "parse/lexer.h"

#include <memory>

namespace promql {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& message) : std::runtime_error(message) {}
};

class TypeCheckError : public std::runtime_error {
public:
    TypeCheckError(const std::string& message) : std::runtime_error(message) {}
};

class Parser {
public:
    Parser(const std::string& input);

    Token last_token() const { return cur_tok; }

    std::unique_ptr<ASTNode> parse();

    static Duration parse_duration(const std::string& dur);

private:
    Lexer lex;
    Token cur_tok;

    void read_token();
    void match(Token expected);

    std::unique_ptr<ASTNode> expression();
    std::unique_ptr<ASTNode> subquery_expression();
    std::unique_ptr<ASTNode> comparison_expression();
    std::unique_ptr<ASTNode> arith_expression();
    std::unique_ptr<ASTNode> term();
    std::unique_ptr<ASTNode> factor();
    std::unique_ptr<ASTNode> power();
    std::unique_ptr<ASTNode> atom();

    std::unique_ptr<ASTNode> vector_selector(const std::string& name);
    void label_matchers(std::vector<LabelMatcher>& matchers);

    std::unique_ptr<ASTNode> function_call(const std::string& name);
};

} // namespace promql

#endif
