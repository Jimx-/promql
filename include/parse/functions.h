#ifndef _FUNCTIONS_H_
#define _FUNCTIONS_H_

#include "common.h"
#include "value.h"

#include <functional>
#include <memory>

namespace promql {

using FunctionType = std::function<std::unique_ptr<VectorValue>(
    const std::vector<ExecValue*>&, uint64_t)>;

struct ExecFunction {
    std::string name;
    FunctionType pfunc;
    ValueType return_type;

    ExecFunction(const std::string& name, FunctionType fp, ValueType rettype)
        : name(name), pfunc(fp), return_type(rettype)
    {}

    static const ExecFunction* get(const std::string& name);
};

} // namespace promql

#endif
