set(PROJECT_NAME Main)

set(UNGROUPED_FILES
	main.cpp
)

source_group("" FILES ${UNGROUPED_FILES})

set(ALL_FILES
	${UNGROUPED_FILES}
)

add_executable(${PROJECT_NAME} WIN32
	${ALL_FILES}
)

add_subdirectory(util)
add_subdirectory(application)
add_subdirectory(event)
add_subdirectory(input)
add_subdirectory(vulkan)
add_subdirectory(math)
add_subdirectory(filesystem)
add_subdirectory(compiler)

setup_msvc_options(${PROJECT_NAME})

find_package(SDL3 REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(glm REQUIRED)
find_package(xxHash REQUIRED)
find_package(spirv-reflect-static REQUIRED)


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
)

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")