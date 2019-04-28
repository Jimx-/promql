#include "index/index_server.h"
#include "index/page_cache.h"
#include "parse/executor.h"
#include "parse/parser.h"
#include "parse/printer.h"

#include "easylogging++.h"

#include <sstream>

namespace promql {

IndexServer::IndexServer() : index_tree(this)
{
    server.config.port = 8080;

    server.resource["^/add$"]["POST"] =
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
            auto content = request->content.string();
            std::stringstream ss;

            try {
                auto pid = this->add(content);

                LOG(INFO) << "inserted " << content << " => " << pid;
                ss << "{\"status\": \"ok\", \"id\": " << pid << "}";
                response->write(ss);
            } catch (const std::runtime_error& e) {
                ss << "{\"status\": \"error\", \"message\": \"" << e.what()
                   << "\"}";
                auto resp = ss.str();
                *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                          << resp.length() << "\r\n\r\n"
                          << resp;
            }
        };

    server.resource["^/query$"]["GET"] =
        [this](std::shared_ptr<HttpServer::Response> response,
               std::shared_ptr<HttpServer::Request> request) {
            std::stringstream ss;

            std::string query_str;
            auto query_fields = request->parse_query_string();
            for (auto& field : query_fields) {
                if (field.first == "query") {
                    query_str = field.second;
                }
            }

            try {
                SystemTime now = std::chrono::system_clock::now();
                auto value = this->query(query_str, now, now, Duration{1});

                ss << "{\"status\": \"ok\", \"data\": " << value->to_json()
                   << "}";

                response->write(ss);
            } catch (const std::runtime_error& e) {
                ss << "{\"status\": \"error\", \"message\": \"" << e.what()
                   << "\"}";
                auto resp = ss.str();
                *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                          << resp.length() << "\r\n\r\n"
                          << resp;
            }
        };
}

void IndexServer::start()
{
    LOG(INFO) << "Starting HTTP server...";

    server.start();
}

PostingID IndexServer::add(const std::string& series)
{
    Parser parser(series);
    auto root = parser.parse();
    VectorSelectorNode* vs = dynamic_cast<VectorSelectorNode*>(root.get());

    if (!vs) {
        throw std::runtime_error("series is not a vector selector");
    }

    std::vector<Label> labels;
    for (auto&& p : vs->get_matchers()) {
        labels.emplace_back(p.name, p.value);
    }

    return index_tree.add_series(labels);
}

std::unique_ptr<ExecValue> IndexServer::query(const std::string& query_str,
                                              SystemTime start, SystemTime end,
                                              Duration interval)
{
    Parser parser(query_str);
    auto root = parser.parse();

    LOG(INFO) << "AST:";
    ASTPrinter printer;
    root->visit(printer);
    LOG(INFO) << "==============================";

    Executor executor(&index_tree, root.get(), start, end, interval);
    LOG(INFO) << "Execution:";
    auto value = executor.execute();
    LOG(INFO) << "==============================";

    return value;
}

} // namespace promql
