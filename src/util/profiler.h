#pragma once

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define FrameMark
#define ZoneScoped
#define ZoneScopedN(name)
#define TracyAlloc(ptr, size)
#define TracyFree(ptr)
#define TracyPlot(name, val)
#endif
