#include "promql/labels.h"

#include <regex>

namespace promql {

bool LabelMatcher::match(const Label& label) const
{
    bool regex_match = false;

    switch (op) {
    case MatchOp::ERROR:
        return false;
    case MatchOp::EQL:
        return label.name == name && label.value == value;
    case MatchOp::NEQ:
        return label.name == name && label.value != value;
    case MatchOp::LSS:
        return label.name == name && label.value < value;
    case MatchOp::GTR:
        return label.name == name && label.value > value;
    case MatchOp::LTE:
        return label.name == name && label.value <= value;
    case MatchOp::GTE:
        return label.name == name && label.value >= value;
    case MatchOp::EQL_REGEX:
        regex_match = true;
        /* fall-through */
    case MatchOp::NEQ_REGEX: {
        std::regex pattern(value);
        return name == label.name &&
               std::regex_match(label.value, pattern) == regex_match;
    }
    }

    return false;
}

} // namespace promql
