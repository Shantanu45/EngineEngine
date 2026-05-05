set(PROJECT_NAME vulkan)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

file(GLOB IMGUI_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/third_party/imgui/backends/*.cpp")
file(GLOB IMGUI_HEADERS CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/third_party/imgui/backends/*.h")

add_library(${PROJECT_NAME} STATIC 
${SOURCES} 
${HEADERS}
${IMGUI_SOURCES}
${IMGUI_HEADERS}
)

setup_msvc_options(${PROJECT_NAME})

add_definitions(-DNOMINMAX) 
target_compile_definitions(${PROJECT_NAME} PRIVATE VK_NO_PROTOTYPES)
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_VULKAN_USE_VOLK)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)

find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(shaderc REQUIRED)
find_package(xxHash REQUIRED)
find_package(spirv-reflect-static REQUIRED)
find_package(imgui REQUIRED)
find_package(Tracy CONFIG REQUIRED)


set(INCLUDES 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
	"$ENV{VULKAN_SDK}/Include"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

set(ADDITIONAL_LIBRARY_DEPENDENCIES
	spdlog::spdlog
	libassert::assert
	sdl::sdl
	glm::glm
	volk::volk
	GPUOpen::VulkanMemoryAllocator
	shaderc::shaderc
	re-spirv
	xxHash::xxhash
	spirv-reflect-static
	filesystem
	compiler
	rendering
	imgui::imgui
	Tracy::TracyClient
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})