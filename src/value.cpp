#include "value.h"

#include <iomanip>
#include <sstream>

namespace promql {

std::string MatrixValue::to_json() const
{
    std::stringstream ss;
    bool first_series = true;
    ss << "{";
    ss << R"("resultType": "matrix", "result": [)";

    for (auto&& s : series) {
        ss << (first_series ? "" : ", ") << R"({"metric": {)";
        bool first = true;
        for (auto&& m : s.metric) {
            ss << (first ? "" : ", ") << '"' << m.name << "\": \"" << m.value
               << '"';
            first = false;
        }

        ss << "}, \"values\": [";
        first = true;
        for (auto&& v : s.values) {
            ss << (first ? "" : ", ") << "[" << std::fixed
               << std::setprecision(3) << (v.get_time() / 1000.0) << ", \""
               << v.get_value() << '"' << "]";
            first = false;
        }
        ss << "]}";

        first_series = false;
    }
    ss << "]}";

    return ss.str();
}

} // namespace promql
