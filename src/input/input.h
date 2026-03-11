#pragma once
#include <cstdint>
#include "SDL3/SDL.h"

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
		KEY_DOWN = 1 << 0,  // 0000 0001 — key is currently held
		KEY_PRESSED = 1 << 1,  // 0000 0010 — went down this frame
		KEY_RELEASED = 1 << 2,  // 0000 0100 — went up this frame
	};

	enum class MouseButton
	{
		Left,
		Middle,
		Right,
		Count
	};

	class InputSystem
	{
	public:
		InputSystem();

		void on_sdl_event(const SDL_Event& e);

		bool is_held(Key k)      const { return keys[static_cast<size_t>(k)] & KEY_DOWN; }
		bool just_pressed(Key k) const { return keys[static_cast<size_t>(k)] & KEY_PRESSED; }
		bool just_released(Key k)const { return keys[static_cast<size_t>(k)] & KEY_RELEASED; }

	private:

		Key sdl_to_key[SDL_SCANCODE_COUNT];
		uint8_t keys[static_cast<size_t>(Key::Count)] = {};
	};
}
