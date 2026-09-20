cmake_minimum_required(VERSION 3.22)

project(
	HighlightLootableCorpses
	VERSION 2.0.0
	LANGUAGES CXX
)

set(PROJECT_NAMESPACE Amazing::${PROJECT_NAME})
set(PROJECT_AUTHOR AmazingBuff)

option(ENABLE_SKYRIM_SE "Enable support for Skyrim SE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_AE "Enable support for Skyrim AE in the dynamic runtime feature." ON)
option(ENABLE_SKYRIM_VR "Enable support for Skyrim VR in the dynamic runtime feature." OFF)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(cmake/packaging.cmake)

file(GLOB_RECURSE SOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HEADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)

set(RENDER_SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders)
set(RENDER_SHADER_EMBED
    icon_overlay.hlsl:IconOverlay
    mask_geometry.hlsl:MaskGeometry
    mask_composite.hlsl:MaskComposite
    mask_glow.hlsl:MaskGlow
)
set(RENDER_SHADER_FILES "")
set(GENERATED_SHADER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/render/shader_sources.h)
foreach(RENDER_SHADER_ENTRY ${RENDER_SHADER_EMBED})
    string(REPLACE ":" ";" RENDER_SHADER_PAIR ${RENDER_SHADER_ENTRY})
    list(GET RENDER_SHADER_PAIR 0 RENDER_SHADER_FILE)
    list(APPEND RENDER_SHADER_FILES ${RENDER_SHADER_DIR}/${RENDER_SHADER_FILE})
endforeach()
add_custom_command(
    OUTPUT ${GENERATED_SHADER_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -DSHADER_DIR=${RENDER_SHADER_DIR}
        -DOUTPUT_HEADER=${GENERATED_SHADER_HEADER}
        "-DEMBED_ENTRIES=${RENDER_SHADER_EMBED}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
    DEPENDS ${RENDER_SHADER_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
    COMMENT "Embedding HLSL sources (render/shaders)"
    VERBATIM
)

set(VERSION_HEADER ${CMAKE_CURRENT_BINARY_DIR}/src/plugin.h)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/plugin.h.in
	${VERSION_HEADER}
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

source_group(Source FILES ${SOURCE_FILES})
source_group(Header FILES ${HEADER_FILES} ${VERSION_HEADER})

add_library(
	${PROJECT_NAME}
	SHARED
	${HEADER_FILES}
	${SOURCE_FILES}
	${GENERATED_SHADER_HEADER}
	${VERSION_HEADER}
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
	cxx_std_23
)

# Align the MSVC toolset with the one vcpkg uses for dependencies (spdlog).
# CMake 3.31 drops the `version=` clause from CMAKE_GENERATOR_TOOLSET (the
# preset's `v143,version=14.44.35207` arrives here as a plain `v143`), which
# resolves to an older installed toolset (14.38) whose STL lacks the vectorized
# symbols 14.44 objects reference — spdlog.lib then fails to link with LNK2019
# on __std_find_*_trivial_pos_1. Pin the toolset per-target so every generated
# .vcxproj uses 14.44 regardless of the generator toolset clause.
set_target_properties(
	${PROJECT_NAME}
	PROPERTIES
	VS_GLOBAL_VCToolsVersion "14.44.35207"
)

if(${CMAKE_CXX_COMPILER_ID} STREQUAL MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
		/sdl	# Enable Additional Security Checks
		/utf-8	# Set Source and Executable character sets to UTF-8
		/Zi	# Debug Information Format

		/permissive-	# Standards conformance
		/Zc:preprocessor	# Enable preprocessor conformance mode

		/wd4200 # nonstandard extension used : zero-sized array in struct/union

		$<$<CONFIG:DEBUG>:/ZI>
		$<$<CONFIG:RELEASE>:/Zi;/Zc:inline;/JMC-;/Ob3>
	)

	target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
		NOMINMAX
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
		$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>
		$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>
	)
endif()

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/src
	${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(BUILD_TESTS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/SKSE-MCP)

find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(simpleini CONFIG REQUIRED)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
	spdlog::spdlog
	fmt::fmt
	SimpleIni::SimpleIni
	CommonLibSSE::CommonLibSSE
	SKSE-MCP::SKSE-MCP
	d3d11
	dxgi
	d3dcompiler
)

target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h
)
