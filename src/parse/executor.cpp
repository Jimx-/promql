#include "parse/executor.h"
#include "labels.h"
#include "parse/token.h"

#include <easylogging++.h>

#include <cassert>
#include <sstream>
#include <unordered_map>

namespace promql {

static inline double vec_elt_binop(Token op, double lhs, double rhs, bool& keep)
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
    case Token::POW:
        return ::pow(lhs, rhs);
    case Token::MOD:
        return ::fmod(lhs, rhs);
    case Token::EQL:
        keep = lhs == rhs;
        return lhs;
    case Token::NEQ:
        keep = lhs != rhs;
        return lhs;
    case Token::GTR:
        keep = lhs > rhs;
        return lhs;
    case Token::LSS:
        keep = lhs < rhs;
        return lhs;
    case Token::GTE:
        keep = lhs >= rhs;
        return lhs;
    case Token::LTE:
        keep = lhs <= rhs;
        return lhs;
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

                return vec_scalar_binop(node->get_op(), lhs, &rv, false,
                                        node->is_return_bool(), ctx);
            },
            {node->get_lhs(), node->get_rhs()}));
    } else if (rhs_type == ValueType::VECTOR && lhs_type == ValueType::SCALAR) {
        push_value(range_eval(
            [node](const std::vector<ExecValue*>& args, EvalContext& ctx) {
                VectorValue* lhs = static_cast<VectorValue*>(args[0]);
                ScalarValue lv = lhs->get_samples()[0].value;
                VectorValue* rhs = static_cast<VectorValue*>(args[1]);

                return vec_scalar_binop(node->get_op(), rhs, &lv, true,
                                        node->is_return_bool(), ctx);
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
    bool has_matrix_arg = false;
    size_t matrix_arg_idx = 0;

    for (auto it = node->get_func()->arg_types.begin();
         it != node->get_func()->arg_types.end(); it++) {
        if (*it == ValueType::MATRIX) {
            has_matrix_arg = true;
            matrix_arg_idx = it - node->get_func()->arg_types.begin();
            break;
        }
    }

    if (!has_matrix_arg) {
        /* if there is no matrix arg then handle the call with range_eval */
        std::vector<ASTNode*> args;
        for (auto&& p : node->get_args()) {
            args.push_back(p.get());
        }

        push_value(range_eval(
            [node](const std::vector<ExecValue*>& args, EvalContext& ctx) {
                return node->get_func()->pfunc(args, ctx);
            },
            args));
    }

    std::vector<std::unique_ptr<MatrixValue>> mats;
    std::vector<std::unique_ptr<VectorValue>> vec_args;
    std::unique_ptr<MatrixValue> mat_arg;
    std::vector<ExecValue*> args;
    auto out_mat = std::make_unique<MatrixValue>();

    size_t arg_idx = 0;
    auto saved_sts = start_timestamp;
    Duration mat_range, mat_offset;
    for (auto&& arg : node->get_args()) {
        if (arg_idx == matrix_arg_idx) {
            /* reset the time range to make sure we get all the points */
            mat_range = arg->get_range();
            mat_offset = arg->get_offset();
            start_timestamp -= mat_range.count() + mat_offset.count();
        }

        arg->visit(*this);
        assert(!value_stack.empty());
        mats.push_back(pop_value());
        start_timestamp = saved_sts;

        if (arg_idx == matrix_arg_idx) {
            mat_arg = std::make_unique<MatrixValue>();
            vec_args.push_back(nullptr);
            args.push_back(mat_arg.get());
        } else {
            vec_args.emplace_back();
            args.push_back(vec_args.back().get());
        }

        arg_idx++;
    }

    auto matrix_arg = mats[matrix_arg_idx].get();
    EvalContext ctx;
    ctx.outvec = std::make_unique<VectorValue>();
    for (auto&& s : matrix_arg->get_series()) {
        MatrixValue::Series ss(s.metric, {});
        int step = 0;
        auto lower = s.values.cbegin(), upper = s.values.cbegin();

        for (auto ts = start_timestamp; ts <= end_timestamp;
             ts += interval.count(), step++) {
            for (size_t i = 0; i < mats.size(); i++) {
                /* prepare non-matrix args */
                if (i == matrix_arg_idx) {
                    continue;
                }

                vec_args[i]->clear();
                auto& vec_series = mats[i]->get_series()[0];
                vec_args[i]->add_sample(
                    {vec_series.metric, vec_series.values[step]});
            }

            /* get data point slice */
            auto maxt = ts - mat_offset.count();
            auto mint = maxt - mat_range.count();
            while (upper != s.values.end() && upper->get_time() <= maxt)
                upper++;
            while (lower < upper && lower->get_time() < mint)
                lower++;

            mat_arg->clear();
            mat_arg->add_series({s.metric, {lower, upper}});

            ctx.ts = ts;
            ctx.mat_start = mint;
            ctx.mat_end = maxt;
            auto result = node->get_func()->pfunc(args, ctx);

            if (!result->get_samples().empty()) {
                ss.values.push_back(
                    {ts, result->get_samples()[0].value.get_value()});
            }

            result->clear();
            ctx.outvec = std::move(result);
        }

        if (!ss.values.empty()) {
            out_mat->add_series(std::move(ss));
        }
    }

    push_value(std::move(out_mat));
}

void Executor::visit(AggregationNode* node)
{
    std::vector<ASTNode*> args;
    args.push_back(node->get_expr());
    if (node->get_param()) {
        args.push_back(node->get_param());
    }

    push_value(range_eval(
        [node, this](const std::vector<ExecValue*>& args, EvalContext& ctx) {
            VectorValue* vec = static_cast<VectorValue*>(args[0]);
            double param = 0;
            if (node->get_param()) {
                param = static_cast<VectorValue*>(args[1])
                            ->get_samples()[0]
                            .value.get_value();
            }

            return this->aggregation(node->get_op(), node->get_grouping(),
                                     node->is_without(), param, vec, ctx);
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

static bool sample_lt(const VectorValue::Sample& lhs,
                      const VectorValue::Sample& rhs)
{
    return lhs.value.get_value() < rhs.value.get_value();
}

static bool sample_gt(const VectorValue::Sample& lhs,
                      const VectorValue::Sample& rhs)
{
    return lhs.value.get_value() > rhs.value.get_value();
}

std::unique_ptr<VectorValue>
Executor::aggregation(Token op, const std::vector<std::string>& grouping,
                      bool without, double param, VectorValue* vec,
                      EvalContext& ctx)
{
    struct AggregationGroup {
        std::vector<Label> labels;
        double value, mean;
        int group_count;
        std::vector<VectorValue::Sample> heap;
    };

    std::unordered_map<std::string, AggregationGroup> groups;
    std::unordered_set<std::string> grouping_set(grouping.begin(),
                                                 grouping.end());
    int k = (int)param;
    double q = param;

    for (auto&& s : vec->get_samples()) {
        const auto& metric = s.metric;
        std::vector<Label> labels;

        for (auto&& l : metric) {
            bool found = grouping_set.find(l.name) != grouping_set.end();

            if (without == found) {
                continue;
            }

            if (without && l.name == METRIC_NAME) continue;

            labels.push_back(l);
        }

        std::sort(labels.begin(), labels.end());
        auto lstr = lset2str(labels);

        auto it = groups.find(lstr);
        if (it == groups.end()) {
            AggregationGroup group;
            group.group_count = 1;
            group.labels = std::move(labels);
            group.value = s.value.get_value();
            group.mean = s.value.get_value();

            switch (op) {
            case Token::STDVAR:
            case Token::STDDEV:
                group.value = 0;
                break;
            case Token::TOP_K:
            case Token::BOTTOM_K:
            case Token::QUANTILE:
                group.heap.push_back(std::move(s));
                break;
            default:
                break;
            }

            groups[lstr] = std::move(group);
            continue;
        }

        auto& group = it->second;
        switch (op) {
        case Token::SUM:
            group.value += s.value.get_value();
            break;
        case Token::AVG:
            group.group_count++;
            group.mean +=
                (s.value.get_value() - group.mean) / (double)group.group_count;
            break;
        case Token::MAX:
            if (group.value < s.value.get_value()) {
                group.value = s.value.get_value();
            }
            break;
        case Token::MIN:
            if (group.value > s.value.get_value()) {
                group.value = s.value.get_value();
            }
            break;
        case Token::COUNT:
        case Token::COUNT_VALUES:
            group.group_count++;
            break;
        case Token::STDVAR:
        case Token::STDDEV: {
            group.group_count++;
            double delta = (s.value.get_value() - group.mean);
            group.mean += delta / (double)group.group_count;
            group.value += delta * (s.value.get_value() - group.mean);
            break;
        }
        case Token::TOP_K:
        case Token::BOTTOM_K: {
            auto cmp = sample_gt;
            if (op == Token::BOTTOM_K) {
                cmp = sample_lt;
            }

            if (group.heap.size() < k || cmp(s, group.heap.front())) {
                if (group.heap.size() == k) {
                    group.heap.front() = s;
                    std::make_heap(group.heap.begin(), group.heap.end(), cmp);
                } else {
                    group.heap.push_back(s);
                    std::push_heap(group.heap.begin(), group.heap.end(), cmp);
                }
            }
            break;
        }

        case Token::QUANTILE:
            group.heap.push_back(s);
            std::push_heap(group.heap.begin(), group.heap.end(), sample_gt);
            break;

        default:
            throw ExecutionError("expected aggregation operator, got " +
                                 tok2str(op));
        }
    }

    for (auto&& p : groups) {
        auto& aggr = p.second;

        switch (op) {
        case Token::AVG:
            aggr.value = aggr.mean;
            break;
        case Token::COUNT:
        case Token::COUNT_VALUES:
            aggr.value = aggr.group_count;
            break;
        case Token::STDDEV:
            aggr.value = ::sqrt(aggr.value / (double)aggr.group_count);
            break;
        case Token::STDVAR:
            aggr.value = aggr.value / (double)aggr.group_count;
            break;

        case Token::TOP_K:
        case Token::BOTTOM_K:
            for (auto&& p : aggr.heap) {
                ctx.outvec->add_sample(std::move(p));
            }
            continue;

        case Token::QUANTILE: {
            if (aggr.heap.empty()) {
                aggr.value = 0;
                break;
            }

            double rank = q * (aggr.heap.size() - 1);
            size_t lower = (rank < 0) ? 0 : (size_t)rank;
            size_t upper = lower + 1;
            if (upper >= aggr.heap.size()) upper = aggr.heap.size() - 1;

            auto cmp = sample_gt;
            if (op == Token::BOTTOM_K) {
                cmp = sample_lt;
            }

            double weight = rank - lower;
            size_t i = 0;
            double value = 0;
            while (!aggr.heap.empty()) {
                std::pop_heap(aggr.heap.begin(), aggr.heap.end(), cmp);

                if (i == lower) {
                    value += aggr.heap.back().value.get_value() * (1 - weight);
                }
                if (i == upper) {
                    value += aggr.heap.back().value.get_value() * weight;
                    break;
                }
                aggr.heap.pop_back();
                i++;
            }
            aggr.value = value;
            break;
        }

        default:
            break;
        }

        ctx.outvec->add_sample(
            {std::move(aggr.labels), ScalarValue(ctx.ts, aggr.value)});
    }

    return std::move(ctx.outvec);
}

} // namespace promql
