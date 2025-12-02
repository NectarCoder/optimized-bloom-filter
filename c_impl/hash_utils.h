#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include <stddef.h>
#include <stdint.h>

uint32_t murmur3_32(const uint8_t *data, size_t len, uint32_t seed);
uint64_t xxhash64(const uint8_t *data, size_t len, uint64_t seed);

#endif /* HASH_UTILS_H */
