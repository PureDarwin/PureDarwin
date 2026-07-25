if(CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "PureDarwin cannot be compiled with Xcode.")
endif()

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)
# libdispatch's src/object.m (OS_object/USE_OBJC glue) needs the ObjC
# frontend available project-wide.
enable_language(OBJC)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated-headers/objc)
file(GLOB _pd_objc4_headers ${CMAKE_SOURCE_DIR}/src/Libraries/objc4/runtime/*.h)
foreach(_pd_h ${_pd_objc4_headers})
  get_filename_component(_pd_name ${_pd_h} NAME)
  if(NOT EXISTS ${CMAKE_BINARY_DIR}/generated-headers/objc/${_pd_name})
    file(CREATE_LINK ${_pd_h} ${CMAKE_BINARY_DIR}/generated-headers/objc/${_pd_name} SYMBOLIC)
  endif()
endforeach()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated-headers/Foundation)
file(GLOB_RECURSE _pd_foundation_headers ${CMAKE_SOURCE_DIR}/src/Libraries/Foundation/*.h)
foreach(_pd_h ${_pd_foundation_headers})
  get_filename_component(_pd_name ${_pd_h} NAME)
  if(NOT EXISTS ${CMAKE_BINARY_DIR}/generated-headers/Foundation/${_pd_name})
    file(CREATE_LINK ${_pd_h} ${CMAKE_BINARY_DIR}/generated-headers/Foundation/${_pd_name} SYMBOLIC)
  endif()
endforeach()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated-headers/mach-o)
file(GLOB _pd_macho_headers ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/EXTERNAL_HEADERS/mach-o/*.h)
foreach(_pd_h ${_pd_macho_headers})
  get_filename_component(_pd_name ${_pd_h} NAME)
  if(NOT EXISTS ${CMAKE_BINARY_DIR}/generated-headers/mach-o/${_pd_name})
    file(CREATE_LINK ${_pd_h} ${CMAKE_BINARY_DIR}/generated-headers/mach-o/${_pd_name} SYMBOLIC)
  endif()
endforeach()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated-headers/dlfcn)
if(EXISTS ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/upstream/include/dlfcn.h AND
   NOT EXISTS ${CMAKE_BINARY_DIR}/generated-headers/dlfcn/dlfcn.h)
  file(CREATE_LINK ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/upstream/include/dlfcn.h
       ${CMAKE_BINARY_DIR}/generated-headers/dlfcn/dlfcn.h SYMBOLIC)
endif()

# Foundation's NSObjCRuntime.h wants <CoreFoundation/CFAvailability.h> (and
# transitively other flat "<CoreFoundation/Foo.h>" headers)
# Mirror that here for header-only consumers like this.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated-headers/CoreFoundation)
file(GLOB_RECURSE _pd_cf_headers ${CMAKE_SOURCE_DIR}/src/Libraries/CoreFoundation/*.h)
foreach(_pd_h ${_pd_cf_headers})
  get_filename_component(_pd_name ${_pd_h} NAME)
  if(NOT EXISTS ${CMAKE_BINARY_DIR}/generated-headers/CoreFoundation/${_pd_name})
    file(CREATE_LINK ${_pd_h} ${CMAKE_BINARY_DIR}/generated-headers/CoreFoundation/${_pd_name} SYMBOLIC)
  endif()
endforeach()

if((NOT CMAKE_C_COMPILER_ID MATCHES "AppleClang") AND (NOT CMAKE_C_COMPILER_ID MATCHES "Clang"))
    message(FATAL_ERROR "PureDarwin must be compiled with clang.")
endif()

cmake_policy(SET CMP0114 NEW)

set(CMAKE_MACOSX_MIN_VERSION 11.0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13)
        message(FATAL_ERROR
            "PureDarwin's vendored libc++ runtime expects AppleClang 13 or newer."
        )
    endif()
endif()

# Target CPU architecture. x86_64 is the primary/working target (QEMU boot,
# loader, dyld). arm64 is an opt-in in progress: configure with
# -DPUREDARWIN_ARCH=arm64. Falls back to CMAKE_OSX_ARCHITECTURES if that's set.
if(NOT PUREDARWIN_ARCH)
    if(CMAKE_OSX_ARCHITECTURES)
        set(PUREDARWIN_ARCH "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(PUREDARWIN_ARCH "x86_64")
    endif()
endif()
set(PUREDARWIN_ARCH "${PUREDARWIN_ARCH}" CACHE STRING "Target architecture (x86_64 or arm64)" FORCE)

# Normalize and derive the arch predicate the tree branches on.
if(PUREDARWIN_ARCH MATCHES "^(arm64|aarch64)$")
    set(PUREDARWIN_ARCH "arm64")
    set(PUREDARWIN_ARM64 TRUE)
elseif(PUREDARWIN_ARCH MATCHES "^(x86_64|amd64)$")
    set(PUREDARWIN_ARCH "x86_64")
    set(PUREDARWIN_ARM64 FALSE)
else()
    message(FATAL_ERROR "PUREDARWIN_ARCH must be x86_64 or arm64, got '${PUREDARWIN_ARCH}'")
endif()
message(STATUS "PureDarwin target architecture: ${PUREDARWIN_ARCH}")

execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-path
    OUTPUT_VARIABLE _puredarwin_xcrun_macosx_sdk
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE _puredarwin_xcrun_macosx_sdk_result)
if(_puredarwin_xcrun_macosx_sdk_result EQUAL 0 AND EXISTS "${_puredarwin_xcrun_macosx_sdk}/usr/include")
    set(_puredarwin_default_macosx_sdk "${_puredarwin_xcrun_macosx_sdk}")
else()
    set(_puredarwin_default_macosx_sdk "/usr/local/osxcross/SDK/MacOSX11.3.sdk")
endif()
set(PUREDARWIN_MACOSX_SDK "${_puredarwin_default_macosx_sdk}" CACHE PATH "macOS SDK root for userspace bootstrap components")

option(PUREDARWIN_ENABLE_PROJECTS "Configure PureDarwin project definitions" ON)
option(PUREDARWIN_ENABLE_KERNEL "Configure PureDarwin kernel targets" ON)
option(PUREDARWIN_ENABLE_LIBRARIES "Configure PureDarwin library targets" ON)
option(PUREDARWIN_ENABLE_USERSPACE "Configure PureDarwin userspace targets" ON)
option(PUREDARWIN_ENABLE_TOOLS "Configure PureDarwin tool targets" ON)
# Real x86_64-apple-darwin cross-compiled cctools (ld64, nm, lipo, etc.),
# meant to eventually run ON PureDarwin itself (self-hosting), as opposed to
# the existing host_* cctools targets which are Linux-native tools used only
# to drive our own cross-builds. Off by default: early work-in-progress.
option(PUREDARWIN_ENABLE_SELFHOST_CCTOOLS "Configure self-hosted (Darwin-target) cctools" OFF)

include(ExternalProject)
include(cmake/asm.cmake)
include(cmake/bundle_resources.cmake)
include(cmake/circular.cmake)
include(cmake/crosscompile.cmake)
include(cmake/get_target_dependencies.cmake)
include(cmake/iig.cmake)
include(cmake/install_helpers.cmake)
include(cmake/kext.cmake)
include(cmake/mig.cmake)
include(cmake/suppress_warnings.cmake)

if(PUREDARWIN_ENABLE_PROJECTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/projects/CMakeLists.txt")
        add_subdirectory(projects)
    endif()
endif()
add_subdirectory(src)
if(PUREDARWIN_ENABLE_TOOLS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/CMakeLists.txt")
        add_subdirectory(tools)
    endif()
endif()
