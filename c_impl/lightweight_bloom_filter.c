#include "lightweight_bloom_filter.h"
#include "xxHash/xxhash.h"

#include <stdlib.h>
#include <string.h>

#define MIN_SIZE 1

static size_t next_power_of_two(size_t value);
static size_t block_index_from_hash(uint64_t digest, unsigned block_bits, size_t mask);
static uint64_t splitmix64_next(uint64_t *state);

bool lbf_init(LightweightBloomFilter *filter, size_t size_bits, uint32_t num_hashes,
              uint64_t seed)
{
    if (!filter || size_bits < MIN_SIZE || num_hashes == 0)
    {
        return false;
    }

    size_t requested_words = (size_bits + 63u) / 64u;
    if (requested_words == 0)
    {
        requested_words = 1;
    }
    size_t word_count = next_power_of_two(requested_words);
    uint64_t *array = (uint64_t *)calloc(word_count, sizeof(uint64_t));
    if (!array)
    {
        return false;
    }

    filter->size_bits = word_count * 64u;
    filter->num_hashes = num_hashes;
    filter->seed = seed;
    filter->word_count = word_count;
    filter->word_mask = word_count - 1u;
    unsigned bits = 0;
    size_t tmp = word_count;
    while (tmp > 1u)
    {
        ++bits;
        tmp >>= 1u;
    }
    filter->block_bits = bits;
    filter->bit_array = array;
    return true;
}

void lbf_free(LightweightBloomFilter *filter)
{
    if (!filter)
    {
        return;
    }
    free(filter->bit_array);
    memset(filter, 0, sizeof(*filter));
}

void lbf_add(LightweightBloomFilter *filter, const char *item)
{
    if (!filter || !item)
    {
        return;
    }

    size_t len = strlen(item);
    uint64_t digest = XXH64(item, len, filter->seed);
    size_t block_index = block_index_from_hash(digest, filter->block_bits, filter->word_mask);
    uint64_t state = digest;
    uint64_t word = filter->bit_array[block_index];

    for (uint32_t i = 0; i < filter->num_hashes; ++i)
    {
        uint64_t bit_pos = splitmix64_next(&state) & 63u;
        word |= UINT64_C(1) << bit_pos;
    }

    filter->bit_array[block_index] = word;
}

bool lbf_contains(const LightweightBloomFilter *filter, const char *item)
{
    if (!filter || !item)
    {
        return false;
    }

    size_t len = strlen(item);
    uint64_t digest = XXH64(item, len, filter->seed);
    size_t block_index = block_index_from_hash(digest, filter->block_bits, filter->word_mask);
    uint64_t state = digest;
    uint64_t word = filter->bit_array[block_index];

    for (uint32_t i = 0; i < filter->num_hashes; ++i)
    {
        uint64_t bit_pos = splitmix64_next(&state) & 63u;
        if ((word & (UINT64_C(1) << bit_pos)) == 0)
        {
            return false;
        }
    }

    return true;
}

static size_t next_power_of_two(size_t value)
{
    if (value <= 1u)
    {
        return 1u;
    }
    --value;
    value |= value >> 1u;
    value |= value >> 2u;
    value |= value >> 4u;
    value |= value >> 8u;
    value |= value >> 16u;
#if SIZE_MAX > UINT32_MAX
    value |= value >> 32u;
#endif
    return value + 1u;
}

static size_t block_index_from_hash(uint64_t digest, unsigned block_bits, size_t mask)
{
    if (block_bits == 0u)
    {
        return 0u;
    }
    return (size_t)(digest >> (64u - block_bits)) & mask;
}

static uint64_t splitmix64_next(uint64_t *state)
{
    static const uint64_t gamma = UINT64_C(0x9E3779B97F4A7C15);
    static const uint64_t mul1 = UINT64_C(0xBF58476D1CE4E5B9);
    static const uint64_t mul2 = UINT64_C(0x94D049BB133111EB);

    uint64_t x = (*state += gamma);
    x = (x ^ (x >> 30u)) * mul1;
    x = (x ^ (x >> 27u)) * mul2;
    x ^= x >> 31u;
    return x;
}
