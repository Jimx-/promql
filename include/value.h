#ifndef _VALUE_H_
#define _VALUE_H_

#include <string>

namespace promql {

enum class ValueType {
    NONE,
    SCALAR,
    STRING,
    VECTOR,
    MATRIX,
};

/* values used during query execution */
class ExecValue {
public:
    virtual ValueType type() const = 0;
};

static std::string valtype2str(ValueType vt)
{
    switch (vt) {
    case ValueType::NONE:
        return "NONE";
    case ValueType::SCALAR:
        return "SCALAR";
    case ValueType::STRING:
        return "STRING";
    case ValueType::VECTOR:
        return "VECTOR";
    case ValueType::MATRIX:
        return "MATRIX";
    }
    return "NONE";
}

} // namespace promql

#endif
