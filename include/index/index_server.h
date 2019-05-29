#ifndef _INDEX_SERVER_H_
#define _INDEX_SERVER_H_

#include "bptree/page_cache.h"
#include "index/index_tree.h"
#include "value.h"

#include "server_http.hpp"

#include <memory>
#include <unordered_map>

namespace promql {

class IndexServer {
public:
    IndexServer();

    void start();

    PostingID add(const std::string& series);
    std::unique_ptr<ExecValue> query(const std::string& query_str,
                                     SystemTime start, SystemTime end,
                                     Duration interval);

    bptree::AbstractPageCache* get_page_cache() { return page_cache.get(); }

private:
    using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

    HttpServer server;
    std::unique_ptr<bptree::AbstractPageCache> page_cache;
    IndexTree index_tree;
};

} // namespace promql

#endif
