#pragma once

#include <cstdint>

/**
 * It is a handle that refers to a specific versioned resource node inside the graph's internal table. It does not hold any GPU data, it does not hold a pointer, 
 * it does not know what format the texture is. It is just a number the graph uses to look things up.
 * Actual data lived in the graph, the `FrameGraphResource` is just your receipt to retrieve it.
 */
using FrameGraphResource = int32_t;
