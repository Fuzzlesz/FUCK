cmake_minimum_required(VERSION 3.20)
set(NAME "FUCK" CACHE STRING "")
set(VERSION 1.1.4 CACHE STRING "")

# ---- Options ----

option(COPY_BUILD "Copy the build output to the Skyrim directory." TRUE)

# ---- Cache build vars ----

macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

macro(find_commonlib_path)
	if (CommonLibName AND NOT ${CommonLibName} STREQUAL "")
		# Check extern
		find_path(CommonLibPath
		include/REL/Relocation.h
		PATHS extern/${CommonLibName})
		if (${CommonLibPath} STREQUAL "CommonLibPath-NOTFOUND")
			#Check path
			set_from_environment(${CommonLibName}Path)
			set(CommonLibPath ${${CommonLibName}Path})
		endif()
	endif()
endmacro()

set_from_environment(VCPKG_ROOT)
# One universal DLL: CommonLibVR's default (ng) branch is CommonLibSSE-NG-based
# with all three runtimes enabled, so the same binary loads on SE, AE, and VR.
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
set(CommonLibName "CommonLibVR")
set(SKSE_SUPPORT_XBYAK ON)
set(BUILD_TESTS OFF)
set_from_environment(Skyrim64Path)
set(SkyrimPath ${Skyrim64Path})
set(SkyrimVersion "Skyrim SSE/AE/VR")
find_commonlib_path()
message(
	STATUS
	"Building ${NAME} ${VERSION} for ${SkyrimVersion} at ${SkyrimPath} with ${CommonLibName} at ${CommonLibPath}."
)

if (DEFINED VCPKG_ROOT)
	set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
	set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
else ()
	message(
		WARNING
		"Variable VCPKG_ROOT is not set. Continuing without vcpkg."
	)
endif ()

set(Boost_USE_STATIC_RUNTIME OFF CACHE BOOL "")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "")

# ---- Project ----

project(
	${NAME}
	VERSION ${VERSION}
	LANGUAGES CXX
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(
		FATAL_ERROR
			"In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
	)
endif()

# ---- Globals ----

add_compile_definitions(
	SKSE_SUPPORT_XBYAK
)

if (MSVC)
	if (NOT ${CMAKE_GENERATOR} STREQUAL "Ninja")
		add_compile_options(
			/MP	# Build with Multiple Processes
		)
	endif ()
endif ()

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF)

set(Boost_USE_STATIC_LIBS ON)

# ---- Dependencies ----
if (DEFINED CommonLibPath AND NOT ${CommonLibPath} STREQUAL "" AND IS_DIRECTORY ${CommonLibPath})
	add_subdirectory(${CommonLibPath} ${CommonLibName})
	if (NOT TARGET CommonLibVR)
		# CommonLibVR (ng branch) keeps upstream CommonLibSSE-NG's project name
		add_library(CommonLibVR::CommonLibVR ALIAS CommonLibSSE)
	endif()
else ()
	message(
		FATAL_ERROR
		"Variable ${CommonLibName}Path is not set or in extern/."
	)
endif()

find_package(directxtex CONFIG REQUIRED)
find_package(Freetype REQUIRED)
find_package(glaze CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(rapidfuzz CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)

find_path(SRELL_INCLUDE_DIRS "srell.hpp")
find_path(CLIB_UTIL_INCLUDE_DIRS "ClibUtil/utils.hpp")

# ImGuiVRHelper client SDK: mirrors the menu into the helper's flat in-scene
# panel and drives it with the wand in VR. Header-only INTERFACE target compiled
# into this plugin; inert at runtime when the helper DLL is absent (flat screen).
find_package(nlohmann_json CONFIG REQUIRED)
include(FetchContent)
FetchContent_Declare(
	ImGuiVRHelper
	GIT_REPOSITORY https://github.com/alandtse/imgui-vr-helper.git
	GIT_TAG        v1.5.2
	GIT_SUBMODULES ""
	SOURCE_SUBDIR  api
)
FetchContent_MakeAvailable(ImGuiVRHelper)

# ---- Add source files ----

include(cmake/headerlist.cmake)
include(cmake/sourcelist.cmake)

source_group(
	TREE
		${CMAKE_CURRENT_SOURCE_DIR}
	FILES
		${headers}
		${sources}
)

source_group(
	TREE
		${CMAKE_CURRENT_BINARY_DIR}
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
)

# ---- Create DLL ----

add_library(
	${PROJECT_NAME}
	SHARED
	${headers}
	${sources}
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	.clang-format
	.editorconfig
)

target_compile_features(
	${PROJECT_NAME}
	PRIVATE
		cxx_std_23
)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		_UNICODE
)

target_include_directories(
	${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${SRELL_INCLUDE_DIRS}
		${CLIB_UTIL_INCLUDE_DIRS}
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		${CommonLibName}::${CommonLibName}
		Microsoft::DirectXTex
		Freetype::Freetype
		glaze::glaze
		imgui::imgui
		rapidfuzz::rapidfuzz
		unordered_dense::unordered_dense
)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		ImGuiVRHelper::api
		nlohmann_json::nlohmann_json
)

target_precompile_headers(
	${PROJECT_NAME}
	PRIVATE
		src/PCH.h
)

if (MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/sdl             # Enable Additional Security Checks
			/utf-8           # Set Source and Executable character sets to UTF-8
			/Zi              # Debug Information Format

			/permissive-     # Standards conformance
			/Zc:preprocessor # Enable preprocessor conformance mode

			/wd4200          # nonstandard extension used : zero-sized array in struct/union

			"$<$<CONFIG:DEBUG>:>"
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif ()

# ---- Post build ----

if (COPY_BUILD)
	# One universal DLL: deploy to every runtime install whose path is set
	set_from_environment(SkyrimVRPath)
	set(DeployPaths ${SkyrimPath})
	if (DEFINED SkyrimVRPath AND NOT "${SkyrimVRPath}" STREQUAL "" AND NOT "${SkyrimVRPath}" STREQUAL "${SkyrimPath}")
		list(APPEND DeployPaths ${SkyrimVRPath})
	endif ()
	if (DeployPaths)
		foreach(DeployPath ${DeployPaths})
			add_custom_command(
				TARGET ${PROJECT_NAME}
				POST_BUILD
				COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${DeployPath}/SKSE/Plugins/
				COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_PDB_FILE:${PROJECT_NAME}> ${DeployPath}/SKSE/Plugins/
			)
		endforeach()
	else ()
		message(
			WARNING
			"Variables Skyrim64Path/SkyrimVRPath are not defined. Skipping post-build copy command."
		)
	endif ()
endif ()
