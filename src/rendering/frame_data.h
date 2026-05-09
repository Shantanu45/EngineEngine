#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include "rendering/light.h"

struct alignas(16) CameraData {
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    glm::vec3 cameraPos = glm::vec3(0.0f);
    float     _pad = 0.0f;
};

struct alignas(16) FrameData_UBO {
    CameraData camera;
    float      time;
    uint32_t   directional_shadow_index; // index into ShadowBuffer_UBO
    uint32_t   point_shadow_index;       // index into ShadowBuffer_UBO
    uint32_t   material_debug_view;
};

struct alignas(16) ObjectData_UBO {
    glm::mat4 model;
    glm::mat4 normalMatrix;
};

// One entry per shadow-casting light, regardless of type.
// matrices[0]     = directional / spot light-space matrix
// matrices[0..5]  = point light cubemap face matrices
// light_pos.xyz   = world position, light_pos.w = far plane (point lights)
struct alignas(16) ShadowData {
    glm::mat4 matrices[6];
    glm::vec4 light_pos;
    uint32_t  light_index;
    float     _pad[3];
};

struct alignas(16) ShadowBuffer_UBO {
    uint32_t   count;
    float      _pad[3];
    ShadowData shadows[MAX_LIGHTS];
};

static_assert(sizeof(ShadowData) == 416,                         "ShadowData size mismatch");
static_assert(sizeof(ShadowBuffer_UBO) == 16 + 416 * MAX_LIGHTS, "ShadowBuffer_UBO size mismatch");
