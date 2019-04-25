#ifndef _COMMON_H_
#define _COMMON_H_

#include <chrono>
#include <cstdint>

namespace promql {

typedef uint64_t PostingID;
typedef uint64_t PageID;

using SystemTime = std::chrono::time_point<std::chrono::system_clock>;
using Duration = std::chrono::milliseconds;

} // namespace promql

#endif
