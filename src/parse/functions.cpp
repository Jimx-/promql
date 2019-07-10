#include "parse/functions.h"
#include "parse/executor.h"

#include <unordered_map>

namespace promql {

namespace detail {

std::unique_ptr<VectorValue> func_time(const std::vector<ExecValue*>& values,
                                       EvalContext& ctx)
{
    auto vec = std::make_unique<VectorValue>();
    vec->add_sample({{}, {ctx.ts, ctx.ts / 1000.}});
    return vec;
}

static std::unique_ptr<VectorValue>
extrapolate_rate(const std::vector<ExecValue*>& values, EvalContext& ctx,
                 bool is_counter, bool is_rate)
{
    const auto* matrix = static_cast<const MatrixValue*>(values.front());

    for (auto&& s : matrix->get_series()) {
        if (s.values.size() < 2) {
            continue;
        }

        double counter_correction = 0, last_value;
        for (auto&& tv : s.values) {
            if (is_counter && tv.get_value() < last_value) {
                counter_correction += last_value;
            }
            last_value = tv.get_value();
        }

        double result_value =
            last_value - s.values.front().get_value() + counter_correction;

        auto dt_start = (s.values.front().get_time() - ctx.mat_start) / 1000.0;
        auto dt_end = (ctx.mat_end - s.values.back().get_time()) / 1000.0;
        auto dt_sampled =
            (s.values.back().get_time() - s.values.front().get_time()) / 1000.0;
        auto avg_dt = dt_sampled / (s.values.size() - 1);

        if (is_counter && result_value > 0 &&
            s.values.front().get_value() >= 0) {
            auto dt_zero =
                dt_sampled * (s.values.front().get_value() / result_value);
            if (dt_zero < dt_start) dt_start = dt_zero;
        }

        auto extrapolation_threshold = avg_dt * 1.1;
        auto extrapolation_dt = avg_dt;

        if (dt_start < extrapolation_threshold) {
            extrapolation_dt += dt_start;
        } else {
            extrapolation_dt += avg_dt / 2;
        }

        if (dt_end < extrapolation_threshold) {
            extrapolation_dt += dt_end;
        } else {
            extrapolation_dt += avg_dt / 2;
        }

        result_value *= extrapolation_dt / avg_dt;
        if (is_rate) {
            result_value /= (ctx.mat_end - ctx.mat_start) / 1000.0;
        }

        ctx.outvec->add_sample({{}, {ctx.ts, result_value}});
    }

    return std::move(ctx.outvec);
}

std::unique_ptr<VectorValue> func_delta(const std::vector<ExecValue*>& values,
                                        EvalContext& ctx)
{
    return extrapolate_rate(values, ctx, false, false);
}

std::unique_ptr<VectorValue>
func_increase(const std::vector<ExecValue*>& values, EvalContext& ctx)
{
    return extrapolate_rate(values, ctx, true, false);
}

std::unique_ptr<VectorValue> func_rate(const std::vector<ExecValue*>& values,
                                       EvalContext& ctx)
{
    return extrapolate_rate(values, ctx, true, true);
}

static const std::unordered_map<std::string, ExecFunction> function_table = {
    {"delta", {"delta", func_rate, {ValueType::MATRIX}, ValueType::VECTOR}},
    {"increase",
     {"increase", func_rate, {ValueType::MATRIX}, ValueType::VECTOR}},
    {"rate", {"rate", func_rate, {ValueType::MATRIX}, ValueType::VECTOR}},
    {"time", {"time", func_time, {}, ValueType::SCALAR}},
};

} // namespace detail

const ExecFunction* ExecFunction::get(const std::string& name)
{
    try {
        return &detail::function_table.at(name);
    } catch (const std::out_of_range& e) {
        return nullptr;
    }
}

} // namespace promql
