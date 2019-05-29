#include "index/index_server.h"

#include <easylogging++.h>

INITIALIZE_EASYLOGGINGPP

int main()
{
    promql::IndexServer server;

    server.start();

    return 0;
}
