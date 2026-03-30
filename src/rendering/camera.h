/*****************************************************************//**
 * \file   camera.h
 * \brief  
 * 
 * \author skumar
 * \date   March 2026
 *********************************************************************/
#pragma once

#include <array>
#include "math/helpers.h"


// -----------------------------------------------------------------------------
//  Camera
//
//  Supports:
//    - Perspective and Orthographic projection
//    - Fly / free-look mode
//    - Orbit / arcball mode
//    - Follow-target mode
//    - Frustum culling (sphere and AABB tests)
// -----------------------------------------------------------------------------

enum class CameraProjection {
	Perspective,
	Orthographic
};

enum class CameraMode {
	Fly,        // Free-look, WASD-style
	Orbit,      // Orbit around a target point
	Follow      // Follows a target transform
};

struct FrustumPlane {
	glm::vec3 normal;
	float     distance; // signed distance from origin
};

struct Frustum {
	// Order: Left, Right, Bottom, Top, Near, Far
	std::array<FrustumPlane, 6> planes;
};

class Camera {
public:
	// --- Construction --------------------------------------------------------

	Camera() { _recalculate_projection(); _recalculate_view(); }

	// --- Projection settings -------------------------------------------------

	void set_perspective(float p_fov_degrees, float p_aspect, float p_near, float p_far);

	void set_orthographic(float p_left, float p_right, float p_bottom, float p_top,
		float p_near, float p_far);

	void set_aspect(float p_aspect) {
		_aspect = p_aspect;
		_recalculate_projection();
	}

	// --- Mode ----------------------------------------------------------------

	void set_mode(CameraMode p_mode) { _mode = p_mode; }
	CameraMode get_mode() const { return _mode; }

	// --- Transform -----------------------------------------------------------

	void set_position(const glm::vec3& p_pos) {
		_position = p_pos;
		_recalculate_view();
	}

	void set_rotation(const glm::quat& p_rot) {
		_rotation = glm::normalize(p_rot);
		_recalculate_view();
	}

	void set_euler_degrees(float p_pitch, float p_yaw, float p_roll = 0.0f);

	const glm::vec3& get_position()  const { return _position; }
	const glm::quat& get_rotation()  const { return _rotation; }

	glm::vec3 get_forward() const { return glm::normalize(_rotation * glm::vec3(0.0f, 0.0f, -1.0f)); }
	glm::vec3 get_right()   const { return glm::normalize(_rotation * glm::vec3(1.0f, 0.0f, 0.0f)); }
	glm::vec3 get_up()      const { return glm::normalize(_rotation * glm::vec3(0.0f, 1.0f, 0.0f)); }

	// --- Fly / Free-look -----------------------------------------------------
	// Call from your input/update loop each frame.

	void fly_move(const glm::vec3& p_local_delta);

	// p_delta_x = yaw delta (radians), p_delta_y = pitch delta (radians)
	void fly_rotate(float p_delta_yaw, float p_delta_pitch);

	// --- Orbit / Arcball -----------------------------------------------------

	void set_orbit_target(const glm::vec3& p_target) {
		_orbit_target = p_target;
		_recalculate_view();
	}

	void set_orbit_distance(float p_distance) {
		_orbit_distance = glm::max(p_distance, 0.001f);
		_recalculate_view();
	}

	// p_delta_x = horizontal orbit (radians), p_delta_y = vertical orbit (radians)
	void orbit_rotate(float p_delta_yaw, float p_delta_pitch);

	void orbit_zoom(float p_delta) {
		set_orbit_distance(_orbit_distance - p_delta);
	}

	const glm::vec3& get_orbit_target()   const { return _orbit_target; }
	float            get_orbit_distance() const { return _orbit_distance; }

	// --- Follow target -------------------------------------------------------

	// p_target_pos:    world position of the entity being followed
	// p_target_forward: forward direction of the entity (used for behind-camera offset)
	void follow_update(const glm::vec3& p_target_pos,
		const glm::vec3& p_target_forward = glm::vec3(0.0f, 0.0f, -1.0f));

	void set_follow_distance(float p_dist) { _follow_distance = p_dist; }
	void set_follow_height(float p_height) { _follow_height = p_height; }
	void set_follow_smoothing(float p_smoothing) { _follow_smoothing = glm::clamp(p_smoothing, 0.0f, 1.0f); }

	// --- Matrices ------------------------------------------------------------

	const glm::mat4& get_view()       const { return _view; }
	const glm::mat4& get_projection() const { return _projection; }
	glm::mat4        get_view_projection() const { return _projection * _view; }

	// --- Frustum culling -----------------------------------------------------

	const Frustum& get_frustum() const { return _frustum; }

	// Returns true if a sphere is inside or intersecting the frustum.
	bool is_sphere_visible(const glm::vec3& p_center, float p_radius) const;

	// Returns true if an AABB (min/max corners) is inside or intersecting the frustum.
	bool is_aabb_visible(const glm::vec3& p_min, const glm::vec3& p_max) const;

	private:
		// --- Internal recalculation -----------------------------------------------

		void _recalculate_projection() {
			if (_projection_type == CameraProjection::Perspective) {
				_projection = glm::perspective(_fov, _aspect, _near, _far);
			}
			else {
				_projection = glm::ortho(_ortho_left, _ortho_right, _ortho_bottom, _ortho_top, _near, _far);
			}
			_projection[1][1] *= -1; // Vulkan clip space Y flip
			_update_frustum();
		}

		void _recalculate_view();

		// Extract frustum planes from the combined view-projection matrix.
		// Uses Gribb/Hartmann method.
		void _update_frustum();
private:
	// --- State ---------------------------------------------------------------

	CameraMode       _mode = CameraMode::Fly;
	CameraProjection _projection_type = CameraProjection::Perspective;

	// Transform
	glm::vec3 _position = glm::vec3(0.0f, 0.0f, 3.0f);
	glm::quat _rotation = glm::quat(glm::vec3(0.0f));
	float     _pitch = 0.0f;
	float     _yaw = 0.0f;
	float     _roll = 0.0f;

	// Perspective
	float _fov = glm::radians(60.0f);
	float _aspect = 16.0f / 9.0f;
	float _near = 0.1f;
	float _far = 1000.0f;

	// Orthographic
	float _ortho_left = -10.0f;
	float _ortho_right = 10.0f;
	float _ortho_bottom = -10.0f;
	float _ortho_top = 10.0f;

	// Orbit
	glm::vec3 _orbit_target = glm::vec3(0.0f);
	float     _orbit_distance = 5.0f;
	float     _orbit_yaw = 0.0f;
	float     _orbit_pitch = 0.0f;

	// Follow
	float _follow_distance = 5.0f;
	float _follow_height = 2.0f;
	float _follow_smoothing = 0.1f;

	// Matrices
	glm::mat4 _view = glm::mat4(1.0f);
	glm::mat4 _projection = glm::mat4(1.0f);

	// Frustum
	Frustum _frustum;

};