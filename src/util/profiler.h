#pragma once

#ifdef TRACY_ENABLE
#include "volk.h"
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#include <cstdlib>
#include <new>
#define ALLOC_MARK(ptr, size, name) TracyAllocN(ptr, size, name)
#define FREE_MARK(ptr, name)        TracyFreeN(ptr, name)
#define TRACY_RESOURCE_OPERATORS(PoolName)                              \
    static void* operator new(std::size_t size) {                       \
        void* p = malloc(size);                                         \
        if (!p) throw std::bad_alloc{};                                 \
        TracyAllocN(p, size, PoolName);                                 \
        return p;                                                       \
    }                                                                   \
    static void operator delete(void* p) noexcept {                    \
        TracyFreeN(p, PoolName);                                        \
        free(p);                                                        \
    }
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
#define TracyAllocNS(ptr, size, depth, name)
#define TracyFreeNS(ptr, depth, name)
#define ALLOC_MARK(ptr, size, name)
#define FREE_MARK(ptr, name)
#define TRACY_RESOURCE_OPERATORS(PoolName)
// Plots
#define TracyPlot(name, val)
#define TracyPlotConfig(name, type, step, fill, color)
// Vulkan GPU zones
#define TracyVkContext(physdev, dev, queue, cmdbuf)  nullptr
#define TracyVkDestroy(ctx)
#define TracyVkCollect(ctx, cmdbuf)
#define TracyVkZone(ctx, cmdbuf, name)
#define TracyVkNamedZone(ctx, varname, cmdbuf, name, active)
#endif
