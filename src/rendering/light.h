#pragma once
#include <glm/glm.hpp>
#include <cstdint>

static constexpr uint32_t MAX_LIGHTS = 16;

enum class LightType : uint32_t {
	Directional = 0,
	Point = 1,
	Spot = 2
};

struct alignas(16) Light {
	glm::vec4 position;    // xyz = pos,   w = range (point/spot)
	glm::vec4 direction;   // xyz = dir,   w = spot inner angle cos
	glm::vec4 color;       // xyz = color, w = intensity
	uint32_t  type;
	float     outer_angle;
	float     _pad0;      // rename for clarity, no behaviour change
	float     _pad1;
};


struct alignas(16) LightBuffer_UBO {
	uint32_t count;
	float    _pad[3];
	Light    lights[MAX_LIGHTS];
};
static_assert(sizeof(LightBuffer_UBO) == 16 + 64 * MAX_LIGHTS, "LightBuffer size mismatch");