#ifndef _PROMQL_HTTP_SERVER_H_
#define _PROMQL_HTTP_SERVER_H_

#include "promql/common.h"
#include "promql/storage.h"
#include "promql/value.h"
#include "server_http.hpp"

#include <memory>
#include <unordered_map>

namespace promql {

class HttpServer {
public:
    HttpServer(Storage* storage);

    void start();

private:
    using InternalHttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

    Storage* storage;
    InternalHttpServer server;

    std::string render_template(const std::string& name);
    std::string get_file(const std::string& filename);

    std::unique_ptr<ExecValue> query(const std::string& query_str,
                                     SystemTime start, SystemTime end,
                                     Duration interval);
};

} // namespace promql

#endif
