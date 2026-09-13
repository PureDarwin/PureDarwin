get_filename_component(_ld_toolchain_bin "${CMAKE_AR}" DIRECTORY)
if(CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING)
    # xcrun cannot see an SDK inside a nix sandbox and prints its error on
    # stdout, so an unchecked capture silently becomes the sysroot. Guard it
    # the same way the top-level SDK detection does.
    execute_process(
        COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _ld_sdk
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _ld_sdk_result
    )
    if(NOT _ld_sdk_result EQUAL 0 OR NOT EXISTS "${_ld_sdk}/usr/include")
        set(_ld_sdk "${PUREDARWIN_MACOSX_SDK}")
    endif()
    find_program(_ld_real_clang NAMES clang REQUIRED)
    set(_ld_target_flags -arch "${PUREDARWIN_ARCH}")
else()
    get_filename_component(_ld_ar_name "${CMAKE_AR}" NAME)
    if(NOT _ld_ar_name MATCHES "^(.+)-ar$")
        message(FATAL_ERROR "Cannot derive the osxcross target triple from CMAKE_AR='${CMAKE_AR}'.")
    endif()
    set(_ld_triple "${CMAKE_MATCH_1}")
    if(DEFINED ENV{OSXCROSS_SDK})
        set(_ld_sdk "$ENV{OSXCROSS_SDK}")
    endif()
    if(NOT _ld_sdk)
        set(_ld_sdk "${PUREDARWIN_MACOSX_SDK}")
    endif()
    set(_ld_real_clang "${_ld_toolchain_bin}/${_ld_triple}-clang")
    if(NOT EXISTS "${_ld_real_clang}")
        message(FATAL_ERROR "Expected the osxcross clang wrapper at '${_ld_real_clang}'.")
    endif()
    set(_ld_target_flags)
endif()

set(_ld_src "${CMAKE_CURRENT_SOURCE_DIR}/../ld64/src")
set(_ld_libcxx "${CMAKE_SOURCE_DIR}/src/Libraries/libcxx")
set(_ld_libcxx_config "${CMAKE_SOURCE_DIR}/src/Libraries/libcxxabi/config")
# ld.cpp and Options.cpp include <cxxabi.h>, which -nostdinc++ below takes the
# SDK's copy out of reach of.
set(_ld_libcxxabi_include "${CMAKE_SOURCE_DIR}/src/Libraries/libcxxabi/include")
set(_ld_corecrypto_include "${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/corecrypto/include")
set(_ld_commoncrypto_include "${CMAKE_SOURCE_DIR}/src/Libraries/CommonCrypto/include/Private")
set(_ld_commoncrypto_public_include "${CMAKE_SOURCE_DIR}/src/Libraries/CommonCrypto/include")

set(_ld_includes
    -I ${_ld_src}/../../include
    -I ${_ld_src}/../../include/foreign
    -I ${_ld_src}/ld
    -I ${_ld_src}
    -I ${_ld_src}/abstraction
    -I ${_ld_src}/3rd
    -I ${_ld_src}/3rd/include
    -I ${_ld_src}/3rd/BlocksRuntime
    -I ${_ld_src}/ld/parsers
    -I ${_ld_src}/ld/passes
    -I ${_ld_commoncrypto_include}
    -I ${_ld_commoncrypto_public_include}
    -I ${_ld_corecrypto_include}
)

set(_ld_cxx_flags
    ${_ld_target_flags}
    -isysroot ${_ld_sdk}
    -std=c++17 -nostdinc++ -fblocks -fexceptions -Os -DNDEBUG
    -D__DARWIN_UNIX03
    -Wno-deprecated "-Wno-#warnings"
    ${_ld_includes}
)
if((CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING) OR PUREDARWIN_NIX_TOOLCHAIN)
    list(APPEND _ld_cxx_flags -I ${_ld_libcxx_config} -I ${_ld_libcxx}/include
        -I ${_ld_libcxxabi_include})
else()
    list(APPEND _ld_cxx_flags -idirafter ${_ld_libcxx_config} -idirafter ${_ld_libcxx}/include
        -idirafter ${_ld_libcxxabi_include})
endif()

set(_ld_c_flags
    ${_ld_target_flags}
    -isysroot ${_ld_sdk}
    -D__DARWIN_UNIX03 -fblocks -Os -DNDEBUG
    ${_ld_includes}
)

# (relative path, is_cxx)
set(_ld_cxx_units
    ld/InputFiles.cpp
    ld/ld.cpp
    ld/Options.cpp
    ld/OutputFile.cpp
    ld/Resolver.cpp
    ld/ResponseFiles.cpp
    ld/Snapshot.cpp
    ld/SymbolTable.cpp
    ld/PlatformSupport.cpp
    ld/code-sign-blobs/blob.cpp
    ld/parsers/archive_file.cpp
    ld/parsers/generic_dylib_file.cpp
    ld/parsers/lto_file.cpp
    ld/parsers/macho_dylib_file.cpp
    ld/parsers/macho_relocatable_file.cpp
    ld/parsers/opaque_section_file.cpp
    ld/parsers/textstub_dylib_file.cpp
    ld/passes/branch_island.cpp
    ld/passes/branch_shim.cpp
    ld/passes/compact_unwind.cpp
    ld/passes/dtrace_dof.cpp
    ld/passes/dylibs.cpp
    ld/passes/got.cpp
    ld/passes/huge.cpp
    ld/passes/objc.cpp
    ld/passes/objc_constants.cpp
    ld/passes/order.cpp
    ld/passes/tlvp.cpp
    ld/passes/stubs/stubs.cpp
    ld/passes/code_dedup.cpp
    ld/passes/thread_starts.cpp
    ld/passes/inits.cpp
)

set(_ld_c_units
    ld/debugline.c
    ld/libcodedirectory.c
    3rd/BlocksRuntime/data.c
    3rd/BlocksRuntime/runtime.c
    3rd/helper.c
    3rd/qsort_r.c
    3rd/strlcat.c
    3rd/strlcpy.c
    3rd/eprintf.c
    3rd/md5.c
    3rd/find_executable.c
)

set(_ld_objs "")
foreach(_rel ${_ld_cxx_units})
    get_filename_component(_base "${_rel}" NAME_WE)
    get_filename_component(_dir "${_rel}" DIRECTORY)
    string(REPLACE "/" "_" _tag "${_dir}_${_base}")
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/${_tag}.o")
    add_custom_command(
        OUTPUT "${_obj}"
        COMMAND "${_ld_real_clang}" ${_ld_cxx_flags} -c "${_ld_src}/${_rel}" -o "${_obj}"
        DEPENDS "${_ld_src}/${_rel}"
        COMMENT "Building ld64 ${_rel} (self-host)"
        VERBATIM
    )
    list(APPEND _ld_objs "${_obj}")
endforeach()

# Real CC_MD5, bridged onto corecrypto (see the file for why it's not part
# of the vendored ld64 tree). Not reachable via a commoncrypto_static CMake
# target here since PUREDARWIN_ENABLE_LIBRARIES is off for this build.
set(_ld_ccmd5_src "${CMAKE_SOURCE_DIR}/src/Libraries/CommonCrypto/libcn/pd_cc_digest_bridge.c")
set(_ld_ccmd5_obj "${CMAKE_CURRENT_BINARY_DIR}/pd_cc_digest_bridge.o")
add_custom_command(
    OUTPUT "${_ld_ccmd5_obj}"
    COMMAND "${_ld_real_clang}" ${_ld_c_flags} -c "${_ld_ccmd5_src}" -o "${_ld_ccmd5_obj}"
    DEPENDS "${_ld_ccmd5_src}"
    COMMENT "Building CC_MD5 corecrypto bridge (self-host)"
    VERBATIM
)
list(APPEND _ld_objs "${_ld_ccmd5_obj}")

# reallocf: our libc has a real one (stdlib/FreeBSD/reallocf.c), but a
# separate weak+hidden-visibility bootstrap fallback in libsystem_kernel's
# _libc_funcptr.c wins symbol resolution and can never be dynamically
# exported (hidden visibility). Rather than touch that early-bootstrap
# indirection, just compile our own copy directly into this binary.
set(_ld_reallocf_src "${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/stdlib/FreeBSD/reallocf.c")
set(_ld_reallocf_obj "${CMAKE_CURRENT_BINARY_DIR}/pd_reallocf.o")
add_custom_command(
    OUTPUT "${_ld_reallocf_obj}"
    COMMAND "${_ld_real_clang}" ${_ld_c_flags} -c "${_ld_reallocf_src}" -o "${_ld_reallocf_obj}"
    DEPENDS "${_ld_reallocf_src}"
    COMMENT "Building reallocf (self-host)"
    VERBATIM
)
list(APPEND _ld_objs "${_ld_reallocf_obj}")

foreach(_rel ${_ld_c_units})
    get_filename_component(_base "${_rel}" NAME_WE)
    get_filename_component(_dir "${_rel}" DIRECTORY)
    string(REPLACE "/" "_" _tag "${_dir}_${_base}")
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/${_tag}.o")
    add_custom_command(
        OUTPUT "${_obj}"
        COMMAND "${_ld_real_clang}" ${_ld_c_flags} -c "${_ld_src}/${_rel}" -o "${_obj}"
        DEPENDS "${_ld_src}/${_rel}"
        COMMENT "Building ld64 ${_rel} (self-host)"
        VERBATIM
    )
    list(APPEND _ld_objs "${_obj}")
endforeach()

set(_ld_bin "${CMAKE_CURRENT_BINARY_DIR}/ld")
set(_ld_link_targets libSystem_B_stub dyld libstuff_static)
set(_ld_extra_archives $<TARGET_FILE:libstuff_static>)
if(TARGET libcxxabi_static)
    list(APPEND _ld_link_targets libcxxabi_static libcxxabi_static_build)
    list(APPEND _ld_extra_archives $<TARGET_FILE:libcxxabi_static>)
endif()
if(TARGET libunwind_static)
    list(APPEND _ld_link_targets libunwind_static libunwind_static_build)
    list(APPEND _ld_extra_archives $<TARGET_FILE:libunwind_static>)
endif()

add_custom_command(
    OUTPUT "${_ld_bin}"
    COMMAND "${_ld_real_clang}" ${_ld_target_flags}
            -isysroot ${_ld_sdk}
            -nostdlib
            -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
            -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:$<TARGET_FILE:libdyld>
            -Wl,-dylinker_install_name,/usr/lib/dyld
            -Wl,-platform_version,macos,26.5,26.5
            ${_ld_objs} ${_ld_extra_archives}
            -lSystem
            -o "${_ld_bin}"
    DEPENDS ${_ld_objs} ${_ld_link_targets}
    COMMENT "Linking ld64 (self-host)"
    VERBATIM
)
add_custom_target(ld64_selfhost ALL DEPENDS "${_ld_bin}")
add_dependencies(ld64_selfhost ${_ld_link_targets})

install(PROGRAMS "${_ld_bin}" DESTINATION usr/bin COMPONENT BaseSystem)
