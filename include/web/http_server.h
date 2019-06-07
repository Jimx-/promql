#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include "index/index_server.h"

#include "server_http.hpp"

#include <memory>
#include <unordered_map>

namespace promql {

class HttpServer {
public:
    HttpServer(const std::string& dir);

    void start();

private:
    using InternalHttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

    IndexServer index_server;
    InternalHttpServer server;
};

} // namespace promql

#endif
