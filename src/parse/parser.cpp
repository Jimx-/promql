#include "parse/parser.h"

namespace promql {

class TypeChecker : public ASTVisitor {
    virtual void visit(UnaryNode* node)
    {
        node->get_operand()->visit(*this);

        if (node->get_op() != Token::ADD && node->get_op() != Token::SUB) {
            throw TypeCheckError(
                "only + and - operators allowed for unary expressions");
        }

        auto ot = node->get_operand()->type();
        if (ot != ValueType::SCALAR && ot != ValueType::VECTOR) {
            throw TypeCheckError("unary expression only allowed on expressions "
                                 "of type scalar or instant vector");
        }
    }

    virtual void visit(BinaryNode* node)
    {
        node->get_lhs()->visit(*this);
        node->get_rhs()->visit(*this);

        auto lt = node->get_lhs()->type();
        auto rt = node->get_lhs()->type();
    }

    virtual void visit(StringLiteralNode* node) {}
    virtual void visit(NumberLiteralNode* node) {}
    virtual void visit(FuncCallNode* node) {}
    virtual void visit(VectorSelectorNode* node) {}
    virtual void visit(MatrixSelectorNode* node) {}
    virtual void visit(SubqueryNode* node)
    {
        node->get_expr()->visit(*this);

        if (node->get_expr()->type() != ValueType::VECTOR) {
            throw TypeCheckError("subquery is only allowed on instant vector");
        }
    }
};

Parser::Parser(const std::string& input) : lex(input) { read_token(); }

void Parser::match(Token expected)
{
    if (cur_tok == expected) {
        read_token();
    } else {
        throw ParseError("unexpected token: " + tok2str(cur_tok) +
                         ", expected: " + tok2str(expected));
    }
}

void Parser::read_token() { cur_tok = lex.get_token(); }

std::unique_ptr<ASTNode> Parser::parse()
{
    auto expr = expression();
    match(Token::EOS);

    TypeChecker typechecker;
    expr->visit(typechecker);

    return expr;
}

std::unique_ptr<ASTNode> Parser::expression() { return subquery_expression(); }

std::unique_ptr<ASTNode> Parser::subquery_expression()
{
    auto expr = comparison_expression();
    std::unique_ptr<SubqueryNode> sq;

    if (cur_tok ==
        Token::LEFT_BRACKET) { /* expression followed by subquery selector */
        match(Token::LEFT_BRACKET);
        std::string range = lex.get_last_strnum();
        match(Token::DURATION);

        match(Token::COLON);
        std::string step = lex.get_last_strnum();
        match(Token::DURATION);

        sq = std::make_unique<SubqueryNode>();
        sq->set_expr(std::move(expr));
        sq->set_step(parse_duration(step));
        sq->set_range(parse_duration(range));

        match(Token::RIGHT_BRACKET);
    }

    if (sq && cur_tok == Token::OFFSET) { /* optional offset */
        match(Token::OFFSET);
        std::string offset = lex.get_last_strnum();
        match(Token::DURATION);

        sq->set_offset(parse_duration(offset));
    }

    if (sq) return sq;
    return expr;
}

std::unique_ptr<ASTNode> Parser::comparison_expression()
{
    auto t = arith_expression();
    std::unique_ptr<BinaryNode> p;
    if ((cur_tok == Token::LSS) || (cur_tok == Token::GTR) ||
        (cur_tok == Token::LTE) || (cur_tok == Token::GTE) ||
        (cur_tok == Token::EQL) || (cur_tok == Token::NEQ)) {
        p = std::make_unique<BinaryNode>();
        p->set_lhs(std::move(t));
        p->set_op(cur_tok);
        match(cur_tok);
        p->set_rhs(arith_expression());
        t = std::move(p);
    }
    return t;
}

std::unique_ptr<ASTNode> Parser::arith_expression()
{
    auto t = term();
    std::unique_ptr<BinaryNode> p;
    if ((cur_tok == Token::ADD) || (cur_tok == Token::SUB)) {
        p = std::make_unique<BinaryNode>();
        p->set_lhs(std::move(t));
        p->set_op(cur_tok);
        match(cur_tok);
        p->set_rhs(term());
        t = std::move(p);
    }
    return t;
}

std::unique_ptr<ASTNode> Parser::term()
{
    auto t = factor();
    std::unique_ptr<BinaryNode> p;
    if ((cur_tok == Token::MUL) || (cur_tok == Token::DIV) ||
        (cur_tok == Token::MOD)) {
        p = std::make_unique<BinaryNode>();
        p->set_lhs(std::move(t));
        p->set_op(cur_tok);
        match(cur_tok);
        p->set_rhs(factor());
        t = std::move(p);
    }
    return t;
}

std::unique_ptr<ASTNode> Parser::factor()
{
    PASTNode t;
    std::unique_ptr<UnaryNode> tmp;

    Token tok = cur_tok;
    switch (cur_tok) {
    case Token::ADD:
    case Token::SUB:
        match(cur_tok);
    default:
        break;
    }

    t = power();

    switch (tok) {
    case Token::ADD:
    case Token::SUB:
        tmp = std::make_unique<UnaryNode>();
        tmp->set_op(tok);
        tmp->set_operand(std::move(t));
        t = std::move(tmp);
        break;
    default:
        break;
    }

    return t;
}

std::unique_ptr<ASTNode> Parser::power()
{
    auto t = atom();
    std::unique_ptr<BinaryNode> p;
    if (cur_tok == Token::POW) {
        p = std::make_unique<BinaryNode>();
        p->set_lhs(std::move(t));
        p->set_op(cur_tok);
        match(cur_tok);
        p->set_rhs(factor());
        t = std::move(p);
    }
    return t;
}

std::unique_ptr<ASTNode> Parser::atom()
{
    PASTNode t;

    switch (cur_tok) {
    case Token::LEFT_PAREN:
        match(Token::LEFT_PAREN);
        t = expression();
        match(Token::RIGHT_PAREN);
        break;

    case Token::STRING: {
        auto node = std::make_unique<StringLiteralNode>();
        node->set_value(lex.get_last_string());
        match(Token::STRING);
        t = std::move(node);
        break;
    }

    case Token::NUMBER: {
        auto node = std::make_unique<NumberLiteralNode>();
        node->set_value(::atof(lex.get_last_strnum().c_str()));
        match(Token::NUMBER);
        t = std::move(node);
        break;
    }

    case Token::LEFT_BRACE: /* vector selector without name */
        t = vector_selector("");
        break;

    case Token::IDENTIFIER: {
        std::string name = lex.get_last_word();

        match(Token::IDENTIFIER);
        if (cur_tok == Token::LEFT_PAREN) {
            /* function call */
            t = function_call(name);
        } else {
            t = vector_selector(name);
        }
        break;
    }

    case Token::METRIC_IDENTIFIER: {
        std::string name = lex.get_last_word();

        match(Token::METRIC_IDENTIFIER);
        t = vector_selector(name);
    }

    default:
        break;
    }

    return t;
}

std::unique_ptr<ASTNode> Parser::vector_selector(const std::string& name)
{
    auto vs = std::make_unique<VectorSelectorNode>();
    std::unique_ptr<MatrixSelectorNode> ms;
    std::unique_ptr<SubqueryNode> sq;

    vs->set_name(name);

    if (cur_tok == Token::LEFT_BRACE) {
        std::vector<LabelMatcher> matchers;
        label_matchers(matchers);

        for (auto&& p : matchers) {
            vs->add_matcher(p.op, p.name, p.value);
        }
    }

    if (name.length()) {
        vs->add_matcher(MatchOp::EQL, METRIC_NAME, name);
    }

    if (cur_tok == Token::LEFT_BRACKET) { /* vector selector followed by
                                             range/subquery selector */
        match(Token::LEFT_BRACKET);
        std::string range = lex.get_last_strnum();
        match(Token::DURATION);

        if (cur_tok == Token::COLON) {
            /* subquery */
            match(Token::COLON);
            std::string step = lex.get_last_strnum();
            match(Token::DURATION);

            sq = std::make_unique<SubqueryNode>();
            sq->set_expr(std::move(vs));
            sq->set_step(parse_duration(step));
            sq->set_range(parse_duration(range));

            match(Token::RIGHT_BRACKET);
        } else {
            /* range selector */
            match(Token::RIGHT_BRACKET);

            ms = std::make_unique<MatrixSelectorNode>();
            ms->set_name(vs->get_name());
            ms->set_range(parse_duration(range));
            for (auto&& p : vs->get_matchers()) {
                ms->add_matcher(p.op, p.name, p.value);
            }

            vs.reset();
        }
    }

    if (cur_tok == Token::OFFSET) { /* optional offset */
        match(Token::OFFSET);
        std::string offset = lex.get_last_strnum();
        match(Token::DURATION);

        if (ms) {
            ms->set_offset(parse_duration(offset));
        } else if (sq) {
            sq->set_offset(parse_duration(offset));
        } else {
            vs->set_offset(parse_duration(offset));
        }
    }

    if (ms)
        return ms;
    else if (sq)
        return sq;
    return vs;
}

void Parser::label_matchers(std::vector<LabelMatcher>& matchers)
{
    match(Token::LEFT_BRACE);

    if (cur_tok != Token::RIGHT_BRACE) {
        std::string label = lex.get_last_word();
        match(Token::IDENTIFIER);
        MatchOp op = tok2mop(cur_tok);
        if (op == MatchOp::ERROR) {
            throw ParseError("unexpected label matcher operator: " +
                             tok2str(cur_tok));
        }
        match(cur_tok);
        std::string value = lex.get_last_string();
        match(Token::STRING);
        matchers.emplace_back(op, label, value);

        while (cur_tok == Token::COMMA) {
            match(cur_tok);

            std::string label = lex.get_last_word();
            match(Token::IDENTIFIER);
            MatchOp op = tok2mop(cur_tok);
            if (op == MatchOp::ERROR) {
                throw ParseError("unexpected label matcher operator: " +
                                 tok2str(cur_tok));
            }
            match(cur_tok);
            std::string value = lex.get_last_string();
            match(Token::STRING);
            matchers.emplace_back(op, label, value);
        }
    }

    match(Token::RIGHT_BRACE);
}

std::unique_ptr<ASTNode> Parser::function_call(const std::string& name)
{
    const auto* func = ExecFunction::get(name);
    auto node = std::make_unique<FuncCallNode>();

    if (!func) {
        throw ParseError("undefined function: " + name);
    }

    node->set_func(func);
    match(Token::LEFT_PAREN);

    if (cur_tok == Token::RIGHT_PAREN) {
        match(Token::RIGHT_PAREN);
        return node;
    }

    auto arg = expression();
    node->add_arg(std::move(arg));

    while (cur_tok == Token::COMMA) {
        match(cur_tok);

        arg = expression();
        node->add_arg(std::move(arg));
    }

    match(Token::RIGHT_PAREN);
    return node;
}

Duration Parser::parse_duration(const std::string& dur)
{
    uint64_t val;
    auto unit = dur.back();
    uint64_t count = ::atoi(dur.substr(0, dur.length() - 1).c_str());

    switch (unit) {
    case 'y':
        count *= 1000 * 60 * 60 * 24 * 365ULL;
        break;
    case 'w':
        count *= 1000 * 60 * 60 * 24 * 7;
        break;
    case 'd':
        count *= 1000 * 60 * 60 * 24;
        break;
    case 'h':
        count *= 1000 * 60 * 60;
        break;
    case 'm':
        count *= 1000 * 60;
        break;
    case 's':
        count *= 1000;
        break;
    default:
        break;
    }

    return Duration{count};
}

} // namespace promql
