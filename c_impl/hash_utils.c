#include "hash_utils.h"

#include <string.h>

uint32_t murmur3_32(const uint8_t *data, size_t len, uint32_t seed) {
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    uint32_t h1 = seed;
    size_t i = 0;

    while (i + 4 <= len) {
        uint32_t k1;
        memcpy(&k1, data + i, sizeof(uint32_t));
        i += 4;

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> 17);
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19);
        h1 = h1 * 5 + 0xe6546b64;
    }

    uint32_t k1 = 0;
    const uint8_t *tail = data + i;
    switch (len & 3u) {
        case 3:
            k1 ^= (uint32_t)tail[2] << 16;
            /* fallthrough */
        case 2:
            k1 ^= (uint32_t)tail[1] << 8;
            /* fallthrough */
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = (k1 << 15) | (k1 >> 17);
            k1 *= c2;
            h1 ^= k1;
            break;
        default:
            break;
    }

    h1 ^= (uint32_t)len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

uint64_t xxhash64(const uint8_t *data, size_t len, uint64_t seed) {
    const uint64_t prime1 = 11400714785074694791ULL;
    const uint64_t prime2 = 14029467366897019727ULL;
    const uint64_t prime3 = 1609587929392839161ULL;
    const uint64_t prime4 = 9650029242287828579ULL;
    const uint64_t prime5 = 2870177450012600261ULL;

    uint64_t h64;
    size_t index = 0;

    if (len >= 32) {
        uint64_t v1 = seed + prime1 + prime2;
        uint64_t v2 = seed + prime2;
        uint64_t v3 = seed;
        uint64_t v4 = seed - prime1;

        while (index <= len - 32) {
            uint64_t k1;
            memcpy(&k1, data + index, sizeof(uint64_t));
            index += 8;
            v1 += k1 * prime2;
            v1 = (v1 << 31) | (v1 >> 33);
            v1 *= prime1;

            uint64_t k2;
            memcpy(&k2, data + index, sizeof(uint64_t));
            index += 8;
            v2 += k2 * prime2;
            v2 = (v2 << 31) | (v2 >> 33);
            v2 *= prime1;

            uint64_t k3;
            memcpy(&k3, data + index, sizeof(uint64_t));
            index += 8;
            v3 += k3 * prime2;
            v3 = (v3 << 31) | (v3 >> 33);
            v3 *= prime1;

            uint64_t k4;
            memcpy(&k4, data + index, sizeof(uint64_t));
            index += 8;
            v4 += k4 * prime2;
            v4 = (v4 << 31) | (v4 >> 33);
            v4 *= prime1;
        }

        h64 = ((v1 << 1) | (v1 >> 63)) + ((v2 << 7) | (v2 >> 57)) +
               ((v3 << 12) | (v3 >> 52)) + ((v4 << 18) | (v4 >> 46));

        v1 *= prime2;
        v1 = (v1 << 31) | (v1 >> 33);
        v1 *= prime1;
        h64 ^= v1;
        h64 = h64 * prime1 + prime4;

        v2 *= prime2;
        v2 = (v2 << 31) | (v2 >> 33);
        v2 *= prime1;
        h64 ^= v2;
        h64 = h64 * prime1 + prime4;

        v3 *= prime2;
        v3 = (v3 << 31) | (v3 >> 33);
        v3 *= prime1;
        h64 ^= v3;
        h64 = h64 * prime1 + prime4;

        v4 *= prime2;
        v4 = (v4 << 31) | (v4 >> 33);
        v4 *= prime1;
        h64 ^= v4;
        h64 = h64 * prime1 + prime4;
    } else {
        h64 = seed + prime5;
    }

    h64 += (uint64_t)len;

    while (index + 8 <= len) {
        uint64_t k1;
        memcpy(&k1, data + index, sizeof(uint64_t));
        index += 8;
        k1 *= prime2;
        k1 = (k1 << 31) | (k1 >> 33);
        k1 *= prime1;
        h64 ^= k1;
        h64 = (h64 << 27) | (h64 >> 37);
        h64 = h64 * prime1 + prime4;
    }

    if (index + 4 <= len) {
        uint32_t tmp32;
        memcpy(&tmp32, data + index, sizeof(uint32_t));
        index += 4;
        uint64_t k1 = tmp32;
        k1 *= prime1;
        k1 = (k1 << 23) | (k1 >> 41);
        k1 *= prime2;
        h64 ^= k1;
        h64 = h64 * prime1 + prime4;
    }

    while (index < len) {
        uint64_t k1 = data[index++];
        k1 *= prime5;
        k1 = (k1 << 11) | (k1 >> 53);
        k1 *= prime1;
        h64 ^= k1;
        h64 = (h64 << 11) | (h64 >> 53);
        h64 = h64 * prime1 + prime4;
    }

    h64 ^= h64 >> 33;
    h64 *= prime2;
    h64 ^= h64 >> 29;
    h64 *= prime3;
    h64 ^= h64 >> 32;

    return h64;
}
