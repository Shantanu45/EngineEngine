set(PROJECT_NAME math)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

add_library(${PROJECT_NAME} STATIC 
${SOURCES} 
${HEADERS}
)

setup_msvc_options(${PROJECT_NAME})

add_definitions(-DNOMINMAX) 

find_package(glm REQUIRED)
find_package(xxHash REQUIRED)

set(INCLUDES 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/third_party"
)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

set(ADDITIONAL_LIBRARY_DEPENDENCIES
	spdlog::spdlog
	libassert::assert
	glm::glm
	xxHash::xxhash
	util
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})