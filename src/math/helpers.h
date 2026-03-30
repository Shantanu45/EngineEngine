#pragma once
#include "math_common.h"
#include <glm/gtc/epsilon.hpp>
#include "glm/gtc/round.hpp"
#include <string>
#include "util/typedefs.h"

namespace math_helpers
{
    _FORCE_INLINE_ glm::vec2 orthogonal(const glm::vec2& v) {
        return glm::vec2(v.y, -v.x);  // or (-v.y, v.x)
    }

    template<typename T>
    bool is_equal_approx(const T& a, const T& b)
    {
        if constexpr (std::is_floating_point_v<T>) {
            return glm::epsilonEqual(a, b, (T)CMP_EPSILON);
        }
        else {
            // For glm::vec2, vec3, etc. - epsilon must also be a vector
            return glm::all(glm::epsilonEqual(a, b, T(CMP_EPSILON)));
        }
    }

    template<typename T>
    bool is_finite(T val)
    {
        return glm::all(glm::isinf(std::forward<T>(val))) || glm::all(glm::isnan(std::forward<T>(val)));
    }

    template<typename Vec>
    std::string vec_to_string(const Vec& v) {
        std::ostringstream oss;
        oss << "(";
        for (int i = 0; i < Vec::length(); ++i) {
            if (i > 0) oss << ", ";
            oss << v[i];
        }
        oss << ")";
        return oss.str();
    }


	template<typename T>
	T nearest_power_of_two(T v)
	{
		T lower = glm::floorPowerOfTwo(v);
		T upper = glm::ceilPowerOfTwo(v);

		return (v - lower < upper - v) ? lower : upper;
	}
}
