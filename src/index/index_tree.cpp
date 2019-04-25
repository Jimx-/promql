#include "index/index_tree.h"
#include "index/index_server.h"
#include "index/page_cache.h"

#include "easylogging++.h"

#include <cassert>
#include <iomanip>

namespace promql {

template <>
void IndexTree::pack_key<uint64_t>(const uint8_t* buf, uint64_t& key)
{
    key = 0;
    for (int i = KEY_WIDTH - 1; i >= 0; i--) {
        key |= ((uint64_t)(*buf++) << (i << 3));
    }
}

template <>
void IndexTree::pack_key<StringKey<IndexTree::KEY_WIDTH>>(
    const uint8_t* buf, StringKey<IndexTree::KEY_WIDTH>& key)
{
    key = StringKey<KEY_WIDTH>(buf);
}

template <> void IndexTree::clear_key<uint64_t>(uint64_t& key) { key = 0; }
template <>
void IndexTree::clear_key<StringKey<IndexTree::KEY_WIDTH>>(
    StringKey<IndexTree::KEY_WIDTH>& key)
{
    key = StringKey<KEY_WIDTH>();
}

IndexTree::IndexTree(IndexServer* server) : next_id(0), server(server)
{
    bwtree = std::make_unique<BwTree>(true);
    bwtree->UpdateThreadLocal(1);
    bwtree->AssignGCID(0);
}

void IndexTree::query_postings(const LabelMatcher& matcher,
                               std::set<PostingID>& postings)
{
    KeyType start_key, end_key, match_key, name_mask;
    uint8_t key_buf[KEY_WIDTH];
    auto op = matcher.op;
    auto name = matcher.name;
    auto value = matcher.value;

    postings.clear();
    clear_key(start_key);
    match_key = make_key(name, value);

    memset(key_buf, 0, sizeof(key_buf));
    memset(key_buf, 0xff, NAME_BYTES);
    pack_key(key_buf, name_mask);

    switch (op) {
    case MatchOp::EQL:
        /* key range: from   | hash(name) | hash(value)     | *
         *              to   | hash(name) | hash(value) + 1 | */
        start_key = make_key(name, value);
        break;
    case MatchOp::NEQ:
        /* key range: from   | hash(name)     | 0 | *
         *              to   | hash(name) + 1 | 0 | */
    case MatchOp::LSS:
        /* key range: from   | hash(name) |      0      | *
         *              to   | hash(name) | hash(value) | */
    case MatchOp::GTR:
        /* key range: from   | hash(name)     | hash(value) | *
         *              to   | hash(name) + 1 |      0      | */
    case MatchOp::LTE:
    case MatchOp::GTE: {
        start_key = make_key(name, value);

        memset(key_buf, 0, sizeof(key_buf));
        key_buf[NAME_BYTES - 1] = 1;
        pack_key(key_buf, end_key);
        end_key = end_key + start_key;

        start_key = start_key & name_mask;
        end_key = end_key & name_mask;

        if (op == MatchOp::LSS || op == MatchOp::LTE) {
            end_key = match_key;
        } else if (op == MatchOp::GTR || op == MatchOp::GTE) {
            start_key = match_key;
        }
        break;
    }
    default:
        break;
    }

    auto it = bwtree->Begin(start_key);
    while (!it.IsEnd()) {
        switch (op) {
        case MatchOp::NEQ:
            if (it->first >= end_key) {
                goto out;
            } else if (it->first == match_key) {
                it++;
                continue;
            }

            break;
        case MatchOp::GTR:
            if (it->first == start_key) {
                it++;
                continue;
            }
            /* fallthrough */
        case MatchOp::LSS:
        case MatchOp::GTE:
            if (it->first >= end_key) {
                goto out;
            }
            break;
        case MatchOp::LTE:
            if (it->first > end_key) {
                goto out;
            }
            break;
        default:
            break;
        }

        auto page_id = it->second;
        auto page = server->get_page_cache()->get_page(page_id);
        uint64_t* p = (uint64_t*)page->lock();
        uint64_t* lim = (uint64_t*)((uint8_t*)p + page->get_size());
        size_t start_index = 0;

        while (p < lim) {
            if (*p > 0) {
                for (uint64_t i = 0; i < 64; i++) {
                    if ((*p) & ((uint64_t)1 << i)) {
                        postings.insert((PostingID)(start_index + i));
                    }
                }
            }

            start_index += 64;
            p++;
        }
        page->unlock();

        if (op == MatchOp::EQL) {
            break;
        }
        it++;
    }

out:
    return;
}

PostingID IndexTree::get_new_id() { return next_id++; }

PostingID IndexTree::add_series(const std::vector<Label>& labels)
{
    auto pid = get_new_id();

    for (auto&& label : labels) {
        insert_label(label, pid);
    }

    return pid;
}

void IndexTree::insert_label(const Label& label, PostingID pid)
{
    auto key = make_key(label.name, label.value);
    insert_posting_id(key, pid);
}

void IndexTree::insert_posting_id(const KeyType& key, PostingID pid)
{
    std::vector<PageID> page_ids;
    bwtree->GetValue(key, page_ids);

    if (page_ids.empty()) {
        /* create page */
        auto page = server->get_page_cache()->create_page();
        auto buf = page->lock();
        memset(buf, 0, page->get_size());
        page->unlock();
        bwtree->Insert(key, page->get_id());
        page_ids.push_back(page->get_id());
    }

    /* update pages */
    for (auto&& p : page_ids) {
        auto page = server->get_page_cache()->get_page(p);
        assert(page != nullptr);

        uint64_t* buf = (uint64_t*)page->lock();
        buf[pid >> 6] |= (uint64_t)1 << (pid & 0x3f);
        page->unlock();
    }
}

IndexTree::KeyType IndexTree::make_key(const std::string& name,
                                       const std::string& value)
{
    uint8_t key_buf[KEY_WIDTH];
    memset(key_buf, 0, sizeof(key_buf));

    _hash_string_name(name, key_buf);
    _hash_string_value(value, &key_buf[NAME_BYTES]);

    KeyType key;
    pack_key(key_buf, key);

    return key;
}

void IndexTree::_hash_string_name(const std::string& str, uint8_t* out)
{
    /* take LSBs of string hash value */
    auto str_hash = std::hash<std::string>()(str);

    for (int j = NAME_BYTES - 1; j >= 0; j--) {
        *out++ = ((uint64_t)str_hash >> (j << 3)) & 0xff;
    }
}

void IndexTree::_hash_string_value(const std::string& str, uint8_t* out)
{
    /* | 4 bytes string prefix | 2 bytes hash value | */
    int padding = VALUE_BYTES - 2 - str.length();

    for (int i = 0; i < ((VALUE_BYTES - 2) > str.length() ? str.length()
                                                          : (VALUE_BYTES - 2));
         i++) {
        *out++ = str[i];
    }
    while (padding-- > 0) {
        *out++ = '\0';
    }

    auto str_hash = std::hash<std::string>()(str);
    *out++ = (str_hash >> 8) & 0xff;
    *out++ = str_hash & 0xff;
}

} // namespace promql
