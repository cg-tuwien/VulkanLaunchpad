cmake_minimum_required(VERSION 3.14)

#================================#
# Vulkan Launchpad Project       #
#================================#

project(VulkanLaunchpad VERSION 0.1.0 LANGUAGES CXX)

# check if this is root
get_directory_property(HAS_PARENT PARENT_DIRECTORY)

if(NOT HAS_PARENT)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    # Update submodules as needed
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if(GIT_SUBMODULE)
            execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                            RESULT_VARIABLE GIT_SUBMOD_RESULT)
            if(NOT GIT_SUBMOD_RESULT EQUAL "0")
                message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
            endif()
        endif()
    endif()
endif()

set(VKL_EXTERNAL_FOLDER "Dependencies")
set(VKL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(VKL_EXTERNAL_DIR "${VKL_SOURCE_DIR}/external")

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
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_GLSLANG_INSTALL OFF)
set(ENABLE_PCH OFF)
set(ENABLE_OPT OFF)
set(ENABLE_SPVREMAPPER OFF)
set(BUILD_EXTERNAL OFF)
add_subdirectory("${VKL_EXTERNAL_DIR}/glslang" EXCLUDE_FROM_ALL)
set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)
add_subdirectory("${VKL_EXTERNAL_DIR}/glfw" EXCLUDE_FROM_ALL)
add_subdirectory("${VKL_EXTERNAL_DIR}/glm" EXCLUDE_FROM_ALL)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_target_properties(glslang MachineIndependent GenericCodeGen OSDependent SPIRV OGLCompiler PROPERTIES FOLDER "${VKL_EXTERNAL_FOLDER}/glslang")
set_target_properties(glfw update_mappings PROPERTIES FOLDER "${VKL_EXTERNAL_FOLDER}/glfw")

#================================#
# Vulkan Launchpad Target        #
#================================#

set(VKL_HEADER_FILES "VulkanLaunchpad.h")
set(VKL_SOURCE_FILES "VulkanLaunchpad.cpp")
source_group("Header Files" FILES ${VKL_HEADER_FILES})
source_group("Source Files" FILES ${VKL_SOURCE_FILES})

add_library(${PROJECT_NAME} STATIC ${VKL_HEADER_FILES} ${VKL_SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME}
	PUBLIC Vulkan::Vulkan glm::glm glfw
	PRIVATE glslang MachineIndependent GenericCodeGen OSDependent SPIRV
)
target_include_directories(${PROJECT_NAME} 
	INTERFACE "${VKL_SOURCE_DIR}"
	PUBLIC "${VKL_EXTERNAL_DIR}/include"
)
add_dependencies(${PROJECT_NAME} glfw glslang MachineIndependent GenericCodeGen OSDependent SPIRV)

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
