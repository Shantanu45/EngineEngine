#pragma once
#include <glm/glm.hpp>
#include <cfloat>

namespace Rendering {

struct AABB {
    glm::vec3 min{ FLT_MAX,  FLT_MAX,  FLT_MAX};
    glm::vec3 max{-FLT_MAX, -FLT_MAX, -FLT_MAX};

    bool valid() const { return min.x <= max.x; }
};

// Works with any primitive range whose elements have .vertices[].position
template<typename PrimRange>
inline AABB compute_aabb(const PrimRange& primitives) {
    AABB result;
    for (const auto& prim : primitives)
        for (const auto& v : prim.vertices) {
            result.min = glm::min(result.min, v.position);
            result.max = glm::max(result.max, v.position);
        }
    return result;
}

// Compute AABB from a single primitive
template<typename Prim>
inline AABB compute_aabb_single(const Prim& prim) {
    AABB result;
    for (const auto& v : prim.vertices) {
        result.min = glm::min(result.min, v.position);
        result.max = glm::max(result.max, v.position);
    }
    return result;
}

// Transform local-space AABB to world space via model matrix
inline AABB transform_aabb(const AABB& aabb, const glm::mat4& m) {
    const glm::vec3& lo = aabb.min;
    const glm::vec3& hi = aabb.max;
    const glm::vec3 corners[8] = {
        {lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z},
        {lo.x, hi.y, lo.z}, {hi.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z},
        {lo.x, hi.y, hi.z}, {hi.x, hi.y, hi.z},
    };
    AABB result;
    for (const auto& c : corners) {
        glm::vec3 wc = glm::vec3(m * glm::vec4(c, 1.0f));
        result.min = glm::min(result.min, wc);
        result.max = glm::max(result.max, wc);
    }
    return result;
}

} // namespace Rendering
