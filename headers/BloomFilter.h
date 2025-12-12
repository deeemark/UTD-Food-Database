#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <cstdint>
#include <cstring>
#include <functional>

// Bloom filter parameters
static const int BLOOM_BITS = 256;
static const int BLOOM_BYTES = BLOOM_BITS / 8;

class BloomFilter {
public:
    uint8_t bits[BLOOM_BYTES];

    BloomFilter() {
        clear();
    }

    // Reset all bits
    void clear() {
        std::memset(bits, 0, BLOOM_BYTES);
    }

    // Insert an integer key into the filter
    void add(int key) {
        std::size_t h1 = hash1(key) % BLOOM_BITS;
        std::size_t h2 = hash2(key) % BLOOM_BITS;

        bits[h1 / 8] |= uint8_t(1u << (h1 % 8));
        bits[h2 / 8] |= uint8_t(1u << (h2 % 8));
    }

    // Check if key is possibly present (may have false positives, never false negatives)
    bool possiblyContains(int key) const {
        std::size_t h1 = hash1(key) % BLOOM_BITS;
        std::size_t h2 = hash2(key) % BLOOM_BITS;

        bool b1 = (bits[h1 / 8] & uint8_t(1u << (h1 % 8))) != 0;
        bool b2 = (bits[h2 / 8] & uint8_t(1u << (h2 % 8))) != 0;

        return b1 && b2;
    }

private:
    static inline std::size_t hash1(int key) {
        return std::hash<int>()(key);
    }
    //hash 2 uses the golden ratio constant
    static inline std::size_t hash2(int key) {
        return static_cast<std::size_t>(key) * 0x9e3779b97f4a7c15ULL;
    }
};

#endif
