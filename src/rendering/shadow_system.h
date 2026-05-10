#pragma once

#include "rendering/drawable.h"
#include "rendering/frame_data.h"
#include "rendering/light.h"
#include "rendering/renderers/renderer.h"
#include "camera.h"

#include <cstdint>
#include <vector>

namespace Rendering {

enum class ShadowProjection : uint32_t {
	Directional2D,
	PointCube,
};

struct ShadowShaderBinding {
	ShadowProjection projection = ShadowProjection::Directional2D;
	Pipeline pipeline;
	RID uniform_set_0;
};

struct ShadowBuildResult {
	ShadowBuffer_UBO buffer{};
	uint32_t directional_shadow_index = 0;
	uint32_t point_shadow_index = 0;
};

class ShadowSystem {
public:
	static ShadowBuildResult build_shadow_buffer(
		const Util::SmallVector<Light>& lights,
		const CameraData& camera,
		DirectionalShadowMode directional_shadow_mode);

	static std::vector<Drawable> build_shadow_drawables(
		const SceneView& view,
		const ShadowShaderBinding& binding);

private:
	static ShadowData _update_cascades(const CameraData& camera, const glm::vec3& light_dir);
};

} // namespace Rendering
