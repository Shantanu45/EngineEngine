set(PROJECT_NAME tutorial)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

add_executable(${PROJECT_NAME} WIN32 
${SOURCES} 
${HEADERS}
)

setup_msvc_options(${PROJECT_NAME})

find_package(SDL3 REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(glm REQUIRED)
find_package(xxHash REQUIRED)
find_package(spirv-reflect-static REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(stb REQUIRED)
find_package(imgui REQUIRED)

set(INCLUDES 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
	"${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include"
	"${PROJECT_SOURCE_DIR}/third_party/volk"
	"$ENV{VULKAN_SDK}/Include"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

set(ADDITIONAL_LIBRARY_DEPENDENCIES
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
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})