#ifndef _VALUE_H_
#define _VALUE_H_

#include "labels.h"

#include <string>
#include <vector>

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
    virtual std::string to_json() const = 0;
};

class ScalarValue : public ExecValue {
public:
    ScalarValue(uint64_t time, double value) : t(time), v(value) {}
    uint64_t get_time() const { return t; }
    double get_value() const { return v; }

    virtual ValueType type() const { return ValueType::SCALAR; }
    virtual std::string to_json() const;

private:
    uint64_t t;
    double v;
};

class VectorValue : public ExecValue {
public:
    struct Sample {
        std::vector<Label> metric;
        ScalarValue value;
        Sample() : value(0, 0) {}
        Sample(const std::vector<Label>& m, const ScalarValue& v)
            : metric(m), value(v)
        {}
    };

    void add_sample(Sample&& s) { samples.push_back(std::move(s)); }
    virtual ValueType type() const { return ValueType::VECTOR; }
    virtual std::string to_json() const;

    const std::vector<Sample> get_samples() const { return samples; }
    void clear() { samples.clear(); }

private:
    std::vector<Sample> samples;
};

class MatrixValue : public ExecValue {
public:
    struct Series {
        std::vector<Label> metric;
        std::vector<ScalarValue> values;

        Series() {}
        Series(const std::vector<Label>& m, const std::vector<ScalarValue>& vs)
            : metric(m), values(vs)
        {}
    };

    void add_series(Series&& s) { series.push_back(std::move(s)); }
    virtual ValueType type() const { return ValueType::MATRIX; }
    virtual std::string to_json() const;

    const std::vector<Series>& get_series() const { return series; }
    void clear() { series.clear(); }

private:
    std::vector<Series> series;
};

static std::string valtype2str(ValueType vt)
{
    switch (vt) {
    case ValueType::NONE:
        return "none";
    case ValueType::SCALAR:
        return "scalar";
    case ValueType::STRING:
        return "string";
    case ValueType::VECTOR:
        return "vector";
    case ValueType::MATRIX:
        return "matrix";
    }
    return "NONE";
}

} // namespace promql

#endif
