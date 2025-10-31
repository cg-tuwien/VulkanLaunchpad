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
elseif(APPLE)
	set(SLANG_OS "macos-aarch64")
elseif(UNIX)
	set(SLANG_OS "linux-x86_64")
endif()

FetchContent_Populate(slang_zip URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_OS}.zip QUIET)
message(STATUS "Slang dir: ${slang_zip_SOURCE_DIR}")
add_library(slang SHARED IMPORTED GLOBAL)
if(WIN32)
	set_target_properties(slang PROPERTIES
			IMPORTED_LOCATION_SLANG  ${slang_zip_SOURCE_DIR}/bin/slang.dll
			IMPORTED_LOCATION_SLANG_GLSLANG ${slang_zip_SOURCE_DIR}/bin/slang-glslang.dll
			IMPORTED_IMPLIB ${slang_zip_SOURCE_DIR}/lib/slang.lib
			INTERFACE_INCLUDE_DIRECTORIES ${slang_zip_SOURCE_DIR}/include
	)
elseif(APPLE)
	set_target_properties(slang PROPERTIES
			IMPORTED_LOCATION ${slang_zip_SOURCE_DIR}/lib/libslang.dylib
			INTERFACE_INCLUDE_DIRECTORIES ${slang_zip_SOURCE_DIR}/include
	)
elseif(UNIX)
	set_target_properties(slang PROPERTIES
			IMPORTED_LOCATION ${slang_zip_SOURCE_DIR}/lib/libslang.so
			INTERFACE_INCLUDE_DIRECTORIES ${slang_zip_SOURCE_DIR}/include
	)
endif()
add_library(slang::slang ALIAS slang)

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
set(ENABLE_HLSL OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_PCH OFF)
set(ENABLE_OPT OFF)
set(ENABLE_SPVREMAPPER OFF)
set(BUILD_EXTERNAL OFF)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
set(GLFW_BUILD_WAYLAND OFF)
set(VKL_GLFW_VERSION "3.4" CACHE STRING "VulkanLaunchpad glfw git tag")
set(VKL_GLM_VERSION "1.0.2" CACHE STRING "VulkanLaunchpad glm git tag")

message(DEBUG "glfw version: ${VKL_GLFW_VERSION}")
FetchContent_Declare(
	glfw
	URL "https://github.com/glfw/glfw/archive/refs/tags/${VKL_GLFW_VERSION}.zip"
	DOWNLOAD_EXTRACT_TIMESTAMP false
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glfw)

message(DEBUG "glm version: ${VKL_GLM_VERSION}")
FetchContent_Declare(
	glm
	URL "https://github.com/g-truc/glm/archive/refs/tags/${VKL_GLM_VERSION}.zip"
	DOWNLOAD_EXTRACT_TIMESTAMP false
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(glm)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(glfw update_mappings PROPERTIES FOLDER "${VKL_EXTERNAL_FOLDER}/glfw")

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
	PUBLIC Vulkan::Vulkan glm::glm glfw
	PRIVATE slang::slang
)
target_include_directories(${PROJECT_NAME} 
	INTERFACE "${VKL_SOURCE_DIR}"
	PUBLIC "${VKL_EXTERNAL_DIR}/include"
)
add_dependencies(${PROJECT_NAME} glfw slang::slang)

#==================================#
# Unset not needed cache variables #
#==================================#

unset(ENABLE_EXCEPTIONS CACHE)
unset(ENABLE_HLSL CACHE)
unset(ENABLE_OPT CACHE)
unset(ENABLE_RTTI CACHE)
unset(ENABLE_SPVREMAPPER CACHE)

unset(GLFW_USE_HYBRID_HPG CACHE)
unset(GLFW_VULKAN_STATIC CACHE)

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
