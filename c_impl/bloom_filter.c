#include "bloom_filter.h"
#include "murmurhash.c/murmurhash.h"
#include "xxHash/xxhash.h"

#include <stdlib.h>
#include <string.h>

#define BLOOM_MIN_SIZE 1

static inline void bloom_set_bit(uint8_t *array, size_t bit_index)
{
    array[bit_index >> 3] |= (uint8_t)(1u << (bit_index & 7u));
}

static inline bool bloom_bit_is_set(const uint8_t *array, size_t bit_index)
{
    return (array[bit_index >> 3] & (uint8_t)(1u << (bit_index & 7u))) != 0;
}

static bool bloom_compute_hashes(const BloomFilter *filter, const char *item,
                                 uint32_t *h1, uint64_t *h2)
{
    if (!filter || !item || !h1 || !h2 || filter->size_bits == 0u || filter->bit_array == NULL)
    {
        return false;
    }

    const uint8_t *data = (const uint8_t *)item;
    const size_t len = strlen(item);
    const uint32_t primary = murmurhash((const char *)data, (uint32_t)len, filter->seed1);
    uint64_t secondary = XXH64(data, len, filter->seed2) % filter->size_bits;
    if (secondary == 0u)
    {
        secondary = 1u; /* ensure non-zero stride for double hashing */
    }

    *h1 = primary;
    *h2 = secondary;
    return true;
}

bool bloom_init(BloomFilter *filter, size_t size_bits, uint32_t num_hashes,
                uint32_t seed1, uint64_t seed2)
{
    if (!filter || size_bits < BLOOM_MIN_SIZE || num_hashes == 0)
    {
        return false;
    }

    if (size_bits > (SIZE_MAX - 7u))
    {
        return false;
    }

    const size_t byte_length = (size_bits + 7u) / 8u;
    filter->bit_array = NULL;
    filter->size_bits = 0u;
    filter->num_hashes = 0u;
    filter->seed1 = 0u;
    filter->seed2 = 0u;
    filter->byte_length = 0u;

    uint8_t *bits = (uint8_t *)calloc(byte_length, sizeof(uint8_t));
    if (!bits)
    {
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

void bloom_free(BloomFilter *filter)
{
    if (!filter)
    {
        return;
    }
    free(filter->bit_array);
    filter->bit_array = NULL;
    memset(filter, 0, sizeof(*filter));
}

void bloom_add(BloomFilter *filter, const char *item)
{
    uint32_t h1 = 0u;
    uint64_t h2 = 0u;

    if (!bloom_compute_hashes(filter, item, &h1, &h2))
    {
        return;
    }

    for (uint32_t i = 0; i < filter->num_hashes; ++i)
    {
        size_t bit_index = (h1 + i * h2) % filter->size_bits;
        bloom_set_bit(filter->bit_array, bit_index);
    }
}

bool bloom_contains(const BloomFilter *filter, const char *item)
{
    uint32_t h1 = 0u;
    uint64_t h2 = 0u;

    if (!bloom_compute_hashes(filter, item, &h1, &h2))
    {
        return false;
    }

    for (uint32_t i = 0; i < filter->num_hashes; ++i)
    {
        size_t bit_index = (h1 + i * h2) % filter->size_bits;
        if (!bloom_bit_is_set(filter->bit_array, bit_index))
        {
            return false;
        }
    }

    return true;
}
