#include "index/index_server.h"
#include "bptree/mem_page_cache.h"
#include "parse/executor.h"
#include "parse/parser.h"
#include "parse/printer.h"

#include "easylogging++.h"

#include <sstream>

namespace promql {

IndexServer::IndexServer(const std::string& dir)
    : page_cache(std::make_unique<bptree::MemPageCache>(4096)),
      index_tree(this), db(dir + "/db")
{}

void IndexServer::insert(const std::string& series, SystemTime timestamp,
                         double value)
{
    Parser parser(series);
    auto root = parser.parse();
    VectorSelectorNode* vs = dynamic_cast<VectorSelectorNode*>(root.get());

    if (!vs) {
        throw std::runtime_error("series is not a vector selector");
    }

    std::vector<Label> labels;
    std::vector<LabelMatcher> matchers;
    for (auto&& p : vs->get_matchers()) {
        labels.emplace_back(p.name, p.value);
        matchers.push_back(p);
    }

    std::unordered_set<tsdb::common::TSID> tsids;
    index_tree.resolve_label_matchers(matchers, tsids);

    if (tsids.size() > 1) {
        throw std::runtime_error("series is not unique");
    }

    if (tsids.empty()) {
        auto tsid = index_tree.add_series(labels);
        tsids.insert(tsid);
    }

    auto appender = db.appender();
    for (auto&& p : tsids) {
        appender->add(
            p,
            std::chrono::duration_cast<Duration>(timestamp.time_since_epoch())
                .count(),
            value);
    }
    appender->commit();
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

    Executor executor(&index_tree, &db, root.get(), start, end, interval);
    auto value = executor.execute();

    return value;
}

} // namespace promql
