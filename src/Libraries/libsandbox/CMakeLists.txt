# libsandbox: sandbox_init(3). Force-loaded into libSystem, as libsystem_sandbox
# is on Darwin; Sandbox.kext enforces the profiles it compiles.
add_darwin_static_library(libsandbox_static)

target_sources(libsandbox_static PRIVATE
    sandbox.c
    sbpl.c
)

target_include_directories(libsandbox_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(libsandbox_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libc/darwin      # libc_private.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libsystem_kernel/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_compile_definitions(libsandbox_static PRIVATE
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
)

target_compile_options(libsandbox_static PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/../libSystem/libplatform/pd_bridgeos_compat.h>
)

target_link_libraries(libsandbox_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
)
add_dependencies(libsandbox_static libc_features)
