#ifndef _PROMQL_PRINTER_H_
#define _PROMQL_PRINTER_H_

#include "promql/parse/ast.h"

namespace promql {

class ASTPrinter : public ASTVisitor {
public:
    virtual void visit(UnaryNode* node);
    virtual void visit(BinaryNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(NumberLiteralNode* node);
    virtual void visit(FuncCallNode* node);
    virtual void visit(AggregationNode* node);
    virtual void visit(VectorSelectorNode* node);
    virtual void visit(MatrixSelectorNode* node);
    virtual void visit(SubqueryNode* node);

private:
    std::string padding;

    void enter(ASTNode* child);
};

} // namespace promql

#endif
