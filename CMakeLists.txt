cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

if((NOT CMAKE_C_COMPILER_ID MATCHES "AppleClang") AND (NOT CMAKE_C_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "PureDarwin must be compiled with clang.")
endif()

include(ExternalProject)

add_subdirectory(tools)
