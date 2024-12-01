# Everything in this directory (including the CMakeLists) is licensed under
# SPDX-License-Identifier: GPL-3

add_compile_options(-Wno-return-type -Wno-error=cpp -Wno-nullability-completeness)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif()

include(corecrypto_shared.cmake)
include(host_corecrypto_static.cmake)

add_darwin_object_library(corecrypto_user)
target_sources(corecrypto_user PRIVATE cc_user_stub.c)
target_link_libraries(corecrypto_user PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    pthread_common_headers
    xnu_headers
)

add_darwin_static_library(corecrypto_static)
target_sources(corecrypto_static PRIVATE
    $<TARGET_OBJECTS:corecrypto_shared>
    $<TARGET_OBJECTS:corecrypto_user>
)

#[[
add_darwin_circular_library(corecrypto
    INSTALL_NAME "/usr/lib/system/libcorecrypto.dylib"
    OBJECT_LIBRARIES corecrypto_shared corecrypto_user
    SIBLINGS
        libsystem_dyld
        libcompiler_rt
        libplatform
        libsystem_malloc
        libsystem_c
        libsystem_blocks
        libsystem_pthread
        macho
        libdispatch_shared
    STRONG_DEPENDENCIES
        libsystem_kernel
)
]]
