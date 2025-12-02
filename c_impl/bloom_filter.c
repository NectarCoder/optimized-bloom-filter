#include "bloom_filter.h"
#include "hash_utils.h"

#include <stdlib.h>
#include <string.h>

#define BLOOM_MIN_SIZE 1

static inline void bloom_set_bit(uint8_t *array, size_t bit_index) {
    array[bit_index >> 3] |= (uint8_t)(1u << (bit_index & 7u));
}

static inline bool bloom_bit_is_set(const uint8_t *array, size_t bit_index) {
    return (array[bit_index >> 3] & (uint8_t)(1u << (bit_index & 7u))) != 0;
}

bool bloom_init(BloomFilter *filter, size_t size_bits, uint32_t num_hashes,
                uint32_t seed1, uint64_t seed2) {
    if (!filter || size_bits < BLOOM_MIN_SIZE || num_hashes == 0) {
        return false;
    }

    size_t byte_length = (size_bits + 7u) / 8u;
    uint8_t *bits = (uint8_t *)calloc(byte_length, sizeof(uint8_t));
    if (!bits) {
        return false;
    }

    filter->size_bits = size_bits;
    filter->num_hashes = num_hashes;
    filter->seed1 = seed1;
    filter->seed2 = seed2;
    filter->bit_array = bits;
    filter->byte_length = byte_length;
    return true;
}

void bloom_free(BloomFilter *filter) {
    if (!filter) {
        return;
    }
    free(filter->bit_array);
    memset(filter, 0, sizeof(*filter));
}

void bloom_add(BloomFilter *filter, const char *item) {
    if (!filter || !item) {
        return;
    }

    const uint8_t *data = (const uint8_t *)item;
    size_t len = strlen(item);
    uint32_t h1 = murmur3_32(data, len, filter->seed1);
    uint64_t h2 = xxhash64(data, len, filter->seed2) % filter->size_bits;
    if (h2 == 0) {
        h2 = 1;
    }

    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        size_t bit_index = (h1 + i * h2) % filter->size_bits;
        bloom_set_bit(filter->bit_array, bit_index);
    }
}

bool bloom_contains(const BloomFilter *filter, const char *item) {
    if (!filter || !item) {
        return false;
    }

    const uint8_t *data = (const uint8_t *)item;
    size_t len = strlen(item);
    uint32_t h1 = murmur3_32(data, len, filter->seed1);
    uint64_t h2 = xxhash64(data, len, filter->seed2) % filter->size_bits;
    if (h2 == 0) {
        h2 = 1;
    }

    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        size_t bit_index = (h1 + i * h2) % filter->size_bits;
        if (!bloom_bit_is_set(filter->bit_array, bit_index)) {
            return false;
        }
    }

    return true;
}

