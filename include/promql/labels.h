#ifndef _PROMQL_LABELS_H_
#define _PROMQL_LABELS_H_

#include <string>
#include <vector>

namespace promql {

#define METRIC_NAME "__name__"

struct Label {
    std::string name, value;

    Label(const std::string& name = "", const std::string& value = "")
        : name(name), value(value)
    {}

    bool operator<(const Label& rhs)
    {
        return name < rhs.name || (name == rhs.name && value < rhs.value);
    }
};

enum class MatchOp {
    ERROR,
    EQL,
    NEQ,
    LSS,
    GTR,
    LTE,
    GTE,
    EQL_REGEX,
    NEQ_REGEX,
};

struct LabelMatcher {
    MatchOp op;
    std::string name, value;

    LabelMatcher(MatchOp op, const std::string& name, const std::string& value)
        : op(op), name(name), value(value)
    {}

    bool match(const Label& label) const;
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

static std::string lset2str(const std::vector<Label>& lset)
{
    std::string str = "";
    for (auto&& l : lset) {
        str += l.name + ":" + l.value + "|";
    }
    return str;
}

} // namespace promql

#endif
