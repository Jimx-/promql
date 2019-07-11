#ifndef _PROMQL_COMMON_H_
#define _PROMQL_COMMON_H_

#include <chrono>
#include <cstdint>

namespace promql {

typedef uint64_t PostingID;

using Duration = std::chrono::milliseconds;
using SystemTime = std::chrono::time_point<std::chrono::system_clock>;

} // namespace promql

#endif
