cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

if((NOT CMAKE_C_COMPILER_ID MATCHES "AppleClang") AND (NOT CMAKE_C_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "PureDarwin must be compiled with clang.")
endif()

cmake_policy(SET CMP0114 NEW)
set(CMAKE_INSTALL_PREFIX "")

include(ExternalProject)
include(scripts/cmake/crosscompile.cmake)
include(scripts/cmake/get_target_dependencies.cmake)
include(scripts/cmake/install_helpers.cmake)
include(scripts/cmake/mig.cmake)

add_subdirectory(projects)
add_subdirectory(tools)
