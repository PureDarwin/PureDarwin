if(CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "PureDarwin cannot be compiled with Xcode.")
endif()

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

if((NOT CMAKE_C_COMPILER_ID MATCHES "AppleClang") AND (NOT CMAKE_C_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "PureDarwin must be compiled with clang.")
endif()
if(NOT CMAKE_HOST_APPLE)
    message(FATAL_ERROR "PureDarwin currently can be built only on macOS.")
endif()

cmake_policy(SET CMP0114 NEW)

set(CMAKE_INSTALL_PREFIX "")
set(CMAKE_MACOSX_MIN_VERSION 11.0)

include(ExternalProject)
include(cmake/asm.cmake)
include(cmake/bundle_resources.cmake)
include(cmake/crosscompile.cmake)
include(cmake/get_target_dependencies.cmake)
include(cmake/install_helpers.cmake)
include(cmake/kext.cmake)
include(cmake/mig.cmake)

add_subdirectory(projects)
add_subdirectory(src)
add_subdirectory(tools)
