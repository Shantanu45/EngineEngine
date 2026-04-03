#pragma once
#include "event/event.h"

namespace EE
{
	class WindowResizeEvent : public EE::Event
	{
	public:
		EE_EVENT_TYPE_DECL(WindowResizeEvent)

		WindowResizeEvent(uint32_t width, uint32_t height)
			: EE::Event(get_type_id()), width(width), height(height) {}

		uint32_t width;
		uint32_t height;
	};
}
