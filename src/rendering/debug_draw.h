#pragma once
#include <cmath>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    void add_basis(const glm::mat4& transform, float scale = 1.0f) {
        const glm::vec3 origin = glm::vec3(transform[3]);
        add_line(origin, origin + glm::normalize(glm::vec3(transform[0])) * scale, { 1, 0, 0, 1 });
        add_line(origin, origin + glm::normalize(glm::vec3(transform[1])) * scale, { 0, 1, 0, 1 });
        add_line(origin, origin + glm::normalize(glm::vec3(transform[2])) * scale, { 0, 0.35f, 1, 1 });
    }

    void add_arrow(glm::vec3 start, glm::vec3 end, glm::vec4 color = { 1, 1, 0, 1 }, float head_size = 0.15f) {
        add_line(start, end, color);

        const glm::vec3 dir = end - start;
        const float len = glm::length(dir);
        if (len <= 0.0001f)
            return;

        const glm::vec3 forward = dir / len;
        const glm::vec3 basis_up = std::abs(forward.y) < 0.95f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 right = glm::normalize(glm::cross(forward, basis_up));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        const float size = glm::min(head_size, len * 0.35f);
        const glm::vec3 base = end - forward * size;

        add_line(end, base + right * size * 0.5f, color);
        add_line(end, base - right * size * 0.5f, color);
        add_line(end, base + up * size * 0.5f, color);
        add_line(end, base - up * size * 0.5f, color);
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

    void add_obb(glm::vec3 center,
                 glm::vec3 axis_x,
                 glm::vec3 axis_y,
                 glm::vec3 axis_z,
                 glm::vec3 half_extents,
                 glm::vec4 color = { 1, 1, 0, 1 }) {
        axis_x = glm::normalize(axis_x) * half_extents.x;
        axis_y = glm::normalize(axis_y) * half_extents.y;
        axis_z = glm::normalize(axis_z) * half_extents.z;

        glm::vec3 c[8] = {
            center - axis_x - axis_y - axis_z,
            center + axis_x - axis_y - axis_z,
            center + axis_x + axis_y - axis_z,
            center - axis_x + axis_y - axis_z,
            center - axis_x - axis_y + axis_z,
            center + axis_x - axis_y + axis_z,
            center + axis_x + axis_y + axis_z,
            center - axis_x + axis_y + axis_z,
        };

        auto l = [&](int a, int b) { add_line(c[a], c[b], color); };
        l(0, 1); l(1, 2); l(2, 3); l(3, 0);
        l(4, 5); l(5, 6); l(6, 7); l(7, 4);
        l(0, 4); l(1, 5); l(2, 6); l(3, 7);
    }

    void add_rectangle(glm::vec3 center,
                       glm::vec3 axis_x,
                       glm::vec3 axis_y,
                       glm::vec2 half_extents,
                       glm::vec4 color = { 1, 1, 0, 1 }) {
        axis_x = glm::normalize(axis_x) * half_extents.x;
        axis_y = glm::normalize(axis_y) * half_extents.y;

        const glm::vec3 c0 = center - axis_x - axis_y;
        const glm::vec3 c1 = center + axis_x - axis_y;
        const glm::vec3 c2 = center + axis_x + axis_y;
        const glm::vec3 c3 = center - axis_x + axis_y;
        add_line(c0, c1, color);
        add_line(c1, c2, color);
        add_line(c2, c3, color);
        add_line(c3, c0, color);
    }

    void add_circle(glm::vec3 center,
                    glm::vec3 normal,
                    float radius,
                    glm::vec4 color = { 1, 1, 0, 1 },
                    int segments = 48) {
        if (radius <= 0.0f || segments < 3)
            return;

        normal = glm::normalize(normal);
        const glm::vec3 basis = std::abs(normal.y) < 0.95f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(basis, normal));
        const glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));

        glm::vec3 prev = center + tangent * radius;
        constexpr float two_pi = 6.28318530717958647692f;
        for (int i = 1; i <= segments; ++i) {
            const float a = two_pi * static_cast<float>(i) / static_cast<float>(segments);
            const glm::vec3 next = center + (std::cos(a) * tangent + std::sin(a) * bitangent) * radius;
            add_line(prev, next, color);
            prev = next;
        }
    }

    void add_sphere(glm::vec3 center,
                    float radius,
                    glm::vec4 color = { 1, 1, 0, 1 },
                    int segments = 48) {
        add_circle(center, { 1, 0, 0 }, radius, color, segments);
        add_circle(center, { 0, 1, 0 }, radius, color, segments);
        add_circle(center, { 0, 0, 1 }, radius, color, segments);
    }

    void add_cylinder(glm::vec3 center,
                      glm::vec3 axis,
                      float radius,
                      float height,
                      glm::vec4 color = { 1, 1, 0, 1 },
                      int segments = 32) {
        if (radius <= 0.0f || height <= 0.0f || segments < 3)
            return;

        axis = glm::normalize(axis);
        const glm::vec3 half_axis = axis * (height * 0.5f);
        const glm::vec3 top = center + half_axis;
        const glm::vec3 bottom = center - half_axis;
        add_circle(top, axis, radius, color, segments);
        add_circle(bottom, axis, radius, color, segments);

        const glm::vec3 basis = std::abs(axis.y) < 0.95f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(basis, axis));
        const glm::vec3 bitangent = glm::normalize(glm::cross(axis, tangent));
        constexpr float two_pi = 6.28318530717958647692f;
        for (int i = 0; i < 4; ++i) {
            const float a = two_pi * static_cast<float>(i) / 4.0f;
            const glm::vec3 offset = (std::cos(a) * tangent + std::sin(a) * bitangent) * radius;
            add_line(bottom + offset, top + offset, color);
        }
    }

    void add_cone(glm::vec3 apex,
                  glm::vec3 direction,
                  float angle_radians,
                  float length,
                  glm::vec4 color = { 1, 1, 0, 1 },
                  int segments = 32) {
        if (length <= 0.0f || angle_radians <= 0.0f || segments < 3)
            return;

        direction = glm::normalize(direction);
        const glm::vec3 base_center = apex + direction * length;
        const float radius = std::tan(angle_radians) * length;
        add_circle(base_center, direction, radius, color, segments);

        const glm::vec3 basis = std::abs(direction.y) < 0.95f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 tangent = glm::normalize(glm::cross(basis, direction));
        const glm::vec3 bitangent = glm::normalize(glm::cross(direction, tangent));
        constexpr float two_pi = 6.28318530717958647692f;
        for (int i = 0; i < 4; ++i) {
            const float a = two_pi * static_cast<float>(i) / 4.0f;
            const glm::vec3 offset = (std::cos(a) * tangent + std::sin(a) * bitangent) * radius;
            add_line(apex, base_center + offset, color);
        }
    }

    void add_capsule(glm::vec3 center,
                     glm::vec3 axis,
                     float radius,
                     float height,
                     glm::vec4 color = { 1, 1, 0, 1 },
                     int segments = 32) {
        if (radius <= 0.0f || height <= 0.0f)
            return;

        axis = glm::normalize(axis);
        const float cylinder_height = glm::max(0.0f, height - radius * 2.0f);
        const glm::vec3 half_axis = axis * (cylinder_height * 0.5f);
        const glm::vec3 top = center + half_axis;
        const glm::vec3 bottom = center - half_axis;
        add_cylinder(center, axis, radius, glm::max(cylinder_height, 0.0001f), color, segments);
        add_sphere(top, radius, color, segments);
        add_sphere(bottom, radius, color, segments);
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
