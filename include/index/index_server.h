#ifndef _INDEX_SERVER_H_
#define _INDEX_SERVER_H_

#include "bptree/page_cache.h"
#include "db/DB.hpp"
#include "index/index_tree.h"
#include "index/series_manager.h"
#include "value.h"

#include <memory>
#include <unordered_map>

namespace promql {

class IndexServer {
public:
    IndexServer(const std::string& dir);

    void insert(const std::string& series, SystemTime timestamp, double value);
    std::unique_ptr<ExecValue> query(const std::string& query_str,
                                     SystemTime start, SystemTime end,
                                     Duration interval);

    bptree::AbstractPageCache* get_page_cache() { return page_cache.get(); }

private:
    std::unique_ptr<bptree::AbstractPageCache> page_cache;
    IndexTree index_tree;
    tsdb::db::DB db;

    PostingID create_series(const std::vector<Label>& labels);
};

} // namespace promql

#endif
