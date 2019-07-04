#include "parse/executor.h"

#include <easylogging++.h>

#include <cassert>
#include <sstream>

namespace promql {

Executor::Executor(IndexTree* index, tsdb::db::DB* db, ASTNode* root,
                   SystemTime start, SystemTime end, Duration interval)
    : index(index), db(db), root(root),
      start_timestamp(
          std::chrono::duration_cast<Duration>(start.time_since_epoch())
              .count()),
      end_timestamp(
          std::chrono::duration_cast<Duration>(end.time_since_epoch()).count()),
      interval(interval)
{}

std::unique_ptr<ExecValue> Executor::execute()
{
    root->visit(*this);

    if (root->type() == ValueType::NONE) {
        return nullptr;
    }

    assert(value_stack.size() == 1);
    auto retval = pop_value();

    if (start_timestamp == end_timestamp && interval.count() == 1) {
        /* instant query: cast the returned matrix to actual type */
        if (root->type() == ValueType::SCALAR) {
            return std::make_unique<ScalarValue>(
                retval->get_series()[0].values[0].get_value(), start_timestamp);
        }
    }

    return retval;
}

void Executor::push_value(std::unique_ptr<MatrixValue>&& val)
{
    value_stack.emplace(std::move(val));
}

std::unique_ptr<MatrixValue> Executor::pop_value()
{
    if (value_stack.empty()) {
        return nullptr;
    }

    auto val = std::move(value_stack.top());
    value_stack.pop();

    return val;
}

std::unique_ptr<MatrixValue>
Executor::range_eval(Executor::EvalFunc&& func,
                     const std::vector<ASTNode*>& exprs)
{
    std::vector<MatrixValue::Series> seriess;
    std::vector<std::unique_ptr<MatrixValue>> mats;
    for (auto&& expr : exprs) {
        expr->visit(*this);
        assert(!value_stack.empty());
        mats.push_back(pop_value());
    }

    EvalContext ctx;
    std::vector<size_t> start_indices(exprs.size(), 0);
    for (auto ts = start_timestamp; ts <= end_timestamp;
         ts += interval.count()) {
        std::vector<std::unique_ptr<VectorValue>> vecs;
        std::vector<ExecValue*> args;

        auto start_index = start_indices.begin();
        for (auto&& mat : mats) {
            auto vec = std::make_unique<VectorValue>();

            for (auto&& series : mat->get_series()) {
                for (size_t i = *start_index; i < series.values.size(); i++) {
                    if (series.values[i].get_time() == ts) {
                        *start_index = i + 1;
                        vec->add_sample({series.metric,
                                         {series.values[i].get_time(),
                                          series.values[i].get_value()}});
                    }
                    break;
                }
            }
            start_index++;
            args.push_back(vec.get());
            vecs.push_back(std::move(vec));
        }

        ctx.ts = ts;
        auto result = func(args, ctx);

        if (seriess.empty()) {
            for (auto&& p : result->get_samples()) {
                seriess.emplace_back();
                seriess.back().metric = p.metric;
            }
        }

        auto it = seriess.begin();
        for (auto&& p : result->get_samples()) {
            it->values.emplace_back(ts, p.value.get_value());
        }
    }

    auto mat = std::make_unique<MatrixValue>();
    for (auto&& p : seriess) {
        mat->add_series(std::move(p));
    }

    return mat;
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

void Executor::visit(FuncCallNode* node)
{
    std::vector<ASTNode*> args;
    for (auto&& p : node->get_args()) {
        args.push_back(p.get());
    }

    push_value(range_eval(
        [node](const std::vector<ExecValue*>& args, EvalContext& ctx) {
            return node->get_func()->pfunc(args, ctx.ts);
        },
        args));
}

void Executor::visit(VectorSelectorNode* node)
{
    std::unordered_set<tsdb::common::TSID> tsids;
    auto mat = std::make_unique<MatrixValue>();

    index->resolve_label_matchers(node->get_matchers(), tsids);
    auto q = db->querier(start_timestamp, end_timestamp);

    auto series_it = q.first->select(tsids);

    while (series_it && series_it->next()) {
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
        for (auto ts = start_timestamp; ts <= end_timestamp;
             ts += interval.count()) {
            if (!value_it->seek(ts)) {
                continue;
            }

            auto tv = value_it->at();
            series.values.emplace_back((uint64_t)tv.first, tv.second);
        }

        if (!series.values.empty()) mat->add_series(std::move(series));
    }

    push_value(std::move(mat));
}

void Executor::visit(MatrixSelectorNode* node)
{
    std::unordered_set<tsdb::common::TSID> tsids;
    auto mat = std::make_unique<MatrixValue>();
    auto offset = node->get_offset();
    auto maxt = end_timestamp - offset.count();
    auto mint = maxt - node->get_range().count();

    index->resolve_label_matchers(node->get_matchers(), tsids);
    auto q = db->querier(start_timestamp, end_timestamp);

    auto series_it = q.first->select(tsids);

    while (series_it && series_it->next()) {
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

        if (!series.values.empty()) mat->add_series(std::move(series));
    }

    push_value(std::move(mat));
}

void Executor::visit(SubqueryNode* node) {}

} // namespace promql
