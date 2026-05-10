#include "camera.h"
#include <glm\gtc\quaternion.hpp>

Camera::Camera()
{
	auto event_manager = Services::get().get<EE::EventManager>();
	event_manager->register_handler<Camera, EE::WindowResizeEvent, &Camera::on_resize>(this);
	_recalculate_projection(); _recalculate_view();
}

void Camera::set_perspective(float p_fov_degrees, float p_aspect, float p_near, float p_far)
{
	_projection_type = CameraProjection::Perspective;
	_fov = glm::radians(p_fov_degrees);
	_aspect = p_aspect;
	_near = p_near;
	_far = p_far;
	_recalculate_projection();
}

float Camera::get_near_clip() const
{
	return _near;
}

float Camera::get_far_clip() const
{
	return _far;
}

void Camera::set_orthographic(float p_left, float p_right, float p_bottom, float p_top, float p_near, float p_far)
{
	_projection_type = CameraProjection::Orthographic;
	_ortho_left = p_left;
	_ortho_right = p_right;
	_ortho_bottom = p_bottom;
	_ortho_top = p_top;
	_near = p_near;
	_far = p_far;
	_recalculate_projection();
}

bool Camera::on_resize(const EE::WindowResizeEvent& e)
{
	if (_recalculate_on_resize)
	{
		set_aspect(static_cast<float>(e.width) / e.height);
		return true;
	}
	return false;
}

void Camera::set_euler_degrees(float p_pitch, float p_yaw, float p_roll /*= 0.0f*/)
{
	_pitch = glm::radians(p_pitch);
	_yaw = glm::radians(p_yaw);
	_roll = glm::radians(p_roll);
	_rotation = glm::quat(glm::vec3(_pitch, _yaw, _roll));
	_recalculate_view();
}

void Camera::fly_move(const glm::vec3& p_local_delta)
{
	// p_local_delta: x = right, y = up, z = forward (positive = forward)
	_position += get_right() * p_local_delta.x;
	_position += get_up() * p_local_delta.y;
	_position += get_forward() * p_local_delta.z;
	_recalculate_view();
}

void Camera::fly_rotate(float p_delta_yaw, float p_delta_pitch)
{
	_yaw += p_delta_yaw;
	_pitch += p_delta_pitch;
	_pitch = glm::clamp(_pitch, glm::radians(-89.0f), glm::radians(89.0f));
	_rotation = glm::quat(glm::vec3(_pitch, _yaw, _roll));
	_recalculate_view();
}

void Camera::orbit_rotate(float p_delta_yaw, float p_delta_pitch)
{
	_orbit_yaw += p_delta_yaw;
	_orbit_pitch += p_delta_pitch;
	_orbit_pitch = glm::clamp(_orbit_pitch, glm::radians(-89.0f), glm::radians(89.0f));
	_recalculate_view();
}

void Camera::follow_update(const glm::vec3& p_target_pos, const glm::vec3& p_target_forward /*= glm::vec3(0.0f, 0.0f, -1.0f)*/)
{
	glm::vec3 offset = -glm::normalize(p_target_forward) * _follow_distance
		+ glm::vec3(0.0f, 1.0f, 0.0f) * _follow_height;

	// Smooth follow using lerp
	_position = glm::mix(_position, p_target_pos + offset, _follow_smoothing);

	// Always look at target + height offset
	glm::vec3 look_at_point = p_target_pos + glm::vec3(0.0f, _follow_height * 0.5f, 0.0f);
	_view = glm::lookAt(_position, look_at_point, glm::vec3(0.0f, 1.0f, 0.0f));
	_update_frustum();
}

bool Camera::is_sphere_visible(const glm::vec3& p_center, float p_radius) const
{
	for (const FrustumPlane& plane : _frustum.planes) {
		if (glm::dot(plane.normal, p_center) + plane.distance + p_radius < 0.0f) {
			return false; // Fully outside this plane
		}
	}
	return true;
}

bool Camera::is_aabb_visible(const glm::vec3& p_min, const glm::vec3& p_max) const
{
	for (const FrustumPlane& plane : _frustum.planes) {
		// Find the positive vertex (furthest along plane normal)
		glm::vec3 positive = p_min;
		if (plane.normal.x >= 0.0f) positive.x = p_max.x;
		if (plane.normal.y >= 0.0f) positive.y = p_max.y;
		if (plane.normal.z >= 0.0f) positive.z = p_max.z;

		if (glm::dot(plane.normal, positive) + plane.distance < 0.0f) {
			return false; // Fully outside this plane
		}
	}
	return true;
}

void Camera::_recalculate_view()
{
	if (_mode == CameraMode::Orbit) {
		// Compute camera position from spherical orbit coordinates
		glm::vec3 offset;
		offset.x = _orbit_distance * cosf(_orbit_pitch) * sinf(_orbit_yaw);
		offset.y = _orbit_distance * sinf(_orbit_pitch);
		offset.z = _orbit_distance * cosf(_orbit_pitch) * cosf(_orbit_yaw);
		_position = _orbit_target + offset;
		_view = glm::lookAt(_position, _orbit_target, glm::vec3(0.0f, 1.0f, 0.0f));
	}
	else {
		// Build view matrix by inverting the camera's world transform (T * R).
		// conjugate(q) == inverse rotation for unit quaternions, cheaper than glm::inverse.
		glm::mat4 t = glm::translate(glm::mat4(1.0f), _position);
		glm::mat4 r = glm::mat4_cast(_rotation);
		_view = glm::inverse(t * r);
	}
	_update_frustum();
}

void Camera::_update_frustum()
{
	glm::mat4 m = _projection * _view;

	// Extract rows from column-major matrix for Gribb/Hartmann plane extraction
	glm::vec4 c0 = glm::vec4(m[0][0], m[1][0], m[2][0], m[3][0]);
	glm::vec4 c1 = glm::vec4(m[0][1], m[1][1], m[2][1], m[3][1]);
	glm::vec4 c2 = glm::vec4(m[0][2], m[1][2], m[2][2], m[3][2]);
	glm::vec4 c3 = glm::vec4(m[0][3], m[1][3], m[2][3], m[3][3]);

	auto extract = [](glm::vec4 row) -> FrustumPlane {
		float len = glm::length(glm::vec3(row));
		return { glm::vec3(row) / len, row.w / len };
		};

	_frustum.planes[0] = extract(c3 + c0); // Left
	_frustum.planes[1] = extract(c3 - c0); // Right
	_frustum.planes[2] = extract(c3 + c1); // Bottom
	_frustum.planes[3] = extract(c3 - c1); // Top
	_frustum.planes[4] = extract(c2);       // Near  (Vulkan ZO: z_clip >= 0, not OpenGL's c3+c2)
	_frustum.planes[5] = extract(c3 - c2); // Far
}