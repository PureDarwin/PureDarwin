cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

cmake_policy(SET CMP0114 NEW)

include(ExternalProject)

# CMake helper scripts
include(${RC_DARWIN_ROOT}/cmake/asm.cmake)
include(${RC_DARWIN_ROOT}/cmake/bundle_resources.cmake)
include(${RC_DARWIN_ROOT}/cmake/crosscompile.cmake)
include(${RC_DARWIN_ROOT}/cmake/get_target_dependencies.cmake)
include(${RC_DARWIN_ROOT}/cmake/install_helpers.cmake)
include(${RC_DARWIN_ROOT}/cmake/kext.cmake)
include(${RC_DARWIN_ROOT}/cmake/mig.cmake)

# Source subdirectories
add_subdirectory(Kernel)
add_subdirectory(Libraries)
