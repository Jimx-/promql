#include "index/page_cache.h"

#include "easylogging++.h"

namespace promql {

PageCache::PageCache() : next_id(1) {}

Page* PageCache::create_page()
{
    auto page_id = get_next_id();
    auto buffer = new uint8_t[PAGE_SIZE];
    auto page = new Page(page_id, buffer, PAGE_SIZE);

    page_map.emplace(page_id, page);
    return page;
}

Page* PageCache::get_page(PageID id)
{
    auto it = page_map.find(id);

    if (it == page_map.end()) return nullptr;
    return it->second.get();
}

PageID PageCache::get_next_id() { return next_id++; }

} // namespace promql
