#include "util/profiler.h"

#ifdef TRACY_ENABLE
#include <cstdlib>
#include <new>

void* operator new(std::size_t size) {
    void* p = malloc(size);
    if (!p) throw std::bad_alloc{};
    TracyAlloc(p, size);
    return p;
}

void* operator new[](std::size_t size) {
    void* p = malloc(size);
    if (!p) throw std::bad_alloc{};
    TracyAlloc(p, size);
    return p;
}

void operator delete(void* p) noexcept {
    TracyFree(p);
    free(p);
}

void operator delete[](void* p) noexcept {
    TracyFree(p);
    free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    TracyFree(p);
    free(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    TracyFree(p);
    free(p);
}
#endif
