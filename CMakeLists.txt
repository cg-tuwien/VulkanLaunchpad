cmake_minimum_required(VERSION 3.21.0)

# Disable insource build
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
	message(FATAL_ERROR
		"CMake generation for this project is not allowed within the source directory!"
		"\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
		"\n "
		"\n rm CMakeCache.txt"
		"\n cd .."
		"\n mkdir cmake-make"
		"\n cd cmake-make"
		"\n cmake ../project"
	)
endif()

#================================#
# Vulkan Launchpad Project       #
#================================#

project(VulkanLaunchpad VERSION 0.1.0 LANGUAGES CXX)

set(VKL_EXTERNAL_FOLDER "Dependencies")

set(VKL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(VKL_EXTERNAL_DIR "${VKL_SOURCE_DIR}/external")

option(BUILD_TEMPLATE "Build VKL Template" ON)
option(USE_AVX "Activate AVX instructions" ON)
option(USE_AVX2 "Activate AVX2 instructions" ON)

#================================#
# Vulkan Launchpad Compiler      #
#================================#

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
if(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHc /MP /Qpar")
	if(USE_AVX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX")
	endif()
	if(USE_AVX2)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:AVX2")
	endif()
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE")
elseif(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wwrite-strings -x objective-c++")
	if(USE_AVX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
	endif()
	if(USE_AVX2)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
	endif()
elseif(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wwrite-strings")
	if(USE_AVX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
	endif()
	if(USE_AVX2)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
	endif()
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
	set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
else()
	message("IPO is not supported: ${output}")
endif()

#================================#
# Vulkan Launchpad Dependencies  #
#================================#

find_package(Vulkan REQUIRED)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_GLSLANG_INSTALL OFF)
set(ENABLE_PCH OFF)
add_subdirectory("${VKL_EXTERNAL_DIR}/glslang" EXCLUDE_FROM_ALL)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
add_subdirectory("${VKL_EXTERNAL_DIR}/glfw" EXCLUDE_FROM_ALL)
add_subdirectory("${VKL_EXTERNAL_DIR}/glm" EXCLUDE_FROM_ALL)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(glslang MachineIndependent GenericCodeGen OSDependent SPIRV SPVRemapper OGLCompiler PROPERTIES FOLDER "${VKL_EXTERNAL_FOLDER}/glslang")
set_target_properties(glfw update_mappings PROPERTIES FOLDER "${VKL_EXTERNAL_FOLDER}/glfw")

#================================#
# Vulkan Launchpad Target        #
#================================#

set(Vkl_Header_Files "VulkanLaunchpad.h")
set(Vkl_Source_Files "VulkanLaunchpad.cpp")
source_group("Header Files" FILES ${Vkl_Header_Files})
source_group("Source Files" FILES ${Vkl_Source_Files})

add_library(${PROJECT_NAME} STATIC ${Vkl_Header_Files} ${Vkl_Source_Files})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}
	PUBLIC Vulkan::Vulkan glm::glm glfw
	PRIVATE glslang
)
target_include_directories(${PROJECT_NAME} 
	INTERFACE "${VKL_SOURCE_DIR}"
	PUBLIC "${VKL_EXTERNAL_DIR}/include"
)
add_dependencies(${PROJECT_NAME} glfw glslang)

if(BUILD_TEMPLATE)
	set(TEMPLATE_TARGET "${PROJECT_NAME}Template")
	add_executable(${TEMPLATE_TARGET} "${VKL_SOURCE_DIR}/template/template.cpp")
	target_link_libraries(${TEMPLATE_TARGET} ${PROJECT_NAME})
endif()

#==================================#
# Unset not needed cache variables #
#==================================#

unset(ENABLE_EXCEPTIONS CACHE)
unset(ENABLE_GLSLANG_JS CACHE)
unset(ENABLE_HLSL CACHE)
unset(ENABLE_OPT CACHE)
unset(ENABLE_RTTI CACHE)
unset(ENABLE_SPVREMAPPER CACHE)
unset(SKIP_GLSLANG_INSTALL CACHE)

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

# unset(CMAKE_BUILD_TYPE CACHE)
# unset(CMAKE_CONFIGURATION_TYPES CACHE)
# unset(CMAKE_INSTALL_PREFIX CACHE)