#include "parse/executor.h"

#include <easylogging++.h>

#include <cassert>
#include <sstream>

namespace promql {

Executor::Executor(IndexTree* index, tsdb::db::DB* db, ASTNode* root,
                   SystemTime start, SystemTime end, Duration interval)
    : index(index), db(db), root(root), start_time(start), end_time(end),
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
    std::unordered_set<tsdb::common::TSID> tsids;
    VectorValue* vec = new VectorValue;
    auto start_ts =
        std::chrono::duration_cast<Duration>(start_time.time_since_epoch())
            .count();

    index->resolve_label_matchers(node->get_matchers(), tsids);
    auto q = db->querier(start_ts, start_ts);
    auto series_it = q.first->select(tsids);

    while (series_it->next()) {
        auto si = series_it->at();
        std::vector<Label> labels;
        if (!index->get_labels(si->tsid(), labels)) {
            continue;
        }

        auto value_it = si->iterator();
        if (!value_it->seek(start_ts)) {
            continue;
        }

        VectorValue::Sample sample;
        for (auto&& l : labels) {
            sample.metric.emplace_back(l.name, l.value);
        }

        auto tv = value_it->at();
        sample.value = ScalarValue(start_ts, tv.second);

        vec->add_sample(std::move(sample));
    }

    push_value(vec);
}

void Executor::visit(MatrixSelectorNode* node)
{
    std::unordered_set<tsdb::common::TSID> tsids;
    MatrixValue* mat = new MatrixValue;
    auto offset = node->get_offset();
    auto maxt = start_time - offset;
    auto mint = maxt - node->get_range();
    auto start_ts =
        std::chrono::duration_cast<Duration>(mint.time_since_epoch()).count();
    auto end_ts =
        std::chrono::duration_cast<Duration>(maxt.time_since_epoch()).count();

    index->resolve_label_matchers(node->get_matchers(), tsids);
    auto q = db->querier(start_ts, end_ts);
    auto series_it = q.first->select(tsids);

    while (series_it->next()) {
        auto si = series_it->at();
        std::vector<Label> labels;
        if (!index->get_labels(si->tsid(), labels)) {
            continue;
        }

        MatrixValue::Series series;
        for (auto&& l : labels) {
            series.metric.emplace_back(l.name, l.value);
        }
        auto value_it = si->iterator();
        while (value_it->next()) {
            auto tv = value_it->at();
            series.values.emplace_back((uint64_t)tv.first, tv.second);
        }

        mat->add_series(std::move(series));
    }

    push_value(mat);
}

void Executor::visit(SubqueryNode* node) {}

} // namespace promql
