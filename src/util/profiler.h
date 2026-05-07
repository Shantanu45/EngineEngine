#pragma once

#ifdef TRACY_ENABLE
#include "volk.h"
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#else
// CPU zones
#define FrameMark
#define ZoneScoped
#define ZoneScopedN(name)
// Allocations
#define TracyAlloc(ptr, size)
#define TracyFree(ptr)
#define TracyAllocN(ptr, size, name)
#define TracyFreeN(ptr, name)
// Plots
#define TracyPlot(name, val)
// Vulkan GPU zones
#define TracyVkContext(physdev, dev, queue, cmdbuf)  nullptr
#define TracyVkDestroy(ctx)
#define TracyVkCollect(ctx, cmdbuf)
#define TracyVkZone(ctx, cmdbuf, name)
#define TracyVkNamedZone(ctx, varname, cmdbuf, name, active)
#endif
