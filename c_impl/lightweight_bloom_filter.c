/*
    Lightweight Locality-Aware, Block-Based Bloom Filter Implementation
    Uses xxHash64 for hashing.
*/

#include "lightweight_bloom_filter.h"
#include "xxHash/xxhash.h"

#include <stdlib.h>
#include <string.h>

#define MIN_SIZE 1

/*
    Function prototypes/declarations
*/
static size_t next_power_of_two(size_t value);
static size_t block_index_from_hash(uint64_t digest, unsigned block_bits, size_t mask);
static uint64_t splitmix64_next(uint64_t *state);
static bool lbf_prepare_block(const LightweightBloomFilter *filter, const char *item, uint64_t *digest, size_t *block_index);

/*
    lbf_init - Initializes the LightweightBloomFilter structure.
    param filter - Pointer to the LightweightBloomFilter structure to initialize.
    param size_bits - Size of the Bloom filter in bits.
    param num_hashes - Number of hash functions to use.
    param seed - Starting seed for the hash function (xxHash64).
    return - true on success, false on failure.
*/
bool lbf_init(LightweightBloomFilter *filter, size_t size_bits, uint32_t num_hashes, uint64_t seed)
{
    if (!filter || size_bits < MIN_SIZE || num_hashes == 0)
    {
        return false;
    }

    if (size_bits > (SIZE_MAX - 63u))
    {
        return false;
    }

    // Calculate the number of 64-bit words needed, rounded up to the next power of two
    size_t requested_words = (size_bits + 63u) / 64u;
    if (requested_words == 0u)
    {
        requested_words = 1u;
    }
    const size_t word_count = next_power_of_two(requested_words);

    // Initialize filter parameters
    filter->size_bits = 0u;
    filter->num_hashes = 0u;
    filter->seed = 0u;
    filter->word_count = 0u;
    filter->word_mask = 0u;
    filter->block_bits = 0u;
    filter->bit_array = NULL;

    // Allocate and zero-initialize the bit array
    uint64_t *array = (uint64_t *)calloc(word_count, sizeof(uint64_t));
    if (!array)
    {
        return false;
    }

    // Set the filter parameters
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

/*
    lbf_free - Helper function to free the LightweightBloomFilter resources.
    param filter - Pointer to the LightweightBloomFilter structure to free.
*/
void lbf_free(LightweightBloomFilter *filter)
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
    lbf_add - Adds an item to the LightweightBloomFilter.
    param filter - Pointer to the LightweightBloomFilter structure.
    param item - The item to add.
*/
void lbf_add(LightweightBloomFilter *filter, const char *item)
{
    uint64_t digest = 0u;
    size_t block_index = 0u;

    if (!lbf_prepare_block(filter, item, &digest, &block_index))
    {
        return;
    }

    // Update the bits in the block
    uint64_t state = digest;
    uint64_t word = filter->bit_array[block_index];

    /*
        For each virtual hash (num_hashes):
        - Derive the next pseudo-random value from 'state' (seeded with 'digest') using splitmix64.
        - Mask it with 63 to obtain a bit position in the range [0, 63] for the 64-bit block.
        - Set the corresponding bit in the block word by OR'ing (this accumulates multiple bits).
     */
    for (uint32_t i = 0; i < filter->num_hashes; ++i)
    {
        uint64_t bit_pos = splitmix64_next(&state) & 63u;
        word |= UINT64_C(1) << bit_pos;
    }

    filter->bit_array[block_index] = word;
}

/*
    lbf_contains - Checks if an item is possibly in the LightweightBloomFilter.
    param filter - Pointer to the LightweightBloomFilter structure.
    param item - The item to check.
    return - true if the item is possibly in the filter, false if definitely not.
*/
bool lbf_contains(const LightweightBloomFilter *filter, const char *item)
{
    uint64_t digest = 0u;
    size_t block_index = 0u;

    if (!lbf_prepare_block(filter, item, &digest, &block_index))
    {
        return false;
    }

    uint64_t state = digest;
    const uint64_t word = filter->bit_array[block_index];

    /*
        For each virtual hash (num_hashes):
        - Derive the next pseudo-random value from 'state' (seeded with 'digest') using splitmix64.
        - Mask it with 63 to obtain a bit position in the range [0, 63] for the 64-bit block.
        - Check if the corresponding bit in the block word is set.
        - If any bit is not set, return false immediately.
        - If all bits are set, return true.
    */
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

/*
    next_power_of_two - Compute next power of two greater than or equal to the given value.
    param value - The input value.
    return - The next power of two.
*/
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
#if SIZE_MAX > UINT32_MAX // 64-bit system compat
    value |= value >> 32u;
#endif
    return value + 1u;
}

/*
    block_index_from_hash - Computes the block index from the hash digest.
    param digest - The hash digest.
    param block_bits - Number of bits used for block indexing.
    param mask - Mask to apply for wrapping the index.
    return - The computed block index.
*/
static size_t block_index_from_hash(uint64_t digest, unsigned block_bits, size_t mask)
{
    if (block_bits == 0u)
    {
        return 0u;
    }
    return (size_t)(digest >> (64u - block_bits)) & mask;
}

/*
    splitmix64_next - Generates the next pseudo-random value using the SplitMix64 algorithm.
    param state - Pointer to the current state.
    return - The next pseudo-random value.
*/
static uint64_t splitmix64_next(uint64_t *state)
{

    // First, we set initialize constants used in the SplitMix64 algorithm
    static const uint64_t gamma = UINT64_C(0x9E3779B97F4A7C15);
    static const uint64_t mul1 = UINT64_C(0xBF58476D1CE4E5B9);
    static const uint64_t mul2 = UINT64_C(0x94D049BB133111EB);

    // Move the internal state forward and scramble it to produce
    // the next pseudo-random 64-bit value. The math below mixes
    // the bits (shifts, XORs, and multiplications) to make the
    // output appear unpredictable.
    uint64_t x = (*state += gamma);
    x = (x ^ (x >> 30u)) * mul1;
    x = (x ^ (x >> 27u)) * mul2;
    x ^= x >> 31u;
    return x;
}

/*
    lbf_prepare_block - Prepares the block index and digest for an item.
    param filter - Pointer to the LightweightBloomFilter structure.
    param item - The item to process.
    param digest - Pointer to store the computed digest.
    param block_index - Pointer to store the computed block index.
    return - true on success, false on failure.
*/
static bool lbf_prepare_block(const LightweightBloomFilter *filter, const char *item,
                              uint64_t *digest, size_t *block_index)
{
    if (!filter || !item || !digest || !block_index || filter->bit_array == NULL || filter->word_count == 0u)
    {
        return false;
    }

    const size_t len = strlen(item);
    const uint64_t hash = XXH64(item, len, filter->seed); // Compute xxHash64 digest

    *digest = hash;
    *block_index = block_index_from_hash(hash, filter->block_bits, filter->word_mask);
    return true;
}
