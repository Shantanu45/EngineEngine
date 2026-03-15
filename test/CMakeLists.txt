set(TEST_PROJECT_NAME Test)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

add_executable(${TEST_PROJECT_NAME}
${SOURCES} 
${HEADERS}
)

find_package(GTest REQUIRED)
find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(xxHash REQUIRED)


# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

setup_msvc_options(${TEST_PROJECT_NAME})

set(TEST_INCLUDE 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
	"${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include"
	"${PROJECT_SOURCE_DIR}/third_party/volk"
)

target_include_directories(${TEST_PROJECT_NAME} PRIVATE ${TEST_INCLUDE})

# Link
set(ADDITIONAL_LIBRARY_DEPENDENCIES
	gtest::gtest
	spdlog::spdlog
	sdl::sdl
	volk
	libassert::assert
	util
	glm::glm
	vulkan	glm::glm
	volk::volk
	GPUOpen::VulkanMemoryAllocator
	application
	application_entry
	xxHash::xxhash

)

target_link_libraries(${TEST_PROJECT_NAME} PUBLIC ${ADDITIONAL_LIBRARY_DEPENDENCIES})

