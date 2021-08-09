cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

if((NOT CMAKE_C_COMPILER_ID MATCHES "AppleClang") AND (NOT CMAKE_C_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "PureDarwin must be compiled with clang.")
endif()

if(NOT KERNEL_BUILD_FREEBSD AND NOT KERNEL_BUILD_XNU)
    message(FATAL_ERROR "You must specify one of KERNEL_BUILD_FREEBSD or KERNEL_BUILD_XNU")
elseif(KERNEL_BUILD_FREEBSD AND KERNEL_BUILD_XNU)
    message(FATAL_ERROR "You cannot specify both KERNEL_BUILD_FREEBSD and KERNEL_BUILD_XNU")
endif()

if(KERNEL_BUILD_FREEBSD)
    include(scripts/cmake/crosscompile_fbsd.cmake)
else()
    if(NOT CMAKE_HOST_APPLE)
        message(FATAL_ERROR "Building PureDarwin with the xnu kernel currently requires a macOS host.")
    endif()
endif()

cmake_policy(SET CMP0114 NEW)

set(CMAKE_INSTALL_PREFIX "")
set(CMAKE_MACOSX_MIN_VERSION 11.0)

include(ExternalProject)
include(scripts/cmake/asm.cmake)
include(scripts/cmake/bundle_resources.cmake)
include(scripts/cmake/crosscompile_darwin.cmake)
include(scripts/cmake/get_target_dependencies.cmake)
include(scripts/cmake/install_helpers.cmake)
include(scripts/cmake/kext.cmake)
include(scripts/cmake/mig.cmake)

add_subdirectory(projects)
add_subdirectory(tools)
