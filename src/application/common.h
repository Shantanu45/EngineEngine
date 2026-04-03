#pragma once

#define VOLK_IMPLEMENTATION
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include "vma/vk_mem_alloc.h"
#include "application/application_entry/application_entry.h"
#include "application/application.h"
#include "libassert/assert.hpp"

