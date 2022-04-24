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

    struct Bucket {
        std::map<Key, Value> map_bucket;
        std::mutex mutex_bucket;
    };

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(Bucket& bucket, const Key& key)
            : guard(bucket.mutex_bucket), ref_to_value(bucket.map_bucket[key]) {}
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets(bucket_count) {}

    Access operator[](const Key& key) {
        size_t bucket_index = static_cast<size_t>(key) % buckets.size();
        return Access(buckets[bucket_index], key);
    }

    std::map<Key, Value> BuildOrdinaryMap() {

        std::map<Key, Value> result;
        for (size_t i = 0; i < buckets.size(); ++i) {
            std::lock_guard<std::mutex> quard(buckets[i].mutex_bucket);
            result.insert(buckets[i].map_bucket.begin(), buckets[i].map_bucket.end());
        }
        return result;
    }

private:
    std::vector<Bucket> buckets;
 };


