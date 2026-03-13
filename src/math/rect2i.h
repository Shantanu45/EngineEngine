#pragma once

#include "util/error_macros.h"
#include "math_common.h"
#include "util/hash_func.h"
#include <array>
#include <cstdint>

class String;
struct Rect2;

struct [[nodiscard]] Rect2i {
	Point2i position;
	Size2i size;

	const Point2i& get_position() const { return position; }
	void set_position(const Point2i& p_position) { position = p_position; }
	const Size2i& get_size() const { return size; }
	void set_size(const Size2i& p_size) { size = p_size; }

	int get_area() const { return size.x * size.y; }

	_FORCE_INLINE_ Vector2i get_center() const { return position + (size / 2); }

	inline bool intersects(const Rect2i& p_rect) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
			ERR_PRINT("Rect2i size is negative, this is not supported. Use Rect2i.abs() to get a Rect2i with a positive size.");
		}
#endif
		if (position.x >= (p_rect.position.x + p_rect.size.x)) {
			return false;
		}
		if ((position.x + size.x) <= p_rect.position.x) {
			return false;
		}
		if (position.y >= (p_rect.position.y + p_rect.size.y)) {
			return false;
		}
		if ((position.y + size.y) <= p_rect.position.y) {
			return false;
		}

		return true;
	}

	inline bool encloses(const Rect2i& p_rect) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
			ERR_PRINT("Rect2i size is negative, this is not supported. Use Rect2i.abs() to get a Rect2i with a positive size.");
		}
#endif
		return (p_rect.position.x >= position.x) && (p_rect.position.y >= position.y) &&
			((p_rect.position.x + p_rect.size.x) <= (position.x + size.x)) &&
			((p_rect.position.y + p_rect.size.y) <= (position.y + size.y));
	}

	_FORCE_INLINE_ bool has_area() const {
		return size.x > 0 && size.y > 0;
	}

	// Returns the intersection between two Rect2is or an empty Rect2i if there is no intersection.
	inline Rect2i intersection(const Rect2i& p_rect) const {
		Rect2i new_rect = p_rect;

		if (!intersects(new_rect)) {
			return Rect2i();
		}

		new_rect.position = glm::max(p_rect.position, position);

		Point2i p_rect_end = p_rect.position + p_rect.size;
		Point2i end = position + size;

		new_rect.size = glm::min(p_rect_end, end) - new_rect.position;

		return new_rect;
	}

	inline Rect2i merge(const Rect2i& p_rect) const { ///< return a merged rect
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0 || p_rect.size.x < 0 || p_rect.size.y < 0)) {
			ERR_PRINT("Rect2i size is negative, this is not supported. Use Rect2i.abs() to get a Rect2i with a positive size.");
		}
#endif
		Rect2i new_rect;

		new_rect.position = glm::min(p_rect.position, position);

		new_rect.size = glm::max((p_rect.position + p_rect.size), (position + size));

		new_rect.size = new_rect.size - new_rect.position; // Make relative again.

		return new_rect;
	}
	bool has_point(const Point2i& p_point) const {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0)) {
			ERR_PRINT("Rect2i size is negative, this is not supported. Use Rect2i.abs() to get a Rect2i with a positive size.");
		}
#endif
		if (p_point.x < position.x) {
			return false;
		}
		if (p_point.y < position.y) {
			return false;
		}

		if (p_point.x >= (position.x + size.x)) {
			return false;
		}
		if (p_point.y >= (position.y + size.y)) {
			return false;
		}

		return true;
	}

	constexpr bool operator==(const Rect2i& p_rect) const { return position == p_rect.position && size == p_rect.size; }
	constexpr bool operator!=(const Rect2i& p_rect) const { return position != p_rect.position || size != p_rect.size; }

	Rect2i grow(int p_amount) const {
		Rect2i g = *this;
		g.position.x -= p_amount;
		g.position.y -= p_amount;
		g.size.x += p_amount * 2;
		g.size.y += p_amount * 2;
		return g;
	}

	inline Rect2i grow_side(Side p_side, int p_amount) const {
		Rect2i g = *this;
		g = g.grow_individual((SIDE_LEFT == p_side) ? p_amount : 0,
			(SIDE_TOP == p_side) ? p_amount : 0,
			(SIDE_RIGHT == p_side) ? p_amount : 0,
			(SIDE_BOTTOM == p_side) ? p_amount : 0);
		return g;
	}

	inline Rect2i grow_side_bind(uint32_t p_side, int p_amount) const {
		return grow_side(Side(p_side), p_amount);
	}

	inline Rect2i grow_individual(int p_left, int p_top, int p_right, int p_bottom) const {
		Rect2i g = *this;
		g.position.x -= p_left;
		g.position.y -= p_top;
		g.size.x += p_left + p_right;
		g.size.y += p_top + p_bottom;

		return g;
	}

	_FORCE_INLINE_ Rect2i expand(const Vector2i& p_vector) const {
		Rect2i r = *this;
		r.expand_to(p_vector);
		return r;
	}

	inline void expand_to(const Point2i& p_vector) {
#ifdef MATH_CHECKS
		if (unlikely(size.x < 0 || size.y < 0)) {
			ERR_PRINT("Rect2i size is negative, this is not supported. Use Rect2i.abs() to get a Rect2i with a positive size.");
		}
#endif
		Point2i begin = position;
		Point2i end = position + size;

		if (p_vector.x < begin.x) {
			begin.x = p_vector.x;
		}
		if (p_vector.y < begin.y) {
			begin.y = p_vector.y;
		}

		if (p_vector.x > end.x) {
			end.x = p_vector.x;
		}
		if (p_vector.y > end.y) {
			end.y = p_vector.y;
		}

		position = begin;
		size = end - begin;
	}

	_FORCE_INLINE_ Rect2i abs() const {
		return Rect2i(position + glm::min(size, 0), glm::abs(size));
	}

	_FORCE_INLINE_ void set_end(const Vector2i& p_end) {
		size = p_end - position;
	}

	_FORCE_INLINE_ Vector2i get_end() const {
		return position + size;
	}

	std::string to_string() const;
	operator Rect2() const;

	uint32_t hash() const {
		//uint32_t h = hash_xxhash_one_32(uint32_t(position.x));
		//h = hash_xxhash_one_32(uint32_t(position.y), h);
		//h = hash_xxhash_one_32(uint32_t(size.x), h);
		//h = hash_xxhash_one_32(uint32_t(size.y), h);

		std::array<uint32_t, 4> d = {
			uint32_t(position.x),
			uint32_t(position.y),
			uint32_t(size.x),
			uint32_t(size.y)
		};

		return hash_xxhash_32(d.data(), d.size());
	}

	Rect2i() = default;
	constexpr Rect2i(int p_x, int p_y, int p_width, int p_height) :
		position(Point2i(p_x, p_y)),
		size(Size2i(p_width, p_height)) {
	}
	constexpr Rect2i(const Point2i& p_pos, const Size2i& p_size) :
		position(p_pos),
		size(p_size) {
	}
};
