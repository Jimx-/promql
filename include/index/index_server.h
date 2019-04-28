#ifndef _INDEX_SERVER_H_
#define _INDEX_SERVER_H_

#include "index/index_tree.h"
#include "index/page_cache.h"
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

    PageCache* get_page_cache() { return &page_cache; }

private:
    using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

    HttpServer server;
    PageCache page_cache;
    IndexTree index_tree;
};

} // namespace promql

#endif
