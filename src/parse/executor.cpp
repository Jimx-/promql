#include "parse/executor.h"

#include <easylogging++.h>

#include <cassert>
#include <sstream>

namespace promql {

Executor::Executor(IndexTree* index, ASTNode* root, SystemTime start,
                   SystemTime end, Duration interval)
    : index(index), root(root), start_time(start), end_time(end),
      interval(interval)
{}

std::unique_ptr<ExecValue> Executor::execute()
{
    root->visit(*this);

    if (root->type() == ValueType::NONE) {
        return nullptr;
    }

    assert(value_stack.size() == 1);
    return pop_value();
}

void Executor::push_value(ExecValue* val) { value_stack.emplace(val); }

std::unique_ptr<ExecValue> Executor::pop_value()
{
    if (value_stack.empty()) {
        return nullptr;
    }

    auto val = std::move(value_stack.top());
    value_stack.pop();

    return val;
}

void Executor::resolve_label_matchers(const std::vector<LabelMatcher>& matchers,
                                      std::set<PostingID>& posting_ids)
{
    bool first = true;
    for (auto&& p : matchers) {
        std::set<PostingID> postings;
        index->query_postings(p, postings);

        if (first) {
            posting_ids = std::move(postings);
            first = false;
        } else {
            auto it1 = posting_ids.begin();
            auto it2 = postings.begin();

            while ((it1 != posting_ids.end()) && (it2 != postings.end())) {
                if (*it1 < *it2) {
                    posting_ids.erase(it1++);
                } else if (*it2 < *it1) {
                    ++it2;
                } else {
                    ++it1;
                    ++it2;
                }
            }
            posting_ids.erase(it1, posting_ids.end());
        }
    }
}

void Executor::visit(BinaryNode* node)
{
    node->get_lhs()->visit(*this);
    node->get_rhs()->visit(*this);

    LOG(INFO) << "Binary Node: " << valtype2str(node->get_lhs()->type()) << " "
              << tok2str(node->get_op()) << " "
              << valtype2str(node->get_rhs()->type());
}

void Executor::visit(UnaryNode* node) {}

void Executor::visit(StringLiteralNode* node) {}

void Executor::visit(NumberLiteralNode* node) {}

void Executor::visit(VectorSelectorNode* node)
{
    std::set<PostingID> posting_ids;
    MatrixValue* mat = new MatrixValue;

    resolve_label_matchers(node->get_matchers(), posting_ids);

    std::stringstream ss;
    bool first;
    ss << "Vector Selector: {";
    first = true;
    for (auto&& p : node->get_matchers()) {
        ss << (first ? "" : ", ") << p.name << " " << mop2str(p.op) << " "
           << '"' << p.value << '"';
        first = false;
    }
    ss << "} => [";
    first = true;
    for (auto&& p : posting_ids) {
        ss << (first ? "" : ", ") << p;
        first = false;

        MatrixValue::Series series;
        for (auto&& m : node->get_matchers()) {
            series.metric.emplace_back(m.name, m.value);
        }
        series.values.push_back(ScalarValue{
            (uint64_t)start_time.time_since_epoch().count(), 100.0});

        mat->add_series(std::move(series));
    }
    ss << "], at " << start_time.time_since_epoch().count();

    LOG(INFO) << ss.str();
    push_value(mat);
}

void Executor::visit(MatrixSelectorNode* node)
{
    std::set<PostingID> posting_ids;
    MatrixValue* mat = new MatrixValue;
    auto offset = node->get_offset();
    auto maxt = start_time - offset;
    auto mint = maxt - node->get_range();

    resolve_label_matchers(node->get_matchers(), posting_ids);

    std::stringstream ss;
    bool first;
    ss << "Matrix Selector: {";
    first = true;
    for (auto&& p : node->get_matchers()) {
        ss << (first ? "" : ", ") << p.name << " " << mop2str(p.op) << " "
           << '"' << p.value << '"';
        first = false;
    }
    ss << "} => [";
    first = true;
    for (auto&& p : posting_ids) {
        ss << (first ? "" : ", ") << p;
        first = false;

        MatrixValue::Series series;
        for (auto&& m : node->get_matchers()) {
            series.metric.emplace_back(m.name, m.value);
        }
        series.values.push_back(ScalarValue{
            (uint64_t)start_time.time_since_epoch().count(), 100.0});

        mat->add_series(std::move(series));
    }
    ss << "], from " << mint.time_since_epoch().count() << " to "
       << maxt.time_since_epoch().count();

    LOG(INFO) << ss.str();
    push_value(mat);
}

void Executor::visit(SubqueryNode* node) {}

} // namespace promql
