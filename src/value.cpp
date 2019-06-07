#include "value.h"

#include <iomanip>
#include <sstream>

namespace promql {

std::string ScalarValue::to_json() const
{
    std::stringstream ss;
    ss << "[" << std::fixed << std::setprecision(3) << (t / 1000.0) << ", \""
       << v << '"' << "]";

    return ss.str();
}

std::string VectorValue::to_json() const
{
    std::stringstream ss;
    bool first_series = true;
    ss << "{";
    ss << R"("resultType": "vector", "result": [)";

    for (auto&& s : samples) {
        ss << (first_series ? "" : ", ") << R"({"metric": {)";
        bool first = true;
        for (auto&& m : s.metric) {
            ss << (first ? "" : ", ") << '"' << m.name << "\": \"" << m.value
               << '"';
            first = false;
        }

        ss << "}, \"value\": " << s.value.to_json() << "}";

        first_series = false;
    }
    ss << "]}";

    return ss.str();
}

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
            ss << (first ? "" : ", ") << v.to_json();
            first = false;
        }
        ss << "]}";

        first_series = false;
    }
    ss << "]}";

    return ss.str();
}

} // namespace promql
