#include "parse/functions.h"

#include <unordered_map>

namespace promql {

namespace detail {

std::unique_ptr<VectorValue> func_time(const std::vector<ExecValue*>& values,
                                       uint64_t timestamp)
{
    auto vec = std::make_unique<VectorValue>();
    vec->add_sample({{}, {timestamp, timestamp / 1000.}});
    return vec;
}

static const std::unordered_map<std::string, ExecFunction> function_table = {
    {"time", {"time", func_time, ValueType::SCALAR}},
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
