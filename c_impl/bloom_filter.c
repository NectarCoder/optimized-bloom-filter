/*
    Standard Bloom filter implementation in C.
    Uses double hashing with MurmurHash3 and xxHash64.
*/

#include "bloom_filter.h"
#include "murmurhash.c/murmurhash.h"
#include "xxHash/xxhash.h"

#include <stdlib.h>
#include <string.h>

#define BLOOM_MIN_SIZE 1

/*
    bloom_set_bit - Sets the bit at the specified index in the bit array.
    param array - Pointer to the bit array.
    param bit_index - Index of the bit to set.
*/
static inline void bloom_set_bit(uint8_t *array, size_t bit_index)
{
    // Use bitwise OR to set the specific bit at bit_index
    array[bit_index >> 3] |= (uint8_t)(1u << (bit_index & 7u));
}

/*
    bloom_bit_is_set - Checks if the bit at the specified index in the bit array is set.
    param array - Pointer to the bit array.
    param bit_index - Index of the bit to check.
    return - true if the bit is set, false otherwise.
*/
static inline bool bloom_bit_is_set(const uint8_t *array, size_t bit_index)
{
    // Use bitwise AND to check if the specific bit at bit_index is set 
    return (array[bit_index >> 3] & (uint8_t)(1u << (bit_index & 7u))) != 0;
}

/*
    bloom_compute_hashes - Computes the two hash values for double hashing.
    param filter - Pointer to the BloomFilter structure.
    param item - The item to hash.
    param h1 - Pointer to store the first hash value (MurmurHash3).
    param h2 - Pointer to store the second hash value (xxHash64).
    return - true on success, false on failure.
*/
static bool bloom_compute_hashes(const BloomFilter *filter, const char *item,
                                 uint32_t *h1, uint64_t *h2)
{
    // Validate inputs
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

/*
    bloom_init - Initializes the Bloom filter.
    param filter - Pointer to the BloomFilter structure to initialize.
    param size_bits - Size of the bit array in bits.
    param num_hashes - Number of hash functions to use.
    param seed1 - Seed for the first hash function (MurmurHash3).
    param seed2 - Seed for the second hash function (xxHash64).
    return - true on success, false on failure.
*/
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

    // Set the filter parameters
    filter->size_bits = size_bits;
    filter->num_hashes = num_hashes;
    filter->seed1 = seed1;
    filter->seed2 = seed2;
    filter->bit_array = bits;
    filter->byte_length = byte_length;
    return true;
}

/*
    bloom_free - Helper function to free the Bloom filter resources.
    param filter - Pointer to the BloomFilter structure to free.
*/
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

/*
    bloom_add - Adds an item to the Bloom filter.
    param filter - Pointer to the BloomFilter structure.
    param item - The item to add.
*/
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

/*
    bloom_contains - Checks if an item is possibly in the Bloom filter.
    param filter - Pointer to the BloomFilter structure.
    param item - The item to check.
    return - true if the item is possibly in the filter, false if definitely not.
*/
bool bloom_contains(const BloomFilter *filter, const char *item)
{
    uint32_t h1 = 0u;
    uint64_t h2 = 0u;

    if (!bloom_compute_hashes(filter, item, &h1, &h2))
    {
        return false;
    }

    // Repeat as many times defined by num_hashes (theoretically, the 'k' value)
    for (uint32_t i = 0; i < filter->num_hashes; ++i)
    {
        // Calculate the bit index using double hashing
        size_t bit_index = (h1 + i * h2) % filter->size_bits;
        if (!bloom_bit_is_set(filter->bit_array, bit_index))
        {
            return false;
        }
    }

    return true;
}
