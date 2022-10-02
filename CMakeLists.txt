cmake_minimum_required(VERSION 3.22)

# References to chapters throughout the file refer to Craig Scott's
# "Professional CMake: A Practical Guide", published in 2018, unless
# specified otherwise.

# --log-context is needed in the cmake commandline options to make use
# of this little helper in the log output (optional)
list(APPEND CMAKE_MESSAGE_CONTEXT VulkanLaunchpad)

project(VulkanLaunchpad VERSION 0.1.0 LANGUAGES CXX)

# Vulkan
find_package(Vulkan REQUIRED)

################################################################################
# Configuration Options for Users
################################################################################

# Directory to output any static or dynamic libraries to
set(Vkl_Library_Output_Directory "./cmake-build/" CACHE STRING "Directory to output built libraries to, relative to CMAKE_CURRENT_SOURCE_DIR, defaults to './cmake-build/'")

# Build a static lib by default
set(BUILD_STATIC_LIBS ON CACHE STRING "Should CMake create a compile_commands.json file?")

################################################################################
# Source groups
################################################################################

set(Vkl_Header_Files
  "VulkanLaunchpad.h"
)
source_group("Header Files" FILES ${Vkl_Header_Files})

set(Vkl_Source_Files
  "VulkanLaunchpad.cpp"
)
source_group("Source Files" FILES ${Vkl_Source_Files})

set(Vkl_All_Files ${Vkl_Header_Files} ${Vkl_Source_Files})

################################################################################
# Target
################################################################################

# I don't know if we want to support building static/dynamic libraries
#or if this library should stay include only.

add_library(${PROJECT_NAME} INTERFACE)

if(BUILD_STATIC_LIBS)
  add_library(${PROJECT_NAME}_static STATIC ${Vkl_All_Files})
endif()

if(BUILD_SHARED_LIBS)
  add_library(${PROJECT_NAME}_shared SHARED ${Vkl_All_Files})
endif()

# "20.1. Project Version" and "21.3. Shared Library Versioning"
set_target_properties(${PROJECT_NAME} PROPERTIES
  VERSION ${VulkanLaunchpad_VERSION} # VulkanLaunchpad_VERSION is automatically
                             # set by project(VulkanLaunchpad VERSION ...)
  SOVERSION ${VulkanLaunchpad_VERSION_MAJOR} # VulkanLaunchpad is
                                     # automatically set by
                                     # project(VulkanLaunchpad VERSION
                                     # ...)
)

# Hide symbols by default as explained in chapter "21.5.1. Specifying
# Default Visibility"
set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN TRUE
)

# The RootNamespace of a VS Project is used to determine naming for
# managed resource DLLs.
set(VS_GLOBAL_ROOTNAMESPACE ${PROJECT_NAME})

# This sets the "keyword" attribute of the Visual Studio project.
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)

################################################################################
# Target name and Output directory
################################################################################
# ARCHIVE_OUTPUT_DIRECTORY is used for static libraries on all platforms and import libraries associated with DLLs on Windows.

if(BUILD_STATIC_LIBS)
  set_target_properties(${PROJECT_NAME}_static PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}_static_$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${Vkl_Library_Output_Directory}"
  )
endif()

if(BUILD_SHARED_LIBS)
  set_target_properties(${PROJECT_NAME}_shared PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}_shared_$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${Vkl_Library_Output_Directory}"
  )
endif()

################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
    $<$<CONFIG:Debug>:
        MultiThreadedDebug # Equal to the /MTd VS compiler flag
    >
    $<$<OR:$<CONFIG:Release>,$<CONFIG:ReleaseExe>>:
        MultiThreaded # Equal to the /MT VS compiler flag
    >
    $<$<NOT:$<OR:$<CONFIG:Debug>,$<CONFIG:Release>,$<CONFIG:ReleaseExe>>>:${MSVC_RUNTIME_LIBRARY_DEFAULT}>
)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

if(BUILD_STATIC_LIBS)
  set_target_properties(${PROJECT_NAME}_static PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
endif()

if(BUILD_SHARED_LIBS)
  set_target_properties(${PROJECT_NAME}_shared PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})
endif()


################################################################################
# Include directories
################################################################################

set(Vkl_Include_Dirs
  "./external/include/"
  "${Vulkan_INCLUDE_DIR}"
)

target_include_directories(${PROJECT_NAME} INTERFACE ${Vkl_Include_Dirs})

if(BUILD_STATIC_LIBS)
  target_include_directories(${PROJECT_NAME}_static PUBLIC ${Vkl_Include_Dirs})
endif()

if(BUILD_SHARED_LIBS)
  target_include_directories(${PROJECT_NAME}_shared PUBLIC ${Vkl_Include_Dirs})
endif()

################################################################################
# Dependencies
################################################################################

set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "$<$<CONFIG:Debug>:"
        "glslangd"
    ">"
    "$<$<CONFIG:Release>:"
        "glslang"
    ">"
    "Vulkan::Vulkan"
)

target_link_libraries(${PROJECT_NAME} INTERFACE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

set(Vkl_Link_Dirs
  "./external/lib/"
  "$ENV{VULKAN_SDK}/Lib"
)

target_link_directories(${PROJECT_NAME} INTERFACE ${Vkl_Link_Dirs})

if(BUILD_STATIC_LIBS)
  target_link_directories(${PROJECT_NAME}_static PRIVATE ${Vkl_Link_Dirs})
endif()

if(BUILD_SHARED_LIBS)
  target_link_directories(${PROJECT_NAME}_shared PRIVATE ${Vkl_Link_Dirs})
endif()

################################################################################
# Compatibilty settings
################################################################################

# Some editors use `compile_commands.json` to parse includes, so we
# want to always generate it for maximum editor support.
if(NOT CMAKE_EXPORT_COMPILE_COMMANDS)
  option(CMAKE_EXPORT_COMPILE_COMMANDS "Should CMake create a compile_commands.json file?" ON)
#  set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE STRING "Should CMake create a compile_commands.json file?")
endif()

message(STATUS "Value of CMAKE_EXPORT_COMPILE_COMMANDS=${CMAKE_EXPORT_COMPILE_COMMANDS}")