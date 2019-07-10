#include "parse/printer.h"

#include <iostream>

namespace promql {

void ASTPrinter::visit(UnaryNode* node)
{
    std::cout << padding << "UnaryNode {" << std::endl;
    std::cout << padding << "  op = " << tok2str(node->get_op()) << std::endl;
    std::cout << padding << "  operand = " << std::endl;
    enter(node->get_operand());
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::visit(BinaryNode* node)
{
    std::cout << padding << "BinaryNode {" << std::endl;
    std::cout << padding << "  op = " << tok2str(node->get_op())
              << (node->is_return_bool() ? "(bool)" : "") << std::endl;
    std::cout << padding << "  left = " << std::endl;
    enter(node->get_lhs());
    std::cout << padding << "  right = " << std::endl;
    enter(node->get_rhs());
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::visit(StringLiteralNode* node)
{
    std::cout << padding << "StringLiteralNode { value = \""
              << node->get_value() << "\" }" << std::endl;
}

void ASTPrinter::visit(NumberLiteralNode* node)
{
    std::cout << padding << "NumberLiteralNode { value = " << node->get_value()
              << " }" << std::endl;
}

void ASTPrinter::visit(FuncCallNode* node)
{
    std::cout << padding << "FuncCallNode {" << std::endl;
    std::cout << padding << "  func = \"" << node->get_func()->name << "\""
              << std::endl;
    const auto& args = node->get_args();
    if (!args.empty()) {
        std::cout << padding << "  args = [" << std::endl;
        for (auto&& p : args) {
            enter(p.get());
        }
        std::cout << padding << "  ]" << std::endl;
    }
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::visit(AggregationNode* node)
{
    std::cout << padding << "AggregationNode {" << std::endl;
    std::cout << padding << "  op = " << tok2str(node->get_op()) << std::endl;
    std::cout << padding << "  expr = " << std::endl;
    enter(node->get_expr());
    auto param = node->get_param();
    if (param) {
        std::cout << padding << "  param = " << std::endl;
        enter(param);
    }
    const auto& grouping = node->get_grouping();
    if (!grouping.empty()) {
        std::cout << padding << "  grouping = [" << std::endl;
        for (auto&& p : grouping) {
            std::cout << padding << "    " << p << "," << std::endl;
        }
        std::cout << padding << "  ]" << std::endl;
    }
    std::cout << padding
              << "  without = " << (node->is_without() ? "true" : "false")
              << std::endl;
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::visit(VectorSelectorNode* node)
{
    std::cout << padding << "VectorSelectorNode {" << std::endl;
    std::cout << padding << "  name = \"" << node->get_name() << "\""
              << std::endl;

    auto offset = node->get_offset();
    if (offset.count()) {
        std::cout << padding << "  offset = " << offset.count() << std::endl;
    }

    std::cout << padding << "  matchers = [" << std::endl;
    for (auto&& p : node->get_matchers()) {
        std::cout << padding << "    LabelMatcher { name = \"" << p.name
                  << "\", value = \"" << p.value << "\" }," << std::endl;
    }
    std::cout << padding << "  ]" << std::endl;
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::visit(MatrixSelectorNode* node)
{
    std::cout << padding << "MatrixSelectorNode {" << std::endl;
    std::cout << padding << "  name = \"" << node->get_name() << "\""
              << std::endl;
    std::cout << padding << "  range = " << node->get_range().count()
              << std::endl;

    auto offset = node->get_offset();
    if (offset.count()) {
        std::cout << padding << "  offset = " << offset.count() << std::endl;
    }

    std::cout << padding << "  matchers = [" << std::endl;
    for (auto&& p : node->get_matchers()) {
        std::cout << padding << "    LabelMatcher { name = \"" << p.name
                  << "\", value = \"" << p.value << "\" }," << std::endl;
    }
    std::cout << padding << "  ]" << std::endl;
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::visit(SubqueryNode* node)
{
    std::cout << padding << "SubqueryNode {" << std::endl;
    std::cout << padding << "  range = " << node->get_range().count()
              << std::endl;
    std::cout << padding << "  step = " << node->get_step().count()
              << std::endl;

    auto offset = node->get_offset();
    if (offset.count()) {
        std::cout << padding << "  offset = " << offset.count() << std::endl;
    }

    std::cout << padding << "  expr =" << std::endl;
    enter(node->get_expr());
    std::cout << padding << "}" << std::endl;
}

void ASTPrinter::enter(ASTNode* node)
{
    padding += "    ";
    node->visit(*this);
    padding = padding.substr(0, padding.length() - 4);
}

} // namespace promql
