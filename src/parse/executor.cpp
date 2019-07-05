#include "parse/executor.h"
#include "labels.h"
#include "parse/token.h"

#include <easylogging++.h>

#include <cassert>
#include <sstream>
#include <unordered_map>

namespace promql {

static double vec_elt_binop(Token op, double lhs, double rhs, bool& keep)
{
    keep = true;
    switch (op) {
    case Token::ADD:
        return lhs + rhs;
    case Token::SUB:
        return lhs - rhs;
    case Token::MUL:
        return lhs * rhs;
    case Token::DIV:
        return lhs / rhs;
    default:
        return 0.0;
    }
}

static std::unique_ptr<VectorValue> vec_scalar_binop(Token op, VectorValue* lhs,
                                                     ScalarValue* rhs,
                                                     bool swap, bool logical,
                                                     EvalContext& ctx)
{
    for (auto&& p : lhs->get_samples()) {
        auto lv = p.value.get_value();
        auto rv = rhs->get_value();

        if (swap) {
            auto tmp = lv;
            lv = rv;
            rv = tmp;
        }

        bool keep;
        auto value = vec_elt_binop(op, lv, rv, keep);

        if (swap && is_comparison_op(op)) {
            value = rv;
        }

        if (logical) {
            value = keep ? 1.0 : 0.0;
            keep = true;
        }

        if (keep) {
            VectorValue::Sample sample(p.metric, {p.value.get_time(), value});
            ctx.outvec->add_sample(std::move(sample));
        }
    }

    return std::move(ctx.outvec);
}

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
                start_timestamp, retval->get_series()[0].values[0].get_value());
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
    std::unordered_map<std::string, MatrixValue::Series> seriess;
    std::vector<std::unique_ptr<MatrixValue>> mats;
    for (auto&& expr : exprs) {
        expr->visit(*this);
        assert(!value_stack.empty());
        mats.push_back(pop_value());
    }

    EvalContext ctx;
    ctx.outvec = std::make_unique<VectorValue>();
    std::vector<std::vector<size_t>> start_indices;
    for (auto&& mat : mats) {
        start_indices.emplace_back(mat->get_series().size(), 0);
    }

    for (auto ts = start_timestamp; ts <= end_timestamp;
         ts += interval.count()) {
        std::vector<std::unique_ptr<VectorValue>> vecs;
        std::vector<ExecValue*> args;

        auto start_index_mat = start_indices.begin();
        for (auto&& mat : mats) {
            auto vec = std::make_unique<VectorValue>();

            auto start_index = start_index_mat->begin();
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
                start_index++;
            }
            start_index_mat++;
            args.push_back(vec.get());
            vecs.push_back(std::move(vec));
        }

        ctx.ts = ts;
        auto result = func(args, ctx);

        for (auto&& p : result->get_samples()) {
            std::string lstr = lset2str(p.metric);
            auto it = seriess.find(lstr);

            if (it == seriess.end()) {
                auto itp =
                    seriess.emplace(lstr, MatrixValue::Series{p.metric, {}});
                it = itp.first;
            }
            it->second.values.emplace_back(ts, p.value.get_value());
        }

        result->clear();
        ctx.outvec = std::move(result);
    }

    auto mat = std::make_unique<MatrixValue>();
    for (auto&& p : seriess) {
        mat->add_series(std::move(p.second));
    }

    return mat;
}

void Executor::visit(BinaryNode* node)
{
    auto lhs_type = node->get_lhs()->type();
    auto rhs_type = node->get_rhs()->type();

    if (lhs_type == ValueType::VECTOR && rhs_type == ValueType::SCALAR) {
        push_value(range_eval(
            [node](const std::vector<ExecValue*>& args, EvalContext& ctx) {
                VectorValue* lhs = static_cast<VectorValue*>(args[0]);
                VectorValue* rhs = static_cast<VectorValue*>(args[1]);
                ScalarValue rv = rhs->get_samples()[0].value;

                return vec_scalar_binop(node->get_op(), lhs, &rv, false, false,
                                        ctx);
            },
            {node->get_lhs(), node->get_rhs()}));
    } else if (rhs_type == ValueType::VECTOR && lhs_type == ValueType::SCALAR) {
        push_value(range_eval(
            [node](const std::vector<ExecValue*>& args, EvalContext& ctx) {
                VectorValue* lhs = static_cast<VectorValue*>(args[0]);
                ScalarValue lv = lhs->get_samples()[0].value;
                VectorValue* rhs = static_cast<VectorValue*>(args[1]);

                return vec_scalar_binop(node->get_op(), rhs, &lv, true, false,
                                        ctx);
            },
            {node->get_lhs(), node->get_rhs()}));
    }
}

void Executor::visit(UnaryNode* node) {}

void Executor::visit(StringLiteralNode* node) {}

void Executor::visit(NumberLiteralNode* node)
{
    push_value(range_eval(
        [node](const std::vector<ExecValue*>& args, EvalContext& ctx) {
            ctx.outvec->add_sample({{}, {0, node->get_value()}});
            return std::move(ctx.outvec);
        },
        {}));
}

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
            series.values.emplace_back(ts, tv.second);
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
