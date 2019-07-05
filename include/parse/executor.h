#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

#include "db/DB.hpp"
#include "index/index_tree.h"
#include "parse/ast.h"
#include "value.h"

#include <set>
#include <stack>

namespace promql {

struct EvalContext {
    uint64_t ts;
    std::unique_ptr<VectorValue> outvec;
};

class Executor : public ASTVisitor {
public:
    Executor(IndexTree* index, tsdb::db::DB* db, ASTNode* root,
             SystemTime start, SystemTime end, Duration interval);

    std::unique_ptr<ExecValue> execute();

    virtual void visit(UnaryNode* node);
    virtual void visit(BinaryNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(NumberLiteralNode* node);
    virtual void visit(FuncCallNode* node);
    virtual void visit(VectorSelectorNode* node);
    virtual void visit(MatrixSelectorNode* node);
    virtual void visit(SubqueryNode* node);

private:
    using EvalFunc = std::function<std::unique_ptr<VectorValue>(
        const std::vector<ExecValue*>&, EvalContext&)>;

    IndexTree* index;
    tsdb::db::DB* db;
    ASTNode* root;
    uint64_t start_timestamp, end_timestamp;
    Duration interval;
    std::stack<std::unique_ptr<MatrixValue>> value_stack;

    void push_value(std::unique_ptr<MatrixValue>&& val);
    std::unique_ptr<MatrixValue> pop_value();

    std::unique_ptr<MatrixValue> range_eval(EvalFunc&& func,
                                            const std::vector<ASTNode*>& exprs);
};

} // namespace promql

#endif
