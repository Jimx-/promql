#ifndef _SERIES_MANAGER_H_
#define _SERIES_MANAGER_H_

#include "common.h"
#include "common/tsid.h"
#include "labels.h"

#include <memory>
#include <unordered_map>

namespace promql {

struct SeriesEntry {
    tsdb::common::TSID tsid;
    std::vector<Label> labels;

    SeriesEntry(const std::vector<Label>& labels) : labels(labels) {}
    SeriesEntry(const tsdb::common::TSID& tsid,
                const std::vector<Label>& labels)
        : tsid(tsid), labels(labels)
    {}
};

class SeriesManager {
public:
    SeriesEntry* add(PostingID pid, const std::vector<Label>& labels);
    SeriesEntry* get(PostingID pid);
    SeriesEntry* get_tsid(const tsdb::common::TSID& tsid);

private:
    std::unordered_map<PostingID, std::unique_ptr<SeriesEntry>> series_map;
    std::unordered_map<tsdb::common::TSID, SeriesEntry*> tsid_map;
};

} // namespace promql

#endif
