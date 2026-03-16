set(PROJECT_NAME filesystem)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

add_library(${PROJECT_NAME} STATIC 
${SOURCES} 
${HEADERS}
)

setup_msvc_options(${PROJECT_NAME})

find_package(libassert REQUIRED)

# allows us to use std::max when Windows.h is included
add_definitions(-DNOMINMAX) 
target_compile_definitions(${PROJECT_NAME} PRIVATE MAKER_DEFAULT_BUILTIN_DIRECTORY=\"${PROJECT_SOURCE_DIR}/assets\")
target_compile_definitions(${PROJECT_NAME} PRIVATE MAKER_DEFAULT_CACHE_DIRECTORY=\"${CMAKE_BINARY_DIR}/cache\")


set(INCLUDES 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

set(ADDITIONAL_LIBRARY_DEPENDENCIES
	libassert::assert
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})