#pragma once
#include <vector>
#include <glm/glm.hpp>

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

    const std::vector<DebugVertex>& vertices() const { return verts; }
    void     clear()                                  { verts.clear(); }
    bool     empty()                            const { return verts.empty(); }

private:
    DebugDraw() = default;
    std::vector<DebugVertex> verts;
};

} // namespace Rendering
