set(PROJECT_NAME compiler)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

add_library(${PROJECT_NAME} STATIC 
${SOURCES} 
${HEADERS}
)

setup_msvc_options(${PROJECT_NAME})

find_package(shaderc REQUIRED)
find_package(xxHash REQUIRED)
find_package(spirv-reflect-static REQUIRED)

# allows us to use std::max when Windows.h is included
add_definitions(-DNOMINMAX) 


set(INCLUDES 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

set(ADDITIONAL_LIBRARY_DEPENDENCIES
	libassert::assert
	shaderc::shaderc
	spirv-tools::spirv-tools
	xxHash::xxhash
	spirv-reflect-static
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})