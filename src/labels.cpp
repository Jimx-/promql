#include "promql/labels.h"

namespace promql {

bool LabelMatcher::match(const Label& label) const
{
    return label.name == name && match_value(label.value);
}

bool LabelMatcher::match_value(const std::string& val) const
{
    bool regex_match = false;

    switch (op) {
    case MatchOp::ERROR:
        return false;
    case MatchOp::EQL:
        return val == value;
    case MatchOp::NEQ:
        return val != value;
    case MatchOp::LSS:
        return val < value;
    case MatchOp::GTR:
        return val > value;
    case MatchOp::LTE:
        return val <= value;
    case MatchOp::GTE:
        return val >= value;
    case MatchOp::EQL_REGEX:
        regex_match = true;
        /* fall-through */
    case MatchOp::NEQ_REGEX: {
        return std::regex_match(val, pattern) == regex_match;
    }
    }

    return false;
}

} // namespace promql
