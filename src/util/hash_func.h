#pragma once

#include "xxhash.h"
#include <cstdint>

inline uint32_t hash_xxhash_one_32(uint32_t value, uint32_t seed = 0)
{
    // XXH32 wants a pointer + size
    return XXH32(&value, sizeof(value), seed);
}

inline uint32_t hash_xxhash_32(uint32_t* value, size_t size, uint32_t seed = 0)
{
    // XXH32 wants a pointer + size
    return XXH32(value, size, seed);
}
