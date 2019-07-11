#include "promql/web/http_server.h"

#include <easylogging++.h>

INITIALIZE_EASYLOGGINGPP

int main()
{
    promql::HttpServer server(tmpnam(nullptr));
    server.start();

    return 0;
}
