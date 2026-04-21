#pragma once

#include "xxhash.h"
#include <vector>
#include <string>
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

inline uint64_t hash_xxhash_one_64(uint64_t value, uint64_t seed = 0)
{
	// XXH32 wants a pointer + size
	return XXH64(&value, sizeof(value), seed);
}

inline uint32_t hash_xxhash_strings_32(const std::vector<std::string>& vec)
{
	XXH32_state_t* state = XXH32_createState();
	XXH32_reset(state, 0); // seed = 0

	for (const auto& str : vec)
	{
		XXH32_update(state, str.data(), str.size());

		// Optional: add separator to avoid collisions like ["ab","c"] vs ["a","bc"]
		//const char sep = '\0';
		//XXH32_update(state, &sep, sizeof(sep));
	}

	uint32_t hash = XXH32_digest(state);
	XXH32_freeState(state);
	return hash;
}
