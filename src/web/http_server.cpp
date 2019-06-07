#include "web/http_server.h"
#include "common.h"

#include "easylogging++.h"

#include <fstream>
#include <sstream>

namespace promql {

HttpServer::HttpServer(const std::string& dir) : index_server(dir)
{
    server.config.port = 8080;

    server.resource["^/$"]["GET"] =
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
            std::ifstream ifs("index.html");
            std::stringstream ss;

            if (ifs.is_open()) {
                std::string line;
                while (getline(ifs, line)) {
                    ss << line << std::endl;
                }
                ifs.close();
            }

            response->write(ss);
        };

    server.resource["^/insert$"]["POST"] =
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
            std::stringstream ss;

            std::string series, value_str;
            auto query_fields = request->parse_query_string();
            for (auto& field : query_fields) {
                if (field.first == "series") {
                    series = field.second;
                } else if (field.first == "value") {
                    value_str = field.second;
                }
            }

            try {
                SystemTime now = std::chrono::system_clock::now();
                double value = ::strtod(value_str.c_str(), nullptr);
                this->index_server.insert(series, now, value);

                ss << "{\"status\": \"ok\"}";
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
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
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
                auto value =
                    this->index_server.query(query_str, now, now, Duration{1});

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

void HttpServer::start()
{
    LOG(INFO) << "Starting HTTP server...";

    server.start();
}

} // namespace promql
