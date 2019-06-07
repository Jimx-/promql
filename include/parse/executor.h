#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

#include "db/DB.hpp"
#include "index/index_tree.h"
#include "parse/ast.h"
#include "value.h"

#include <set>
#include <stack>

namespace promql {

class Executor : public ASTVisitor {
public:
    Executor(IndexTree* index, tsdb::db::DB* db, ASTNode* root,
             SystemTime start, SystemTime end, Duration interval);

    std::unique_ptr<ExecValue> execute();

    virtual void visit(UnaryNode* node);
    virtual void visit(BinaryNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(NumberLiteralNode* node);
    virtual void visit(VectorSelectorNode* node);
    virtual void visit(MatrixSelectorNode* node);
    virtual void visit(SubqueryNode* node);

private:
    IndexTree* index;
    tsdb::db::DB* db;
    ASTNode* root;
    SystemTime start_time, end_time;
    Duration interval;
    std::stack<std::unique_ptr<ExecValue>> value_stack;

    void push_value(ExecValue* val);
    std::unique_ptr<ExecValue> pop_value();
};

} // namespace promql

#endif
