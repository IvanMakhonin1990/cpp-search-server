#pragma once

#include <algorithm>
#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>
#include <string>


template <typename Key, typename Value> class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>,
        "ConcurrentMap supports only integer keys");

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(std::mutex& mut, const size_t& index, const Key& key,
            std::vector<std::map<Key, Value>>& values)
            : guard(mut), ref_to_value(values[index][key]) {}
    };

    explicit ConcurrentMap(size_t bucket_count)
        : map_buckets_(bucket_count), mutex_buckets_(bucket_count) {}

    Access operator[](const Key& key) {
        size_t bucket_index = static_cast<size_t>(key) % map_buckets_.size();
        return Access(mutex_buckets_[bucket_index], bucket_index, key, map_buckets_);
    }

    std::map<Key, Value> BuildOrdinaryMap() {

        std::map<Key, Value> result;
        for (size_t i = 0; i < map_buckets_.size(); ++i) {
            std::lock_guard<std::mutex> quard(mutex_buckets_[i]);
            result.insert(map_buckets_[i].begin(), map_buckets_[i].end());
        }
        return result;
    }

private:
    std::mutex m;
    std::vector<std::map<Key, Value>> map_buckets_;
    std::vector<std::mutex> mutex_buckets_;
};


