#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "util/small_vector.h"

namespace Rendering {

struct DebugVertex {
    glm::vec3 pos;
    glm::vec4 color;
};

class DebugDraw {
public:
    static DebugDraw& get() { static DebugDraw inst; return inst; }

    void add_line(glm::vec3 a, glm::vec3 b, glm::vec4 color = { 1, 1, 0, 1 }) {
        verts.push_back({ a, color });
        verts.push_back({ b, color });
    }

    void add_aabb(glm::vec3 mn, glm::vec3 mx, glm::vec4 color = { 1, 1, 0, 1 }) {
        // 12 edges of a box
        auto l = [&](glm::vec3 a, glm::vec3 b) { add_line(a, b, color); };
        l({mn.x,mn.y,mn.z}, {mx.x,mn.y,mn.z});
        l({mn.x,mn.y,mn.z}, {mn.x,mx.y,mn.z});
        l({mn.x,mn.y,mn.z}, {mn.x,mn.y,mx.z});
        l({mx.x,mn.y,mn.z}, {mx.x,mx.y,mn.z});
        l({mx.x,mn.y,mn.z}, {mx.x,mn.y,mx.z});
        l({mn.x,mx.y,mn.z}, {mx.x,mx.y,mn.z});
        l({mn.x,mx.y,mn.z}, {mn.x,mx.y,mx.z});
        l({mn.x,mn.y,mx.z}, {mx.x,mn.y,mx.z});
        l({mn.x,mn.y,mx.z}, {mn.x,mx.y,mx.z});
        l({mx.x,mx.y,mn.z}, {mx.x,mx.y,mx.z});
        l({mx.x,mn.y,mx.z}, {mx.x,mx.y,mx.z});
        l({mn.x,mx.y,mx.z}, {mx.x,mx.y,mx.z});
    }

    void add_frustum(const glm::mat4& view_projection, glm::vec4 color = { 0, 1, 1, 1 }) {
        const glm::mat4 inv = glm::inverse(view_projection);
        glm::vec3 corners[8];
        const glm::vec3 ndc[8] = {
            {-1.0f, -1.0f, 0.0f}, { 1.0f, -1.0f, 0.0f},
            { 1.0f,  1.0f, 0.0f}, {-1.0f,  1.0f, 0.0f},
            {-1.0f, -1.0f, 1.0f}, { 1.0f, -1.0f, 1.0f},
            { 1.0f,  1.0f, 1.0f}, {-1.0f,  1.0f, 1.0f},
        };

        for (int i = 0; i < 8; ++i) {
            const glm::vec4 world = inv * glm::vec4(ndc[i], 1.0f);
            corners[i] = glm::vec3(world) / world.w;
        }

        auto l = [&](int a, int b) { add_line(corners[a], corners[b], color); };
        l(0, 1); l(1, 2); l(2, 3); l(3, 0);
        l(4, 5); l(5, 6); l(6, 7); l(7, 4);
        l(0, 4); l(1, 5); l(2, 6); l(3, 7);
    }

    const Util::SmallVector<DebugVertex>& vertices() const { return verts; }
    void     clear()                                  { verts.clear(); }
    bool     empty()                            const { return verts.empty(); }

private:
    DebugDraw() = default;
    Util::SmallVector<DebugVertex> verts;
};

} // namespace Rendering
