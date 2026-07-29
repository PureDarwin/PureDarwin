set(DYLD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/upstream" CACHE PATH "Path to the dyld source tree")

if(NOT EXISTS "${DYLD_SOURCE_DIR}/src/dyld2.cpp")
    message(WARNING "dyld source not found at ${DYLD_SOURCE_DIR}; skipping dyld target")
    return()
endif()

set(DYLD_SRCS
    src/dyld2.cpp
    src/dyldAPIs.cpp
    src/dyldInitialization.cpp
    src/dyldNew.cpp
    src/dyldExceptions.c
    src/dyldLock.cpp
    src/dyld_debugger.cpp
    src/glue.c
    src/threadLocalVariables.c
    src/ImageLoader.cpp
    src/ImageLoaderMachO.cpp
    src/ImageLoaderMachOClassic.cpp
    src/ImageLoaderMachOCompressed.cpp
    src/ImageLoaderMegaDylib.cpp

    dyld3/AllImages.cpp
    dyld3/APIs.cpp
    dyld3/APIs_macOS.cpp
    dyld3/BootArgs.cpp
    dyld3/Closure.cpp
    dyld3/ClosureBuilder.cpp
    dyld3/ClosureFileSystemNull.cpp
    dyld3/ClosureFileSystemPhysical.cpp
    dyld3/ClosureWriter.cpp
    dyld3/Diagnostics.cpp
    # JSONReader.mm deliberately omitted: it needs Foundation/ObjC and is only used
    # by the closure-builder tooling, never the dyld2 runtime linker path. dyld
    # loads Foundation, so it cannot depend on it.
    dyld3/Loading.cpp
    dyld3/Logging.cpp
    dyld3/MachOAnalyzer.cpp
    dyld3/MachOAnalyzerSet.cpp
    dyld3/MachOFile.cpp
    dyld3/MachOLoaded.cpp
    dyld3/PathOverrides.cpp
    dyld3/RootsChecker.cpp
    dyld3/SharedCacheRuntime.cpp
    dyld3/Tracing.cpp
    dyld3/libdyldEntryVector.cpp
    dyld3/shared-cache/DyldSharedCache.cpp

    src/dyldStartup.s
    src/dyld_stub_binder.s
    src/threadLocalHelpers.s
    src/stub_binding_helper.s
    src/start_glue.s
)
list(TRANSFORM DYLD_SRCS PREPEND "${DYLD_SOURCE_DIR}/")

add_darwin_executable(dyld MACOSX_VERSION_MIN 11.0)
target_sources(dyld PRIVATE ${DYLD_SRCS}
    ${CMAKE_CURRENT_SOURCE_DIR}/pd_stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pd_libc_common_storage.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pd_fileutils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pd_dyld_internal_stubs.cpp)

get_target_property(_dyld_copts dyld COMPILE_OPTIONS)
list(REMOVE_ITEM _dyld_copts -nostdinc)
set_target_properties(dyld PROPERTIES COMPILE_OPTIONS "${_dyld_copts}")

set_source_files_properties(
    "${DYLD_SOURCE_DIR}/src/dyldStartup.s"
    "${DYLD_SOURCE_DIR}/src/dyld_stub_binder.s"
    "${DYLD_SOURCE_DIR}/src/threadLocalHelpers.s"
    "${DYLD_SOURCE_DIR}/src/stub_binding_helper.s"
    "${DYLD_SOURCE_DIR}/src/start_glue.s"
    PROPERTIES LANGUAGE ASM COMPILE_FLAGS "-x assembler-with-cpp"
)

target_include_directories(dyld PRIVATE
    ${DYLD_SOURCE_DIR}
    ${DYLD_SOURCE_DIR}/include
    ${DYLD_SOURCE_DIR}/src
    ${DYLD_SOURCE_DIR}/dyld3
    ${DYLD_SOURCE_DIR}/dyld3/shared-cache
    ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/corecrypto/include
    ${CMAKE_SOURCE_DIR}/src/Libraries/CrashReporterClient/include
    ${CMAKE_CURRENT_SOURCE_DIR}/compat-include
    # Prefer libsystem_kernel's generated userspace MIG headers over libc's
    # copied XNU osfmk/mach headers. compat-include/mach/mach.h redirects only
    # the umbrella; do not add libsystem_kernel/mach as an include root because
    # it also contains a private string.h used only by MIG.
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/public_include
)

# _DARWIN_C_SOURCE exposes BSD extensions (strnlen etc.) that XNU's kcdata.h,
# leaked into dyld2.cpp's include chain, relies on.
# BUILDING_LIBDYLD too: the dyld3 runtime files (AllImages/APIs/...) are the
# "libdyld" launch-closure code compiled into dyld, and gate types like
# GrowableArray on BUILDING_LIBDYLD.
# CORECRYPTO_DONOT_USE_TRANSPARENT_UNION: transparent_union is a C-only attribute
# (ignored in C++), so corecrypto's ccdigest_ctx_t union won't accept a plain
# ccdigest_ctx pointer in dyld's C++ TUs; force the plain-pointer typedef.
# _FORTIFY_SOURCE=0: avoids the __*_chk fortified libc variants (___memcpy_chk,
# ___memset_chk) which would otherwise need a libc we don't link.
target_compile_definitions(dyld PRIVATE NDEBUG BUILDING_DYLD=1 BUILDING_LIBDYLD=1
    PRIVATE=1 _DARWIN_C_SOURCE CORECRYPTO_DONOT_USE_TRANSPARENT_UNION _FORTIFY_SOURCE=0)
if(PUREDARWIN_USE_LD64_LLD)
    target_compile_definitions(dyld PRIVATE PD_LD64LLD_WEAK_DYLD_FALLBACKS)
endif()

# Ensure the MIG-generated mach public headers exist before dyld compiles.
add_dependencies(dyld libsystem_kernel_static)

# dyld uses C++ exceptions (throw/try) for its error path. It must be built with
# -mno-red-zone like the rest of the low-level Darwin code, and needs the
# bridgeos/TARGET_OS_BRIDGE availability shim force-included (see the header).
target_compile_options(dyld PRIVATE
    -mno-red-zone -fno-stack-protector
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17;-fno-rtti>
    # PD normally builds -nostdinc with explicit -I header dirs, so it sets no
    # sysroot. dyld compiles against the SDK's coherent libc/libc++ instead (its
    # wrapper headers #include_next into the C library), so it needs -isysroot.
    # PD's own headers still take priority via -I.
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    # The C++ compat force-include must NOT reach the .s files (it would inject
    # C++ into assembly); scope it to C/C++ translation units only.
    $<$<COMPILE_LANGUAGE:C,CXX>:-include${CMAKE_CURRENT_SOURCE_DIR}/dyld_sdk_compat.h>
)

target_link_libraries(dyld PRIVATE
    AvailabilityHeaders
    dyld_availability_headers   # generated dyld/VersionMap.h + for_dyld_priv.inc
    clang_bootstrap_headers
    architecture_headers
    libc_public_headers
    libc_private_headers
    libc_kernel_headers
    pthread_common_headers
    pthread_common_private_headers
    libplatform_headers
    libplatform_private_headers
)

add_dependencies(dyld libsystem_kernel_static corecrypto_static CrashReporterClient libplatform_static libc_static libc_internal_static pthread_static)
target_link_libraries(dyld PRIVATE
    $<TARGET_FILE:libsystem_kernel_static>   # Mach RPC + higher-level wrappers
    # libplatform: string/mem primitives (memcpy/memset/strlen/...) + OSAtomic.
    $<TARGET_FILE:libplatform_static>
    # pthread (VARIANT_DYLD): mutex/cond/rwlock/tsd/self for dyld's own locking
    # and libc++abi's exception paths.
    $<TARGET_FILE:pthread_static>
    # libc: leaf funcs (strcat/strdup/strrchr/qsort/realpath/*_chk/subsystem).
    $<TARGET_FILE:libc_static>
    # libc internal-header half: dir I/O (opendir/readdir/closedir) built
    # -nostdinc against Libc's internal DIR. (stdio/locale/getcwd to follow.)
    $<TARGET_FILE:libc_internal_static>
    # syscalls.a: raw BSD syscall traps (_write/_socket/_syscall...). Its CMake
    # IMPORTED target (libsyscall_traps) is directory-scoped and not visible here,
    # so reference the archive by its known build path.
    ${CMAKE_BINARY_DIR}/src/Libraries/libSystem/libsystem_kernel/syscalls.a
    $<TARGET_FILE:corecrypto_static>         # _ccdigest_*/_ccsha*
    $<TARGET_FILE:CrashReporterClient>       # _gCRAnnotations
)

# libc++abi (Itanium C++ ABI runtime): __cxa_*, operator new/delete, personality,
# std::*_error + typeinfo/vtables. Built from the vendored in-tree libc++abi (see
# src/Libraries/libcxxabi). Link after the libc/platform archives so its
# malloc/pthread/abort references resolve.
if(TARGET libcxxabi_static)
    add_dependencies(dyld libcxxabi_static_build)
    target_link_libraries(dyld PRIVATE $<TARGET_FILE:libcxxabi_static>)
endif()
# libunwind: the _Unwind_* EH machinery libc++abi's personality routine drives.
if(TARGET libunwind_static)
    add_dependencies(dyld libunwind_static_build)
    target_link_libraries(dyld PRIVATE $<TARGET_FILE:libunwind_static>)
endif()

target_link_options(dyld PRIVATE
    -e __dyld_start
    -Wl,-dylinker
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-fixup_chains
    -Wl,-data_const
    # dyld is normally dead-stripped. It also lets the linker drop the dead code
    # inside the monolithic pthread.o (workqueue/bsdthread_init/qos, pulled in
    # only because pthread_self et al share the TU) along with its undefined refs
    # to thread_start/start_wqthread/_pthread_qos_*/_pthread_set_main_qos - none
    # of which dyld's used call graph reaches.
    -Wl,-dead_strip
)
if(PUREDARWIN_USE_LD64_LLD)
    # ld64.lld diagnoses duplicate archive members before ordinary dead_strip
    # can discard them. Apple's ld64 accepts this dyld link as-is.
    target_link_options(dyld PRIVATE -Wl,--dead-strip-duplicates)
endif()

install(TARGETS dyld RUNTIME DESTINATION usr/lib COMPONENT BaseSystem)
