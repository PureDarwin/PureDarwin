# libnv: FreeBSD's nvlist/nvpair wire-format library, vendored verbatim from
# github.com/PureDarwin/XPC (src/libnv), which itself vendored it from FreeBSD.
# Real XPC's own serialization builds on this. Fully self-contained for
# userland: the kernel-only headers (sys/lock.h, sys/malloc.h, sys/systm.h,
# machine/stdarg.h) are all behind #ifdef _KERNEL, which we don't define, so
# this takes the plain userland path (errno.h, stdarg.h, etc) unconditionally.
# PJDLOG_ASSERT/PJDLOG_ABORT similarly already fall back to plain
# assert()/abort() when HAVE_PJDLOG isn't defined (also not defined here) -
# no pjdlog dependency needed.
#
# libsbuf.c isn't referenced by nvlist.c/nvpair.c at all (only by libxpc's
# xpc_misc.c later); included here anyway since it's genuinely self-contained
# (struct sbuf is defined inline in the same file, no sys/sbuf.h needed).
add_darwin_static_library(XPC_libnv_static)
target_sources(XPC_libnv_static PRIVATE
    nvlist.c
    nvpair.c
    libsbuf.c
)

target_compile_definitions(XPC_libnv_static PRIVATE __DARWIN_ONLY_UNIX_CONFORMANCE=1)

target_include_directories(XPC_libnv_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libc/darwin
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/pthread/include/pthread
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/private_include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libSystem/libsystem_kernel/include
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libc/include/${PUREDARWIN_ARCH}
)

target_link_libraries(XPC_libnv_static PRIVATE
    AvailabilityHeaders
    clang_bootstrap_headers
    libc_public_headers
    libc_features
    pthread_common_headers
    xnu_headers
    xnu_private_headers
)
add_dependencies(XPC_libnv_static libc_features)
