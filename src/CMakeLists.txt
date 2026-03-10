set(PROJECT_NAME Main)

set(UNGROUPED_FILES
	main.cpp
)

source_group("" FILES ${UNGROUPED_FILES})

set(ALL_FILES
	${UNGROUPED_FILES}
)

add_executable(${PROJECT_NAME}
	${ALL_FILES}
)

setup_msvc_options(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE ASSET_DIRECTORY=\"${PROJECT_SOURCE_DIR}/assets\")

find_package(SDL3 REQUIRED)

set(INCLUDES 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
	"${PROJECT_SOURCE_DIR}/third_party/Vulkan-Headers/include"
	"${PROJECT_SOURCE_DIR}/third_party/volk"
)

target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

set(ADDITIONAL_LIBRARY_DEPENDENCIES
	spdlog::spdlog
	libassert::assert
	sdl::sdl
	volk
)

target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")