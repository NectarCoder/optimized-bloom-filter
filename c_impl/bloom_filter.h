#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct
    {
        size_t size_bits;
        uint32_t num_hashes;
        uint32_t seed1;
        uint64_t seed2;
        uint8_t *bit_array;
        size_t byte_length;
    } BloomFilter;

    bool bloom_init(BloomFilter *filter, size_t size_bits, uint32_t num_hashes,
                    uint32_t seed1, uint64_t seed2);
    void bloom_free(BloomFilter *filter);
    void bloom_add(BloomFilter *filter, const char *item);
    bool bloom_contains(const BloomFilter *filter, const char *item);

#ifdef __cplusplus
}
#endif

#endif
