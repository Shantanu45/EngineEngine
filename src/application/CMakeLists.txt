set(PROJECT_NAME application)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

add_library(${PROJECT_NAME} STATIC 
${SOURCES} 
${HEADERS}
)

setup_msvc_options(${PROJECT_NAME})

add_subdirectory(application_entry)

find_package(Tracy CONFIG REQUIRED)
find_package(SDL3 REQUIRED)
find_package(xxHash REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(stb REQUIRED)
find_package(imgui REQUIRED)
find_package(CLI11 REQUIRED)

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
	volk::volk
	sdl::sdl
	input
	vulkan
	xxHash::xxhash
	TinyGLTF::TinyGLTF
	stb::stb
	imgui::imgui
	event
	CLI11::CLI11
)

target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES} Tracy::TracyClient)