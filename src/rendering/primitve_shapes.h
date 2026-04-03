/*****************************************************************//**
 * \file   primitive_shapes.h
 * \brief  CPU-side mesh data and MeshStorage upload helpers for
 *         basic shapes: quad, cube, sphere, cylinder, cone, plane.
 *
 *         All shapes are unit-sized, centered at the origin, and use
 *         the same Vertex layout as the rest of the renderer
 *         (position, normal, texcoord, tangent).
 *
 * Usage:
 *   // Build CPU data
 *   auto data = Shapes::make_sphere(32, 32);
 *
 *   // Upload directly into MeshStorage
 *   MeshHandle h = Shapes::upload(storage, "my_sphere", data, vertex_format);
 *
 *   // Or use the prebuilt shape uploaders on WSI
 *   MeshHandle h = Shapes::upload_sphere(wsi, "sphere");
 * 
 * \author Shantanu Kumar
 * \date   March 2026
 *********************************************************************/

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <numbers>

#include "rendering/mesh_storage.h"
#include "rendering/wsi.h"

 // Forward declaration — Vertex is defined in your rendering headers
namespace Rendering { struct Vertex; }

namespace Rendering::Shapes
{

    // --- Raw CPU mesh data --------------------------------------------------------

    struct ShapeData
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
    };

    // --- Shape generators --------------------------------------------------------

    // Quad — unit quad on the XY plane, facing +Z
    // 2 triangles, 4 vertices
    inline ShapeData make_quad()
    {
        ShapeData d;

        const glm::vec3 normal = { 0, 0, 1 };
        const glm::vec4 tangent = { 1, 0, 0, 1 };

        d.vertices =
        {
            { { -0.5f, -0.5f, 0.0f }, normal, { 0.0f, 0.0f }, tangent },
            { {  0.5f, -0.5f, 0.0f }, normal, { 1.0f, 0.0f }, tangent },
            { {  0.5f,  0.5f, 0.0f }, normal, { 1.0f, 1.0f }, tangent },
            { { -0.5f,  0.5f, 0.0f }, normal, { 0.0f, 1.0f }, tangent },
        };

        d.indices = { 0, 1, 2,  2, 3, 0 };
        return d;
    }

    // Plane — unit grid on the XZ plane, facing +Y
    // subdivisions controls tessellation (1 = single quad)
    inline ShapeData make_plane(uint32_t subdivisions = 1)
    {
        ShapeData d;

        const uint32_t verts_per_side = subdivisions + 1;
        const float    step = 1.0f / float(subdivisions);

        const glm::vec3 normal = { 0, 1, 0 };
        const glm::vec4 tangent = { 1, 0, 0, 1 };

        for (uint32_t row = 0; row <= subdivisions; ++row)
        {
            for (uint32_t col = 0; col <= subdivisions; ++col)
            {
                float x = -0.5f + col * step;
                float z = -0.5f + row * step;
                float u = float(col) / float(subdivisions);
                float v = float(row) / float(subdivisions);
                d.vertices.push_back({ { x, 0.0f, z }, normal, { u, v }, tangent });
            }
        }

        for (uint32_t row = 0; row < subdivisions; ++row)
        {
            for (uint32_t col = 0; col < subdivisions; ++col)
            {
                uint32_t i0 = row * verts_per_side + col;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = i0 + verts_per_side;
                uint32_t i3 = i2 + 1;

                d.indices.insert(d.indices.end(), { i0, i2, i1,  i1, i2, i3 });
            }
        }

        return d;
    }

    // Cube — unit cube centered at origin, each face has its own vertices
    // for correct per-face normals and UVs
    inline ShapeData make_cube()
    {
        ShapeData d;

        // face: normal, tangent, and 4 corner positions in CCW order
        struct Face
        {
            glm::vec3 normal;
            glm::vec4 tangent;
            glm::vec3 corners[4]; // bl, br, tr, tl
        };

        const Face faces[] =
        {
            // +Z front
            { { 0, 0, 1 }, { 1, 0, 0, 1 },
              { { -0.5f,-0.5f, 0.5f }, { 0.5f,-0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f } } },
              // -Z back
              { { 0, 0,-1 }, { -1, 0, 0, 1 },
                { {  0.5f,-0.5f,-0.5f }, {-0.5f,-0.5f,-0.5f }, {-0.5f, 0.5f,-0.5f }, {  0.5f, 0.5f,-0.5f } } },
                // +X right
                { { 1, 0, 0 }, { 0, 0,-1, 1 },
                  { {  0.5f,-0.5f, 0.5f }, { 0.5f,-0.5f,-0.5f }, { 0.5f, 0.5f,-0.5f }, {  0.5f, 0.5f, 0.5f } } },
                  // -X left
                  { {-1, 0, 0 }, { 0, 0, 1, 1 },
                    { { -0.5f,-0.5f,-0.5f }, {-0.5f,-0.5f, 0.5f }, {-0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f,-0.5f } } },
                    // +Y top
                    { { 0, 1, 0 }, { 1, 0, 0, 1 },
                      { { -0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f,-0.5f }, { -0.5f, 0.5f,-0.5f } } },
                      // -Y bottom
                      { { 0,-1, 0 }, { 1, 0, 0, 1 },
                        { { -0.5f,-0.5f,-0.5f }, { 0.5f,-0.5f,-0.5f }, { 0.5f,-0.5f, 0.5f }, { -0.5f,-0.5f, 0.5f } } },
        };

        const glm::vec2 uvs[4] = { {0,0}, {1,0}, {1,1}, {0,1} };

        for (auto& face : faces)
        {
            uint32_t base = static_cast<uint32_t>(d.vertices.size());

            for (int i = 0; i < 4; ++i)
                d.vertices.push_back({ face.corners[i], face.normal, uvs[i], face.tangent });

            d.indices.insert(d.indices.end(),
                { base, base + 1, base + 2,  base + 2, base + 3, base });
        }

        return d;
    }

    // Sphere — UV sphere, radius 0.5, centered at origin
    // stacks = horizontal rings, slices = vertical segments
    inline ShapeData make_sphere(uint32_t stacks = 16, uint32_t slices = 16)
    {
        ShapeData d;

        const float pi = std::numbers::pi_v<float>;
        const float tau = 2.0f * pi;

        for (uint32_t i = 0; i <= stacks; ++i)
        {
            float phi = pi * float(i) / float(stacks);      // 0 .. PI  (top to bottom)
            float y = std::cos(phi);
            float r = std::sin(phi);

            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = tau * float(j) / float(slices); // 0 .. 2PI

                glm::vec3 normal =
                {
                    r * std::cos(theta),
                    y,
                    r * std::sin(theta),
                };

                glm::vec2 uv =
                {
                    float(j) / float(slices),
                    float(i) / float(stacks),
                };

                // Tangent is the derivative with respect to theta (longitude)
                glm::vec4 tangent =
                {
                    -std::sin(theta), 0.0f, std::cos(theta), 1.0f
                };

                d.vertices.push_back({ normal * 0.5f, normal, uv, tangent });
            }
        }

        for (uint32_t i = 0; i < stacks; ++i)
        {
            for (uint32_t j = 0; j < slices; ++j)
            {
                uint32_t row_cur = i * (slices + 1);
                uint32_t row_next = (i + 1) * (slices + 1);

                uint32_t a = row_cur + j;
                uint32_t b = row_cur + j + 1;
                uint32_t c = row_next + j;
                uint32_t d_ = row_next + j + 1;

                // CW whinding order
                // Skip degenerate triangles at poles
 /*               if (i != 0)
					d.indices.insert(d.indices.end(), { a, c, b });
				if (i + 1 != stacks)
					d.indices.insert(d.indices.end(), { b, c, d_ });*/

                // CCW whindin order
				if (i != 0)
					d.indices.insert(d.indices.end(), { a, b, c });
				if (i + 1 != stacks)
					d.indices.insert(d.indices.end(), { b, d_, c });
            }
        }

        return d;
    }

    // Cylinder — radius 0.5, height 1.0, centered at origin
    // includes top and bottom caps
    inline ShapeData make_cylinder(uint32_t slices = 16, bool caps = true)
    {
        ShapeData d;

        const float tau = 2.0f * std::numbers::pi_v<float>;
        const float radius = 0.5f;
        const float half_h = 0.5f;

        // Side vertices — two rings (bottom and top), each slice gets its own
        // vertex pair so normals are correct per-column
        for (uint32_t j = 0; j <= slices; ++j)
        {
            float theta = tau * float(j) / float(slices);
            float cos_t = std::cos(theta);
            float sin_t = std::sin(theta);
            float u = float(j) / float(slices);

            glm::vec3 normal = { cos_t, 0.0f, sin_t };
            glm::vec4 tangent = { -sin_t, 0.0f, cos_t, 1.0f };

            // bottom
            d.vertices.push_back({ { cos_t * radius, -half_h, sin_t * radius }, normal, { u, 0.0f }, tangent });
            // top
            d.vertices.push_back({ { cos_t * radius,  half_h, sin_t * radius }, normal, { u, 1.0f }, tangent });
        }

        // Side indices
        for (uint32_t j = 0; j < slices; ++j)
        {
            uint32_t b0 = j * 2;
            uint32_t t0 = j * 2 + 1;
            uint32_t b1 = (j + 1) * 2;
            uint32_t t1 = (j + 1) * 2 + 1;

            d.indices.insert(d.indices.end(), { b0, b1, t0,  t0, b1, t1 });
        }

        if (caps)
        {
            // Bottom cap — normal pointing down
            uint32_t bottom_center = static_cast<uint32_t>(d.vertices.size());
            d.vertices.push_back({ { 0.0f, -half_h, 0.0f }, { 0,-1,0 }, { 0.5f, 0.5f }, { 1,0,0,1 } });

            uint32_t bottom_ring_start = static_cast<uint32_t>(d.vertices.size());
            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = tau * float(j) / float(slices);
                float u = 0.5f + 0.5f * std::cos(theta);
                float v = 0.5f + 0.5f * std::sin(theta);
                d.vertices.push_back({
                    { std::cos(theta) * radius, -half_h, std::sin(theta) * radius },
                    { 0,-1,0 }, { u, v }, { 1,0,0,1 }
                    });
            }
            for (uint32_t j = 0; j < slices; ++j)
                d.indices.insert(d.indices.end(),
                    { bottom_center, bottom_ring_start + j + 1, bottom_ring_start + j });

            // Top cap — normal pointing up
            uint32_t top_center = static_cast<uint32_t>(d.vertices.size());
            d.vertices.push_back({ { 0.0f, half_h, 0.0f }, { 0,1,0 }, { 0.5f, 0.5f }, { 1,0,0,1 } });

            uint32_t top_ring_start = static_cast<uint32_t>(d.vertices.size());
            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = tau * float(j) / float(slices);
                float u = 0.5f + 0.5f * std::cos(theta);
                float v = 0.5f + 0.5f * std::sin(theta);
                d.vertices.push_back({
                    { std::cos(theta) * radius, half_h, std::sin(theta) * radius },
                    { 0,1,0 }, { u, v }, { 1,0,0,1 }
                    });
            }
            for (uint32_t j = 0; j < slices; ++j)
                d.indices.insert(d.indices.end(),
                    { top_center, top_ring_start + j, top_ring_start + j + 1 });
        }

        return d;
    }

    // Cone — base radius 0.5, height 1.0, base centered at origin, tip at +Y
    inline ShapeData make_cone(uint32_t slices = 16, bool cap = true)
    {
        ShapeData d;

        const float tau = 2.0f * std::numbers::pi_v<float>;
        const float radius = 0.5f;
        const float height = 1.0f;
        const float half_h = height * 0.5f;

        // Slope normal: (height, radius, 0) normalized, rotated per slice
        const float slope_len = std::sqrt(height * height + radius * radius);
        const float ny = radius / slope_len;
        const float nr = height / slope_len;

        // Side — tip vertex per slice for correct normals
        for (uint32_t j = 0; j < slices; ++j)
        {
            float theta0 = tau * float(j) / float(slices);
            float theta1 = tau * float(j + 1) / float(slices);
            float mid = (theta0 + theta1) * 0.5f;

            glm::vec3 tip_normal = { nr * std::cos(mid), ny, nr * std::sin(mid) };
            glm::vec4 tangent = { -std::sin(mid), 0, std::cos(mid), 1 };

            // tip
            uint32_t tip_idx = static_cast<uint32_t>(d.vertices.size());
            d.vertices.push_back({ { 0, half_h, 0 }, tip_normal, { 0.5f, 1.0f }, tangent });

            glm::vec3 n0 = { nr * std::cos(theta0), ny, nr * std::sin(theta0) };
            glm::vec3 n1 = { nr * std::cos(theta1), ny, nr * std::sin(theta1) };

            d.vertices.push_back({
                { std::cos(theta0) * radius, -half_h, std::sin(theta0) * radius },
                n0, { float(j) / float(slices), 0.0f }, tangent });
            d.vertices.push_back({
                { std::cos(theta1) * radius, -half_h, std::sin(theta1) * radius },
                n1, { float(j + 1) / float(slices), 0.0f }, tangent });

            d.indices.insert(d.indices.end(), { tip_idx, tip_idx + 1, tip_idx + 2 });
        }

        if (cap)
        {
            uint32_t center = static_cast<uint32_t>(d.vertices.size());
            d.vertices.push_back({ { 0, -half_h, 0 }, { 0,-1,0 }, { 0.5f,0.5f }, { 1,0,0,1 } });

            uint32_t ring_start = static_cast<uint32_t>(d.vertices.size());
            for (uint32_t j = 0; j <= slices; ++j)
            {
                float theta = tau * float(j) / float(slices);
                d.vertices.push_back({
                    { std::cos(theta) * radius, -half_h, std::sin(theta) * radius },
                    { 0,-1,0 },
                    { 0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta) },
                    { 1,0,0,1 }
                    });
            }
            for (uint32_t j = 0; j < slices; ++j)
                d.indices.insert(d.indices.end(),
                    { center, ring_start + j + 1, ring_start + j });
        }

        return d;
    }

    // --- Upload helpers -----------------------------------------------------------

    // Generic upload — takes any ShapeData and uploads it into MeshStorage
    inline MeshHandle upload(
        MeshStorage& storage,
        const std::string& name,
        const ShapeData& shape,
        RenderingDevice::VertexFormatID           vertex_format,
        RenderingDeviceCommons::IndexBufferFormat index_format = RenderingDeviceCommons::INDEX_BUFFER_FORMAT_UINT32)
    {
        if (storage.has(name))
            return storage.get_handle(name);

        const uint64_t vb_size = shape.vertices.size() * sizeof(Vertex);
        const uint64_t ib_size = shape.indices.size() * sizeof(uint32_t);

        std::vector<uint8_t> vertex_data(vb_size);
        std::vector<uint8_t> index_data(ib_size);

        std::memcpy(vertex_data.data(), shape.vertices.data(), vb_size);
        std::memcpy(index_data.data(), shape.indices.data(), ib_size);

        PrimitiveRange range{};
        range.vertex_offset = 0;
        range.vertex_byte_offset = 0;
        range.index_offset = 0;
        range.index_count = static_cast<uint32_t>(shape.indices.size());
        range.vertex_count = static_cast<uint32_t>(shape.vertices.size());

        return storage.create_mesh(name, vertex_data, index_data, { range }, vertex_format, index_format);
    }

    // Per-shape convenience uploaders via WSI
    inline MeshHandle upload_quad(WSI& wsi, const std::string& name = "primitive_quad",
        VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        return upload(*wsi.get_mesh_storage(), name, make_quad(), wsi.get_vertex_format_by_type(fmt));
    }

    inline MeshHandle upload_plane(WSI& wsi, uint32_t subdivisions = 1, const std::string& name = "primitive_plane",
        VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        return upload(*wsi.get_mesh_storage(), name, make_plane(subdivisions), wsi.get_vertex_format_by_type(fmt));
    }

    inline MeshHandle upload_cube(WSI& wsi, const std::string& name = "primitive_cube",
        VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        return upload(*wsi.get_mesh_storage(), name, make_cube(), wsi.get_vertex_format_by_type(fmt));
    }

    inline MeshHandle upload_sphere(WSI& wsi, uint32_t stacks = 16, uint32_t slices = 16,
        const std::string& name = "primitive_sphere",
        VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        return upload(*wsi.get_mesh_storage(), name, make_sphere(stacks, slices), wsi.get_vertex_format_by_type(fmt));
    }

    inline MeshHandle upload_cylinder(WSI& wsi, uint32_t slices = 16, bool caps = true,
        const std::string& name = "primitive_cylinder",
        VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        return upload(*wsi.get_mesh_storage(), name, make_cylinder(slices, caps), wsi.get_vertex_format_by_type(fmt));
    }

    inline MeshHandle upload_cone(WSI& wsi, uint32_t slices = 16, bool cap = true,
        const std::string& name = "primitive_cone",
        VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        return upload(*wsi.get_mesh_storage(), name, make_cone(slices, cap), wsi.get_vertex_format_by_type(fmt));
    }

    // Upload all primitives at once — useful for a debug/editor setup
    inline void upload_all(WSI& wsi, VERTEX_FORMAT_VARIATIONS fmt = VERTEX_FORMAT_VARIATIONS::DEFAULT)
    {
        upload_quad(wsi, "primitive_quad", fmt);
        upload_plane(wsi, 1, "primitive_plane", fmt);
        upload_cube(wsi, "primitive_cube", fmt);
        upload_sphere(wsi, 16, 16, "primitive_sphere", fmt);
        upload_cylinder(wsi, 16, true, "primitive_cylinder", fmt);
        upload_cone(wsi, 16, true, "primitive_cone", fmt);
    }

} // namespace Rendering::Shapes