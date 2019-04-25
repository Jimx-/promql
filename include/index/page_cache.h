#ifndef _PAGE_CACHE_H_
#define _PAGE_CACHE_H_

#include "common.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace promql {

class Page {
public:
    explicit Page(PageID id, uint8_t* buffer, size_t size)
        : id(id), buffer(buffer), size(size)
    {}

    uint8_t* lock()
    {
        mutex.lock();
        return buffer.get();
    }

    void unlock() { mutex.unlock(); }

    PageID get_id() const { return id; }
    size_t get_size() const { return size; }

private:
    PageID id;
    std::unique_ptr<uint8_t[]> buffer;
    size_t size;
    std::mutex mutex;
};

class PageCache {
public:
    PageCache();

    Page* create_page();
    Page* get_page(PageID id);

private:
    static const size_t PAGE_SIZE = 4096;

    std::atomic<PageID> next_id;
    std::unordered_map<PageID, std::unique_ptr<Page>> page_map;

    PageID get_next_id();
};

} // namespace promql

#endif
