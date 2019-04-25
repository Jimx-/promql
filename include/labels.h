#ifndef _LABELS_H_
#define _LABELS_H_

#include <string>

namespace promql {

#define METRIC_NAME "__name__"

struct Label {
    std::string name, value;

    Label(const std::string& name, const std::string& value)
        : name(name), value(value)
    {}
};

enum class MatchOp {
    ERROR,
    EQL,
    NEQ,
    LSS,
    GTR,
    LTE,
    GTE,
};

struct LabelMatcher {
    MatchOp op;
    std::string name, value;

    LabelMatcher(MatchOp op, const std::string& name, const std::string& value)
        : op(op), name(name), value(value)
    {}
};

static std::string mop2str(MatchOp op)
{
    switch (op) {
    case MatchOp::EQL:
        return "==";
    case MatchOp::NEQ:
        return "!=";
    case MatchOp::LSS:
        return "<";
    case MatchOp::GTR:
        return ">";
    case MatchOp::LTE:
        return "<=";
    case MatchOp::GTE:
        return ">=";
    default:
        break;
    }
    return "ERROR";
}

} // namespace promql

#endif
