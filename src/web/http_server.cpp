#include "promql/web/http_server.h"
#include "inja/inja.hpp"
#include "promql/common.h"
#include "promql/parse/executor.h"
#include "promql/parse/parser.h"

#include <fstream>
#include <sstream>

namespace promql {

HttpServer::HttpServer(Storage* storage, int num_workers)
    : storage(storage), num_workers(num_workers), pool(num_workers)
{
    server.config.port = 9090;

    server.resource["^/graph$"]["GET"] =
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
            try {
                response->write(render_template("graph"));
            } catch (const std::runtime_error& e) {
                std::string resp = e.what();
                *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                          << resp.length() << "\r\n\r\n"
                          << resp;
            }
        };

    server.resource["^/insert$"]["POST"] =
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
            pool.push([this, response, request](int id) {
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

                    Parser parser(series);
                    auto root = parser.parse();
                    VectorSelectorNode* vs =
                        dynamic_cast<VectorSelectorNode*>(root.get());

                    if (!vs) {
                        throw std::runtime_error(
                            "series is not a vector selector");
                    }

                    std::vector<Label> labels;
                    for (auto&& p : vs->get_matchers()) {
                        labels.emplace_back(p.name, p.value);
                    }

                    auto app = this->storage->appender();
                    app->add(labels,
                             std::chrono::duration_cast<Duration>(
                                 now.time_since_epoch())
                                 .count(),
                             value);
                    app->commit();

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
            });
        };

    server.resource
        ["^/api/v1/query$"]
        ["GET"] = [this](std::shared_ptr<InternalHttpServer::Response> response,
                         std::shared_ptr<InternalHttpServer::Request> request) {
        pool.push([this, response, request](int id) {
            std::stringstream ss;

            std::string query_str, time;
            auto query_fields = request->parse_query_string();
            for (auto& field : query_fields) {
                if (field.first == "query") {
                    query_str = field.second;
                } else if (field.first == "time") {
                    time = field.second;
                }
            }

            try {
                SystemTime qt = std::chrono::system_clock::now();
                if (time.length()) {
                    char* endp;
                    double time_ts = ::strtod(time.c_str(), &endp);
                    if (endp != time.c_str() + time.length())
                        throw std::runtime_error("invalid parameter 'time'");

                    qt = SystemTime(
                        std::chrono::milliseconds((uint64_t)(time_ts * 1000)));
                }

                auto value = this->query(query_str, qt, qt, Duration{1});

                ss << "{\"status\": \"success\", \"data\": {\"resultType\": \""
                   << valtype2str(value->type())
                   << "\", \"result\": " << value->to_json() << "}}";
                auto resp = ss.str();

                *response << "HTTP/1.1 200 OK\r\nContent-Length: "
                          << resp.length()
                          << "\r\nContent-Type: application/json\r\n\r\n"
                          << resp;
            } catch (const std::runtime_error& e) {
                ss << "{\"status\": \"error\", \"message\": \"" << e.what()
                   << "\"}";
                auto resp = ss.str();
                *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                          << resp.length() << "\r\n\r\n"
                          << resp;
            }
        });
    };

    server.resource
        ["^/api/v1/query_range$"]
        ["GET"] = [this](std::shared_ptr<InternalHttpServer::Response> response,
                         std::shared_ptr<InternalHttpServer::Request> request) {
        pool.push([this, request, response](int id) {
            std::stringstream ss;

            std::string query_str, start, end, step;
            auto query_fields = request->parse_query_string();
            for (auto& field : query_fields) {
                if (field.first == "query") {
                    query_str = field.second;
                } else if (field.first == "start") {
                    start = field.second;
                } else if (field.first == "end") {
                    end = field.second;
                } else if (field.first == "step") {
                    step = field.second;
                }
            }

            try {
                if (!start.length())
                    throw std::runtime_error("invalid parameter 'start'");
                if (!end.length())
                    throw std::runtime_error("invalid parameter 'end'");
                if (!step.length())
                    throw std::runtime_error("invalid parameter 'step'");

                char* endp;
                double start_ts = ::strtod(start.c_str(), &endp);
                if (!start.length() || endp != start.c_str() + start.length())
                    throw std::runtime_error("invalid parameter 'start'");
                double end_ts = ::strtod(end.c_str(), &endp);
                if (!end.length() || endp != end.c_str() + end.length())
                    throw std::runtime_error("invalid parameter 'end'");
                double step_ts = ::strtod(step.c_str(), &endp);
                if (!step.length())
                    throw std::runtime_error("invalid parameter 'step'");

                Duration step_dur;
                if (endp == step.c_str() + step.length()) {
                    step_dur = std::chrono::seconds((uint64_t)step_ts);
                } else {
                    step_dur = Parser::parse_duration(step);
                }

                SystemTime start_tp(
                    std::chrono::milliseconds((uint64_t)(start_ts * 1000)));
                SystemTime end_tp(
                    std::chrono::milliseconds((uint64_t)(end_ts * 1000)));
                auto value = this->query(query_str, start_tp, end_tp, step_dur);

                ss << "{\"status\": \"success\", \"data\": {\"resultType\": \""
                   << valtype2str(value->type())
                   << "\", \"result\": " << value->to_json() << "}}";
                auto resp = ss.str();

                *response << "HTTP/1.1 200 OK\r\nContent-Length: "
                          << resp.length()
                          << "\r\nContent-Type: application/json\r\n\r\n"
                          << resp;
            } catch (const std::runtime_error& e) {
                ss << "{\"status\": \"error\", \"message\": \"" << e.what()
                   << "\"}";
                auto resp = ss.str();
                *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: "
                          << resp.length() << "\r\n\r\n"
                          << resp;
            }
        });
    };

    server.resource["^/api/v1/label/([^/]+)/values"]["GET"] =
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
            std::string label_name = request->path_match[1];
            std::stringstream ss;

            std::unordered_set<std::string> values;
            this->storage->label_values(label_name, values);

            ss << "{\"status\": \"success\", \"data\": [";
            bool first = true;
            for (auto&& v : values) {
                if (!first) ss << ", ";
                ss << "\"" << v << "\"";
                first = false;
            }
            ss << "]}";

            auto resp = ss.str();

            *response << "HTTP/1.1 200 OK\r\nContent-Length: " << resp.length()
                      << "\r\nContent-Type: application/json\r\n\r\n"
                      << resp;
        };

    server.resource["^/static/(.+)$"]["GET"] =
        [this](std::shared_ptr<InternalHttpServer::Response> response,
               std::shared_ptr<InternalHttpServer::Request> request) {
            try {
                std::string content =
                    get_file("static/" + std::string(request->path_match[1]));

                response->write(content);
            } catch (const std::runtime_error& e) {
                std::string resp = e.what();
                *response << "HTTP/1.1 404 Not Found\r\nContent-Length: "
                          << resp.length() << "\r\n\r\n"
                          << resp;
            }
        };
}

void HttpServer::start() { server.start(); }

std::string HttpServer::render_template(const std::string& name)
{
    std::string content = get_file("templates/" + name + ".html");
    nlohmann::json data;

    data["buildVersion"] = "0.1";
    data["pathPrefix"] = "";
    data["pageTitle"] = name;

    return inja::render(content, data);
}

std::string HttpServer::get_file(const std::string& filename)
{
    std::ifstream ifs(filename);

    if (ifs.is_open()) {
        std::string str((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
        ifs.close();
        return str;
    } else {
        throw std::runtime_error("file '" + filename + "' not found");
    }

    return "";
}

std::unique_ptr<ExecValue> HttpServer::query(const std::string& query_str,
                                             SystemTime start, SystemTime end,
                                             Duration interval)
{
    Parser parser(query_str);
    auto root = parser.parse();

    Executor executor(storage, root.get(), start, end, interval);
    auto value = executor.execute();

    return value;
}

} // namespace promql
