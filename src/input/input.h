#pragma once
#include <cstdint>
#include "SDL3/SDL.h"
#include "glm/glm.hpp"

namespace EE
{
	enum class Key : uint16_t
	{
		Unknown,
		A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
		Return,
		LeftCtrl,
		LeftAlt,
		LeftShift,
		Space,
		Escape,
		Left, Right, Up, Down,
		_1, _2, _3, _4, _5, _6, _7, _8, _9, _0,
		Count
	};

	enum KeyState : uint8_t
	{
		KEY_NONE = 0,
		KEY_DOWN = 1 << 0,		// 0000 0001 — key is currently held
		KEY_PRESSED = 1 << 1,	// 0000 0010 — went down this frame
		KEY_RELEASED = 1 << 2,  // 0000 0100 — went up this frame
	};

	enum class MouseButton
	{
		Left,
		Middle,
		Right,
		Count
	};

	// Add to KeyState enum
	enum MouseState : uint8_t
	{
		MOUSE_NONE = 0,
		MOUSE_DOWN = 1 << 0,      // 0000 0001 — button is held
		MOUSE_PRESSED = 1 << 1,   // 0000 0010 — went down this frame
		MOUSE_RELEASED = 1 << 2,  // 0000 0100 — went up this frame
	};

	class InputSystem
	{
	public:
		InputSystem();

		void on_sdl_event(const SDL_Event& e);

		bool is_held(Key k)      const { return keys[static_cast<size_t>(k)] & KEY_DOWN; }
		bool just_pressed(Key k) const { return keys[static_cast<size_t>(k)] & KEY_PRESSED; }
		bool just_released(Key k)const { return keys[static_cast<size_t>(k)] & KEY_RELEASED; }

		bool is_mouse_held(MouseButton b) const;
		bool just_mouse_pressed(MouseButton b) const;
		bool just_mouse_released(MouseButton b) const;
		glm::vec2 get_mouse_position() const;

		glm::vec2 get_mouse_delta() const;

		void update();

	private:

		Key sdl_to_key[SDL_SCANCODE_COUNT];
		uint8_t keys[static_cast<size_t>(Key::Count)] = {};

		uint8_t mouse[static_cast<size_t>(MouseButton::Count)] = {};
		glm::vec2 mouse_pos = {};
		glm::vec2 mouse_delta = {};
		glm::vec2 last_mouse_pos = {};
	};
}
