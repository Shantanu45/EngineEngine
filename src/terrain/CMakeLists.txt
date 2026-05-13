set(TERRAIN_CORE_NAME terrain_core)

add_library(${TERRAIN_CORE_NAME} STATIC
	terrain_generator.cpp
	terrain_generator.h
)

setup_msvc_options(${TERRAIN_CORE_NAME})

find_package(glm REQUIRED)
find_package(xxHash REQUIRED)
find_package(spdlog REQUIRED)
find_package(libassert REQUIRED)
find_package(stb REQUIRED)
find_package(TinyGLTF REQUIRED)

add_definitions(-DNOMINMAX) 

target_include_directories(${TERRAIN_CORE_NAME} PRIVATE
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
)

target_link_libraries(${TERRAIN_CORE_NAME} PRIVATE
	spdlog::spdlog
	libassert::assert
	glm::glm
	xxHash::xxhash
	stb::stb
	TinyGLTF::TinyGLTF
	rendering
	util
)

set(PROJECT_NAME terrain)

add_executable(${PROJECT_NAME} WIN32
	terrain.cpp
	terrain_runtime.cpp
	terrain_runtime.h
)

setup_msvc_options(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)

find_package(Tracy CONFIG REQUIRED)
find_package(SDL3 REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(glm REQUIRED)
find_package(xxHash REQUIRED)
find_package(spirv-reflect-static REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(stb REQUIRED)
find_package(imgui REQUIRED)
find_package(EnTT REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
	"${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include"
	"${PROJECT_SOURCE_DIR}/third_party/volk"
	"$ENV{VULKAN_SDK}/Include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
	spdlog::spdlog
	libassert::assert
	sdl::sdl
	volk
	util
	application
	application_entry
	event
	input
	vulkan
	GPUOpen::VulkanMemoryAllocator
	glm::glm
	xxHash::xxhash
	filesystem
	compiler
	spirv-reflect-static
	TinyGLTF::TinyGLTF
	stb::stb
	imgui::imgui
	EnTT::EnTT
	Tracy::TracyClient
	terrain_core
)
