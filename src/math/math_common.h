#pragma once

#include "glm/glm.hpp"

using Color = glm::vec4;
using Point2i = glm::ivec2;
using Point2 = glm::vec2;
using Size2i = glm::ivec2;
using Size2 = glm::vec2;
using Vector2i = glm::ivec2;
using Vector2 = glm::vec2;

enum Side {
	SIDE_LEFT,
	SIDE_TOP,
	SIDE_RIGHT,
	SIDE_BOTTOM
};

#ifdef REAL_T_IS_DOUBLE
typedef double real_t;
#else
typedef float real_t;
#endif

#define CMP_EPSILON 0.00001

