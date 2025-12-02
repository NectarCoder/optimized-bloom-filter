#ifndef LIGHTWEIGHT_BLOOM_FILTER_H
#define LIGHTWEIGHT_BLOOM_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t size_bits;
    uint32_t num_hashes;
    uint64_t seed;
    size_t word_count;
    size_t word_mask;
    unsigned block_bits;
    uint64_t *bit_array;
} LightweightBloomFilter;

bool lbf_init(LightweightBloomFilter *filter, size_t size_bits, uint32_t num_hashes,
              uint64_t seed);
void lbf_free(LightweightBloomFilter *filter);
void lbf_add(LightweightBloomFilter *filter, const char *item);
bool lbf_contains(const LightweightBloomFilter *filter, const char *item);

#ifdef __cplusplus
}
#endif

#endif /* LIGHTWEIGHT_BLOOM_FILTER_H */
