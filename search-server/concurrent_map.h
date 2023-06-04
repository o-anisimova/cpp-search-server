#pragma once
#include <map>
#include <string>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Bucket {
        std::mutex m_;
        std::map<Key, Value> bucket_;
    };

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            :guard(bucket.m_), ref_to_value(bucket.bucket_[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        :segments_(bucket_count) {
    };

    Access operator[](const Key& key) {
        Bucket& bucket = GetBucketRef(key);
        return { key, bucket };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result_map;
        for (Bucket& bucket : segments_) {
            std::lock_guard guard(bucket.m_);
            result_map.insert(bucket.bucket_.begin(), bucket.bucket_.end());
        }
        return result_map;
    }

    void erase(const Key& key) {
        Bucket& bucket = GetBucketRef(key);
        std::lock_guard guard(bucket.m_);
        bucket.bucket_.erase(key);
    }

private:
    std::vector<Bucket> segments_;

    Bucket& GetBucketRef(const Key& key) {
        size_t segment_num = static_cast<uint64_t>(key) % segments_.size();
        return segments_[segment_num];
    }
};