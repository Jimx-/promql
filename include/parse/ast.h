#ifndef _AST_H_
#define _AST_H_

#include "common.h"
#include "labels.h"
#include "parse/token.h"
#include "value.h"

#include <memory>
#include <vector>

namespace promql {

class ASTVisitor;

class ASTNode {
public:
    virtual ValueType type() const { return ValueType::NONE; }

    virtual void visit(ASTVisitor& visitor) = 0;
};

using PASTNode = std::unique_ptr<ASTNode>;

class UnaryNode;
class BinaryNode;
class StringLiteralNode;
class NumberLiteralNode;
class VectorSelectorNode;
class MatrixSelectorNode;
class SubqueryNode;

class ASTVisitor {
public:
    virtual void visit(UnaryNode* node) = 0;
    virtual void visit(BinaryNode* node) = 0;
    virtual void visit(StringLiteralNode* node) = 0;
    virtual void visit(NumberLiteralNode* node) = 0;
    virtual void visit(VectorSelectorNode* node) = 0;
    virtual void visit(MatrixSelectorNode* node) = 0;
    virtual void visit(SubqueryNode* node) = 0;
};

class UnaryNode : public ASTNode {
public:
    UnaryNode() : operand(nullptr) {}
    ASTNode* get_operand() const { return this->operand.get(); }
    void set_operand(PASTNode&& operand) { this->operand = std::move(operand); }
    Token get_op() const { return this->op; }
    void set_op(Token op) { this->op = op; }

    virtual ValueType type() const { return operand->type(); }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    PASTNode operand;
    Token op;
};

class BinaryNode : public ASTNode {
public:
    ASTNode* get_lhs() const { return this->lhs.get(); }
    ASTNode* get_rhs() const { return this->rhs.get(); }
    void set_lhs(PASTNode&& lhs) { this->lhs = std::move(lhs); }
    void set_rhs(PASTNode&& rhs) { this->rhs = std::move(rhs); }
    Token get_op() const { return this->op; }
    void set_op(Token op) { this->op = op; }

    virtual ValueType type() const
    {
        if (lhs->type() == ValueType::SCALAR &&
            rhs->type() == ValueType::SCALAR)
            return ValueType::SCALAR;
        return ValueType::VECTOR;
    }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    PASTNode lhs, rhs;
    Token op;
};

class StringLiteralNode : public ASTNode {
public:
    std::string get_value() const { return value; }
    void set_value(const std::string& val) { value = val; }

    virtual ValueType type() const { return ValueType::STRING; }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    std::string value;
};

class NumberLiteralNode : public ASTNode {
public:
    double get_value() const { return value; }
    void set_value(double val) { value = val; }

    virtual ValueType type() const { return ValueType::SCALAR; }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    double value;
};

class VectorSelectorNode : public ASTNode {
public:
    std::string get_name() const { return name; }
    void set_name(const std::string& name) { this->name = name; }
    const std::vector<LabelMatcher>& get_matchers() const { return matchers; }
    void add_matcher(MatchOp op, const std::string& name,
                     const std::string& value)
    {
        matchers.emplace_back(op, name, value);
    }
    Duration get_offset() const { return offset; }
    void set_offset(Duration offset) { this->offset = offset; }

    virtual ValueType type() const { return ValueType::VECTOR; }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    std::string name;
    Duration offset;
    std::vector<LabelMatcher> matchers;
};

class MatrixSelectorNode : public ASTNode {
public:
    std::string get_name() const { return name; }
    void set_name(const std::string& name) { this->name = name; }
    const std::vector<LabelMatcher>& get_matchers() const { return matchers; }
    void add_matcher(MatchOp op, const std::string& name,
                     const std::string& value)
    {
        matchers.emplace_back(op, name, value);
    }
    Duration get_range() const { return range; }
    void set_range(Duration range) { this->range = range; }
    Duration get_offset() const { return offset; }
    void set_offset(Duration offset) { this->offset = offset; }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    std::string name;
    Duration range;
    Duration offset;
    std::vector<LabelMatcher> matchers;
};

class SubqueryNode : public ASTNode {
public:
    ASTNode* get_expr() const { return expr.get(); }
    void set_expr(PASTNode&& expr) { this->expr = std::move(expr); }
    Duration get_range() const { return range; }
    void set_range(Duration range) { this->range = range; }
    Duration get_step() const { return step; }
    void set_step(Duration step) { this->step = step; }
    Duration get_offset() const { return offset; }
    void set_offset(Duration offset) { this->offset = offset; }

    virtual void visit(ASTVisitor& visitor) { visitor.visit(this); }

private:
    PASTNode expr;
    Duration range;
    Duration step;
    Duration offset;
};

} // namespace promql

#endif
