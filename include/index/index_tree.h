#ifndef _INDEX_TREE_H_
#define _INDEX_TREE_H_

#include "bptree/tree.h"
#include "common.h"
#include "labels.h"
#include "string_key.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <set>
#include <type_traits>
#include <vector>

namespace promql {

class IndexServer;

/* use 64-bit unsigned integer as key when possible */
template <size_t N, typename Enable = void> struct KeyTypeSelector;
template <size_t N>
struct KeyTypeSelector<N,
                       typename std::enable_if<N <= sizeof(uint64_t)>::type> {
    typedef uint64_t key_type;
};

template <size_t N>
struct KeyTypeSelector<
    N, typename std::enable_if<!(N <= sizeof(uint64_t))>::type> {
    typedef StringKey<N> key_type;
};

class IndexTree {
public:
    IndexTree(IndexServer* server);

    PostingID add_series(const std::vector<Label>& labels);
    void query_postings(const LabelMatcher& matcher,
                        std::set<PostingID>& posting_ids);

private:
    static const size_t NAME_BYTES = 4;
    static const size_t VALUE_BYTES = 4;
    static constexpr size_t KEY_WIDTH = NAME_BYTES + VALUE_BYTES;

    // using KeyType = KeyTypeSelector<KEY_WIDTH>::key_type;
    using KeyType = std::conditional<KEY_WIDTH <= sizeof(uint64_t), uint64_t,
                                     StringKey<KEY_WIDTH>>::type;
    using BPTree = bptree::BTree<200, KeyType, PageID>;

    IndexServer* server;
    std::atomic<PostingID> next_id;
    std::mutex tree_mutex;
    std::unique_ptr<BPTree> btree;

    PostingID get_new_id();

    void insert_label(const Label& label, PostingID pid);
    void insert_posting_id(const KeyType& key, PostingID pid);

    KeyType make_key(const std::string& name, const std::string& value);

    void _hash_string_name(const std::string& str, uint8_t* out);
    void _hash_string_value(const std::string& str, uint8_t* out);

    template <typename K> void pack_key(const uint8_t* key_buf, K& key);
    template <typename K> void clear_key(K& key);
};

} // namespace promql

#endif
