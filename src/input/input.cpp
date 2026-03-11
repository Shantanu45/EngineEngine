#include "input.h"
#include <algorithm>

namespace EE
{
	InputSystem::InputSystem()
	{
		std::fill(sdl_to_key, sdl_to_key + SDL_SCANCODE_COUNT, Key::Unknown);

		// Letters
		sdl_to_key[SDL_SCANCODE_A] = Key::A;
		sdl_to_key[SDL_SCANCODE_B] = Key::B;
		sdl_to_key[SDL_SCANCODE_C] = Key::C;
		sdl_to_key[SDL_SCANCODE_D] = Key::D;
		sdl_to_key[SDL_SCANCODE_E] = Key::E;
		sdl_to_key[SDL_SCANCODE_F] = Key::F;
		sdl_to_key[SDL_SCANCODE_G] = Key::G;
		sdl_to_key[SDL_SCANCODE_H] = Key::H;
		sdl_to_key[SDL_SCANCODE_I] = Key::I;
		sdl_to_key[SDL_SCANCODE_J] = Key::J;
		sdl_to_key[SDL_SCANCODE_K] = Key::K;
		sdl_to_key[SDL_SCANCODE_L] = Key::L;
		sdl_to_key[SDL_SCANCODE_M] = Key::M;
		sdl_to_key[SDL_SCANCODE_N] = Key::N;
		sdl_to_key[SDL_SCANCODE_O] = Key::O;
		sdl_to_key[SDL_SCANCODE_P] = Key::P;
		sdl_to_key[SDL_SCANCODE_Q] = Key::Q;
		sdl_to_key[SDL_SCANCODE_R] = Key::R;
		sdl_to_key[SDL_SCANCODE_S] = Key::S;
		sdl_to_key[SDL_SCANCODE_T] = Key::T;
		sdl_to_key[SDL_SCANCODE_U] = Key::U;
		sdl_to_key[SDL_SCANCODE_V] = Key::V;
		sdl_to_key[SDL_SCANCODE_W] = Key::W;
		sdl_to_key[SDL_SCANCODE_X] = Key::X;
		sdl_to_key[SDL_SCANCODE_Y] = Key::Y;
		sdl_to_key[SDL_SCANCODE_Z] = Key::Z;

		// Numbers (top row)
		sdl_to_key[SDL_SCANCODE_0] = Key::_0;
		sdl_to_key[SDL_SCANCODE_1] = Key::_1;
		sdl_to_key[SDL_SCANCODE_2] = Key::_2;
		sdl_to_key[SDL_SCANCODE_3] = Key::_3;
		sdl_to_key[SDL_SCANCODE_4] = Key::_4;
		sdl_to_key[SDL_SCANCODE_5] = Key::_5;
		sdl_to_key[SDL_SCANCODE_6] = Key::_6;
		sdl_to_key[SDL_SCANCODE_7] = Key::_7;
		sdl_to_key[SDL_SCANCODE_8] = Key::_8;
		sdl_to_key[SDL_SCANCODE_9] = Key::_9;

		// Navigation
		sdl_to_key[SDL_SCANCODE_UP] = Key::Up;
		sdl_to_key[SDL_SCANCODE_DOWN] = Key::Down;
		sdl_to_key[SDL_SCANCODE_LEFT] = Key::Left;
		sdl_to_key[SDL_SCANCODE_RIGHT] = Key::Right;

		// Common
		sdl_to_key[SDL_SCANCODE_SPACE] = Key::Space;
		sdl_to_key[SDL_SCANCODE_RETURN] = Key::Return;
		sdl_to_key[SDL_SCANCODE_ESCAPE] = Key::Escape;
		sdl_to_key[SDL_SCANCODE_LSHIFT] = Key::LeftShift;
		sdl_to_key[SDL_SCANCODE_LCTRL] = Key::LeftCtrl;
		sdl_to_key[SDL_SCANCODE_LALT] = Key::LeftAlt;
	}

	void InputSystem::on_sdl_event(const SDL_Event& e)
	{
		switch (e.type)
		{
		case SDL_EVENT_KEY_DOWN:
			if (!e.key.repeat)
			{
				Key k = sdl_to_key[e.key.scancode];
				if (k != Key::Unknown)
					keys[static_cast<size_t>(k)] |= (KeyState::KEY_DOWN | KeyState::KEY_PRESSED);
			}
			break;

		case SDL_EVENT_KEY_UP:
		{
			Key k = sdl_to_key[e.key.scancode];
			if (k != Key::Unknown)
			{
				keys[static_cast<size_t>(k)] &= ~(KeyState::KEY_DOWN);
				keys[static_cast<size_t>(k)] |= KeyState::KEY_RELEASED;
			}
			break;
		}
		}
	}
}
