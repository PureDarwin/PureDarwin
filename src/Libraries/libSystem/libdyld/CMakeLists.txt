set(DYLD_SRC_DIR ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/upstream)

add_darwin_shared_library(libdyld
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /usr/lib/system/libdyld.dylib)
set_property(TARGET libdyld PROPERTY OUTPUT_NAME "dyld")

target_sources(libdyld PRIVATE
    ${DYLD_SRC_DIR}/src/dyldAPIsInLibSystem.cpp
    ${DYLD_SRC_DIR}/src/dyldLock.cpp
    ${DYLD_SRC_DIR}/dyld3/AllImages.cpp
    ${DYLD_SRC_DIR}/dyld3/APIs.cpp
    ${DYLD_SRC_DIR}/dyld3/APIs_macOS.cpp
    ${DYLD_SRC_DIR}/dyld3/Closure.cpp
    ${DYLD_SRC_DIR}/dyld3/ClosureBuilder.cpp
    ${DYLD_SRC_DIR}/dyld3/ClosureFileSystemNull.cpp
    ${DYLD_SRC_DIR}/dyld3/ClosureFileSystemPhysical.cpp
    ${DYLD_SRC_DIR}/dyld3/ClosureWriter.cpp
    ${DYLD_SRC_DIR}/dyld3/Diagnostics.cpp
    ${DYLD_SRC_DIR}/dyld3/Loading.cpp
    ${DYLD_SRC_DIR}/dyld3/Logging.cpp
    ${DYLD_SRC_DIR}/dyld3/MachOAnalyzer.cpp
    ${DYLD_SRC_DIR}/dyld3/MachOAnalyzerSet.cpp
    ${DYLD_SRC_DIR}/dyld3/MachOFile.cpp
    ${DYLD_SRC_DIR}/dyld3/MachOLoaded.cpp
    ${DYLD_SRC_DIR}/dyld3/PathOverrides.cpp
    ${DYLD_SRC_DIR}/dyld3/RootsChecker.cpp
    ${DYLD_SRC_DIR}/dyld3/Tracing.cpp
    ${DYLD_SRC_DIR}/dyld3/libdyldEntryVector.cpp
    ${DYLD_SRC_DIR}/dyld3/shared-cache/DyldSharedCache.cpp
    ${DYLD_SRC_DIR}/src/dyldLibSystemGlue.c   # __DATA,__dyld section + _dyld_func_lookup
    ${DYLD_SRC_DIR}/src/start_glue.s          # _start (LC_MAIN startGlueToCallExit)
    ${DYLD_SRC_DIR}/src/dyld_stub_binder.s    # dyld_stub_binder for classic lazy binds
    ${DYLD_SRC_DIR}/src/threadLocalHelpers.s  # _tlv_get_addr (real asm TLV-descriptor thunk, was just unwired)
    # _tlv_atexit/_tlv_exit: real Darwin provides thread_local (C++11) teardown
    # from libdyld.dylib, libc's atexit.c (statically bundled below) calls it
    # by that exact contract (see its own comment: "existing _tlv_atexit() in
    # libdyld"). The main `dyld` executable target also builds this file for
    # its OWN internal bootstrap use; this is a second, independent compilation
    # for the separate libdyld.dylib image, not a duplicate/conflicting symbol.
    ${DYLD_SRC_DIR}/src/threadLocalVariables.c
    pd_mach_o_arch.c
    pd_libdyld_exports.c
    pd_libdyld_monitor.cpp
    # Block_copy/Block_release: atexit_b()'s block-based atexit variant (same
    # translation unit as atexit()) needs the ObjC-block runtime support funcs.
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime/data.c
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch/src/BlocksRuntime/runtime.c)

set_source_files_properties(${DYLD_SRC_DIR}/src/start_glue.s PROPERTIES
    LANGUAGE ASM COMPILE_FLAGS "-x assembler-with-cpp")
set_source_files_properties(${DYLD_SRC_DIR}/src/dyld_stub_binder.s PROPERTIES
    LANGUAGE ASM COMPILE_FLAGS "-x assembler-with-cpp")

set(_pd_cpu_caps_redirect ${CMAKE_CURRENT_BINARY_DIR}/compat-include/System/machine/cpu_capabilities.h)
set(_pd_cpu_caps_real ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/i386/cpu_capabilities.h)
file(WRITE ${_pd_cpu_caps_redirect} "#include \"${_pd_cpu_caps_real}\"\n")

target_include_directories(libdyld PRIVATE
    ${DYLD_SRC_DIR}
    ${DYLD_SRC_DIR}/include
    ${DYLD_SRC_DIR}/src
    ${DYLD_SRC_DIR}/dyld3
    ${DYLD_SRC_DIR}/dyld3/shared-cache
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/corecrypto/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/dyld/compat-include
    ${CMAKE_SOURCE_DIR}/src/Libraries/CrashReporterClient/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libplatform/private
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/libxpc/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/darwin
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/bsd
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
    ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/libkern
    ${CMAKE_CURRENT_BINARY_DIR}/compat-include)

set_source_files_properties(${DYLD_SRC_DIR}/src/dyldAPIsInLibSystem.cpp PROPERTIES
    COMPILE_OPTIONS "-include${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libdispatch/dispatch/dispatch.h")

target_compile_definitions(libdyld PRIVATE
    NDEBUG
    BUILDING_DYLD=1
    BUILDING_LIBDYLD=1
    PRIVATE=1
    __DARWIN_ONLY_UNIX_CONFORMANCE=1
    __DARWIN_ONLY_64_BIT_INO_T=1
    _DARWIN_C_SOURCE
    CORECRYPTO_DONOT_USE_TRANSPARENT_UNION
    _FORTIFY_SOURCE=0)

# Like dyld itself: compile against the SDK sysroot (these TUs pull mach/*,
# pthread.h, malloc/malloc.h) instead of PD's -nostdinc header set. Drop the
# -nostdinc that add_darwin_shared_library injects; PD's own -I dirs still win.
get_target_property(_libdyld_copts libdyld COMPILE_OPTIONS)
if(_libdyld_copts)
    list(REMOVE_ITEM _libdyld_copts -nostdinc)
    set_target_properties(libdyld PROPERTIES COMPILE_OPTIONS "${_libdyld_copts}")
endif()
target_compile_options(libdyld PRIVATE
    -mno-red-zone -fno-stack-protector
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17;-fno-rtti>
    $<$<COMPILE_LANGUAGE:C>:-isysroot ${PUREDARWIN_MACOSX_SDK}>
    $<$<COMPILE_LANGUAGE:CXX>:-isysroot ${PUREDARWIN_MACOSX_SDK}>
    # bridgeos availability shim: threadLocalVariables.c pulls mach-o/dyld.h,
    # whose bridgeos __API_UNAVAILABLE entries neither the SDK nor PD define
    # (same fix dyld's own CMakeLists applies via dyld_sdk_compat.h).
    $<$<COMPILE_LANGUAGE:C,CXX>:-include${CMAKE_SOURCE_DIR}/src/Libraries/dyld/dyld_sdk_compat.h>)

# Code archives that satisfy the helper table's references (malloc/free/pthread_*/
# mmap/vm_allocate/__cxa_*) plus the raw syscall traps. Linked by path so their
# INTERFACE include dirs don't leak onto the compile line (same rationale as dyld).
add_dependencies(libdyld libsystem_kernel_static corecrypto_static libc_static libc_internal_static
    pthread_static libplatform_static libmalloc_static libcxxabi_static_build
    CrashReporterClient dyld_availability_headers)
target_link_libraries(libdyld PRIVATE
    dyld_availability_headers
    $<TARGET_FILE:libsystem_kernel_static>
    $<TARGET_FILE:libplatform_static>
    $<TARGET_FILE:pthread_static>
    $<TARGET_FILE:libmalloc_static>    # malloc/free/malloc_size (helper table + atexit)
    $<TARGET_FILE:libc_static>
    $<TARGET_FILE:libc_internal_static>
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/syscalls.a
    $<TARGET_FILE:corecrypto_static>
    $<TARGET_FILE:CrashReporterClient>
    $<TARGET_FILE:libcxxabi_static>)   # __cxa_atexit/__cxa_finalize[_ranges]

target_link_options(libdyld PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -compatibility_version 1.0.0
    -current_version 1292.100.5
    -Wl,-dead_strip
    -Wl,-fixup_chains
    # -dead_strip drops these two otherwise - nothing inside libdyld itself
    # calls them, they're only ever called from outside (libunwind, via
    # libSystem_B_stub's own -Wl,-u/-reexport_library of this dylib), so
    # dead-code stripping sees them as unreferenced roots and removes them.
    -Wl,-u,__dyld_find_unwind_sections
    -Wl,-u,__dyld_register_func_for_remove_image
    -Wl,-u,__NSGetExecutablePath
    # clang @available() helper: nothing inside libdyld calls it (only external
    # binaries do, via libSystem's reexport), so dead_strip would drop it.
    -Wl,-u,___isPlatformVersionAtLeast)
if(CMAKE_HOST_APPLE)
    set(_libdyld_apple_ld_stub_dir ${CMAKE_CURRENT_BINARY_DIR}/apple-ld-stubs)
    set(_libdyld_apple_ld_pthread_stub ${_libdyld_apple_ld_stub_dir}/libsystem_pthread.tbd)
    add_custom_command(
        OUTPUT ${_libdyld_apple_ld_pthread_stub}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_libdyld_apple_ld_stub_dir}
        COMMAND ${CMAKE_COMMAND}
            -D OUTPUT=${_libdyld_apple_ld_pthread_stub}
            -P ${CMAKE_SOURCE_DIR}/cmake/write_libsystem_pthread_tbd.cmake
        VERBATIM)
    add_custom_target(libdyld_apple_ld_stubs DEPENDS ${_libdyld_apple_ld_pthread_stub})
    add_dependencies(libdyld libdyld_apple_ld_stubs)
    target_link_options(libdyld PRIVATE
        -Wl,-L,${_libdyld_apple_ld_stub_dir}
        -Wl,-framework,System)
else()
    target_link_options(libdyld PRIVATE -lSystem)
endif()

install(TARGETS libdyld LIBRARY DESTINATION usr/lib/system COMPONENT BaseSystem)
