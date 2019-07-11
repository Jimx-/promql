#ifndef _PROMQL_STORAGE_H_
#define _PROMQL_STORAGE_H_

#include "promql/labels.h"

#include <cstdint>
#include <memory>
#include <unordered_set>

namespace promql {

class SeriesIterator {
public:
    virtual bool seek(uint64_t t) = 0;
    virtual std::pair<uint64_t, double> at() = 0;
    virtual bool next() = 0;
};

class Series {
public:
    virtual void labels(std::vector<Label>& labels) = 0;
    virtual std::unique_ptr<SeriesIterator> iterator() = 0;
};

class SeriesSet {
public:
    virtual bool next() = 0;
    virtual std::shared_ptr<Series> at() = 0;
};

class Querier {
public:
    virtual std::shared_ptr<SeriesSet>
    select(const std::vector<LabelMatcher>& matchers) = 0;
};

class Queryable {
public:
    virtual std::shared_ptr<Querier> querier(uint64_t mint, uint64_t maxt) = 0;
    virtual void label_values(const std::string& name,
                              std::unordered_set<std::string>& values) = 0;
};

class Appender {
public:
    virtual void add(const std::vector<Label>& labels, uint64_t t,
                     double v) = 0;

    virtual void commit() {}
};

class Storage : public Queryable {
public:
    virtual std::shared_ptr<Appender> appender() = 0;
    virtual void close() {}
};

} // namespace promql

#endif
