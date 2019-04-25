#include "index/index_server.h"
#include "index/page_cache.h"

#include <easylogging++.h>

INITIALIZE_EASYLOGGINGPP

extern bool wangziqi2013::bwtree::print_flag;

int main()
{
    wangziqi2013::bwtree::print_flag = false;
    promql::IndexServer server;

    server.start();

    return 0;
}
