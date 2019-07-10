#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include "common.h"
#include "value.h"

#include <functional>
#include <memory>

namespace promql {

struct EvalContext;

using FunctionType = std::function<std::unique_ptr<VectorValue>(
    const std::vector<ExecValue*>&, EvalContext&)>;

struct ExecFunction {
    std::string name;
    FunctionType pfunc;
    std::vector<ValueType> arg_types;
    ValueType return_type;

    ExecFunction(const std::string& name, FunctionType fp,
                 const std::vector<ValueType>& argtypes, ValueType rettype)
        : name(name), pfunc(fp), arg_types(argtypes), return_type(rettype)
    {}

    static const ExecFunction* get(const std::string& name);
};

} // namespace promql

#endif
