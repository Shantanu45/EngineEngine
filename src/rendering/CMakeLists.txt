################################################################################
# bc7enc_lib — BC7 block encoder (subset of bc7enc_rdo, no lodepng/miniz)
################################################################################
add_library(bc7enc_lib STATIC
    ${PROJECT_SOURCE_DIR}/third_party/bc7enc_rdo/bc7enc.cpp
)
target_include_directories(bc7enc_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/third_party/bc7enc_rdo
)
if(MSVC)
    target_compile_options(bc7enc_lib PRIVATE /W0 /wd4127)
endif()

set(PROJECT_NAME rendering)

file(GLOB SOURCES CONFIGURE_DEPENDS "*.cpp")
file(GLOB HEADERS CONFIGURE_DEPENDS "*.h")

file(GLOB FrameGraph CONFIGURE_DEPENDS "fg/*.cpp")
file(GLOB FrameGraphHeaders CONFIGURE_DEPENDS "fg/*.h")
file(GLOB FrameGraphInl CONFIGURE_DEPENDS "fg/*.inl")

set(FG 
    ${FrameGraph}
    ${FrameGraphHeaders}
    ${FrameGraphInl}
)

source_group("framegraph" FILES ${FG})

file(GLOB RenderPassesCpp CONFIGURE_DEPENDS "render_passes/*.cpp")
file(GLOB RenderPassesHeader CONFIGURE_DEPENDS "render_passes/*.h")

set(RP
    ${RenderPassesCpp}
    ${RenderPassesHeader}
)

source_group("render_passes" FILES ${RP})

file(GLOB RenderersCpp CONFIGURE_DEPENDS "renderers/*.cpp")
file(GLOB RenderersHeader CONFIGURE_DEPENDS "renderers/*.h")

set(RENDERERS
    ${RenderersCpp}
    ${RenderersHeader}
)

source_group("renderers" FILES ${RENDERERS})

add_library(${PROJECT_NAME} STATIC
${SOURCES}
${HEADERS}
${FG}
${RP}
${RENDERERS}
)

setup_msvc_options(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)

add_definitions(-DNOMINMAX) 

find_package(SDL3 REQUIRED)
find_package(glm REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)
find_package(shaderc REQUIRED)
find_package(xxHash REQUIRED)
find_package(spirv-reflect-static REQUIRED)
find_package(TinyGLTF REQUIRED)
find_package(stb REQUIRED)
find_package(imgui REQUIRED)
find_package(EnTT REQUIRED)
find_package(Tracy REQUIRED)


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
	TinyGLTF::TinyGLTF
	stb::stb
	imgui::imgui
	util
	event
	EnTT::EnTT
	Tracy::TracyClient
	bc7enc_lib
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ADDITIONAL_LIBRARY_DEPENDENCIES})