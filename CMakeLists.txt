cmake_minimum_required(VERSION 3.14)
include(FetchContent)

#================================#
# Vulkan Launchpad Project       #
#================================#

project(VulkanLaunchpad VERSION 0.1.0 LANGUAGES CXX)

set(VKL_EXTERNAL_FOLDER "Dependencies")
set(VKL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(VKL_EXTERNAL_DIR "${VKL_SOURCE_DIR}/external")

set(SLANG_VERSION "2025.19.1")
if(WIN32)
	set(SLANG_OS "windows-x86_64")
elseif(UNIX)
	set(SLANG_OS "linux-x86_64")
endif()

if (NOT APPLE)
	FetchContent_Declare(slang
		URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}.zip
		DOWNLOAD_EXTRACT_TIMESTAMP false
	)
	FetchContent_MakeAvailable(slang)
	message(STATUS "Slang dir: ${slang_SOURCE_DIR}")
else ()
	# Since we build a universal binary for apple, we need to download both architectures
	# of slang and merge them together using lipo
	set(SLANG_OS "macos-aarch64")
	FetchContent_Declare(slang-arm
		URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}.zip
		DOWNLOAD_EXTRACT_TIMESTAMP false
	)
	FetchContent_MakeAvailable(slang-arm)
	message(STATUS "Slang arm64 dir: ${slang-arm_SOURCE_DIR}")

	set(SLANG_OS "macos-x86_64")
	FetchContent_Declare(slang-x86
		URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}.zip
		DOWNLOAD_EXTRACT_TIMESTAMP false
	)
	FetchContent_MakeAvailable(slang-x86)
	message(STATUS "Slang x86_64 dir: ${slang-x86_SOURCE_DIR}")
endif ()

add_library(slang INTERFACE)
add_library(slang::slang-compiler SHARED IMPORTED GLOBAL)
add_library(slang::slang-glslang MODULE IMPORTED GLOBAL)

if(WIN32)
	target_include_directories(slang
			INTERFACE ${slang_SOURCE_DIR}/include
	)

	set_target_properties(slang::slang-compiler PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang.dll"
			IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang.lib"
	)

	set_target_properties(slang::slang-glslang PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/slang-glslang.dll"
	)
elseif(APPLE)
	set(slang_SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/slang-universal/")

	function (create_universal_slang_lib lib_name)
		set(LIB_PATH "/lib/${lib_name}.dylib")
		add_custom_command(
				OUTPUT "${slang_SOURCE_DIR}${LIB_PATH}"
				COMMAND lipo -create
					"${slang-arm_SOURCE_DIR}${LIB_PATH}"
					"${slang-x86_SOURCE_DIR}${LIB_PATH}"
					-output "${slang_SOURCE_DIR}${LIB_PATH}"
				DEPENDS "${slang-arm_SOURCE_DIR}${LIB_PATH}" "${slang-x86_SOURCE_DIR}${LIB_PATH}"
				COMMENT "Creating universal ${lib_name}.dylib"
		)
	endfunction ()

	create_universal_slang_lib("libslang")
	create_universal_slang_lib("libslang-glslang")

	add_custom_target(slang-universal ALL
			DEPENDS "${slang_SOURCE_DIR}/lib/libslang.dylib" "${slang_SOURCE_DIR}/lib/libslang-glslang.dylib"
	)

	target_include_directories(slang
			INTERFACE ${slang-arm_SOURCE_DIR}/include
	)

	add_dependencies(slang::slang-compiler slang-universal)
	set_target_properties(slang::slang-compiler PROPERTIES
			IMPORTED_LOCATION ${slang_SOURCE_DIR}/lib/libslang.dylib
	)

	add_dependencies(slang::slang-glslang slang-universal)
	set_target_properties(slang::slang-glslang PROPERTIES
			IMPORTED_LOCATION ${slang_SOURCE_DIR}/lib/libslang-glslang.dylib
	)
elseif(UNIX)
	target_include_directories(slang
			INTERFACE ${slang_SOURCE_DIR}/include
	)

	set_target_properties(slang::slang-compiler PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/libslang.so"
	)

	set_target_properties(slang::slang-glslang PROPERTIES
			IMPORTED_LOCATION "${slang_SOURCE_DIR}/lib/libslang-glslang.so"
	)
endif()
target_link_libraries(slang INTERFACE slang::slang-compiler)

#================================#
# Vulkan Launchpad Compiler      #
#================================#

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /Qpar")
endif()

#================================#
# Vulkan Launchpad Dependencies  #
#================================#

find_package(Vulkan REQUIRED)
set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_TEST OFF)
set(VKL_GLM_VERSION "1.0.2" CACHE STRING "VulkanLaunchpad glm git tag")
set(VKL_SDL_VERSION "release-3.2.26" CACHE STRING "VulkanLaunchpad SDL git tag")

message(DEBUG "glm version: ${VKL_GLM_VERSION}")
FetchContent_Declare(
	glm
	URL "https://github.com/g-truc/glm/archive/refs/tags/${VKL_GLM_VERSION}.zip"
	DOWNLOAD_EXTRACT_TIMESTAMP false
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glm)

message(DEBUG "SDL version: ${VKL_SDL_VERSION}")
FetchContent_Declare(
		sdl3
		URL "https://github.com/libsdl-org/SDL/archive/refs/tags/${VKL_SDL_VERSION}.zip"
		DOWNLOAD_EXTRACT_TIMESTAMP false
		EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(sdl3)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#================================#
# Vulkan Launchpad Target        #
#================================#

set(VKL_HEADER_FILES "VulkanLaunchpad.h" "Camera.h")
set(VKL_SOURCE_FILES "VulkanLaunchpad.cpp" "Camera.cpp")
source_group("Header Files" FILES ${VKL_HEADER_FILES})
source_group("Source Files" FILES ${VKL_SOURCE_FILES})

add_library(${PROJECT_NAME} STATIC ${VKL_HEADER_FILES} ${VKL_SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}
	PUBLIC Vulkan::Vulkan glm::glm SDL3::SDL3
	PRIVATE slang
)
target_include_directories(${PROJECT_NAME} 
	INTERFACE "${VKL_SOURCE_DIR}"
	PUBLIC "${VKL_EXTERNAL_DIR}/include"
)
add_dependencies(${PROJECT_NAME} slang)

#==================================#
# Unset not needed cache variables #
#==================================#

unset(ENABLE_OPT CACHE)

unset(LLVM_USE_CRT_DEBUG CACHE)
unset(LLVM_USE_CRT_MINSIZEREL CACHE)
unset(LLVM_USE_CRT_RELEASE CACHE)
unset(LLVM_USE_CRT_RELWITHDEBINFO CACHE)

unset(BUILD_EXTERNAL CACHE)
unset(BUILD_SHARED_LIBS CACHE)
unset(BUILD_TEMPLATE CACHE)
unset(USE_CCACHE CACHE)
unset(USE_MSVC_RUNTIME_LIBRARY_DLL CACHE)
unset(OVERRIDE_MSVCCRT CACHE)
