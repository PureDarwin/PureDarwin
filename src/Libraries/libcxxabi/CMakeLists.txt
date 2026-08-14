set(_libcxxabi  "${CMAKE_CURRENT_SOURCE_DIR}")
set(_libcxx     "${CMAKE_CURRENT_SOURCE_DIR}/../libcxx")
set(_libunwind  "${CMAKE_CURRENT_SOURCE_DIR}/../libunwind")
if(NOT EXISTS "${_libcxxabi}/src/cxa_exception.cpp")
    message(FATAL_ERROR "vendored libc++abi sources missing at ${_libcxxabi}/src \
(expected the vendored LLVM libcxxabi subtree checked in here).")
endif()

get_filename_component(_toolchain_bin "${CMAKE_AR}" DIRECTORY)

# SDK/toolchain selection. CMake's APPLE is true for a Darwin target even when
# the host is Linux, so use CMAKE_HOST_APPLE to distinguish native AppleClang
# from osxcross.
if(CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING)
    # PUREDARWIN_MACOSX_SDK is the SDK the rest of the build was configured
    # against - honour it rather than asking xcrun, which answers for whatever
    # DEVELOPER_DIR happens to point at and, when that is a bare SDK tree with
    # no surrounding developer directory, fails and prints its error where the
    # path should be.
    if(PUREDARWIN_MACOSX_SDK AND EXISTS "${PUREDARWIN_MACOSX_SDK}")
        set(_sdk "${PUREDARWIN_MACOSX_SDK}")
    else()
        execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE _sdk
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()

    find_program(_real_clang NAMES clang REQUIRED)

    set(_target_flags -arch "${PUREDARWIN_ARCH}")
else()
    # osxcross tools normally look like:
    #   x86_64-apple-darwin20.4-ar
    get_filename_component(_ar_name "${CMAKE_AR}" NAME)

    if(NOT _ar_name MATCHES "^(.+)-ar$")
        message(FATAL_ERROR
            "Cannot derive the osxcross target triple from CMAKE_AR='${CMAKE_AR}'."
        )
    endif()

    set(_triple "${CMAKE_MATCH_1}")

    if(DEFINED ENV{OSXCROSS_SDK})
        set(_sdk "$ENV{OSXCROSS_SDK}")
    endif()
    if(NOT _sdk)
        set(_sdk "${PUREDARWIN_MACOSX_SDK}")
    endif()

    set(_real_clang "${_toolchain_bin}/${_triple}-clang")
    if(NOT EXISTS "${_real_clang}")
        message(FATAL_ERROR
            "Expected the osxcross clang wrapper at '${_real_clang}'."
        )
    endif()

    set(_target_flags)
endif()

set(_config "${CMAKE_CURRENT_SOURCE_DIR}/config")   # __config_site, __assertion_handler

set(_abi_flags
    ${_target_flags}
    -isysroot ${_sdk}
    # LLVM 21's libc++/libc++abi are written in C++23 (constinit, etc).
    -std=c++23 -nostdinc++ -funwind-tables -fexceptions -fPIC -Os -DNDEBUG
    # charconv.cpp reaches for <shared/fp_bits.h>, which LLVM 21 shares with
    # llvm-libc rather than keeping under libcxx; the headers it needs are
    # vendored at src/Libraries/llvm-libc.
    -I ${CMAKE_CURRENT_SOURCE_DIR}/../llvm-libc
    -I ${CMAKE_CURRENT_SOURCE_DIR}/../llvm-libc/include
    -I ${_config}
    -I ${_libcxxabi}/include
    -I ${_libcxxabi}/src
    -I ${_libcxx}/src
    -D_LIBCXXABI_BUILDING_LIBRARY
    -D_LIBCPP_BUILDING_LIBRARY
    -DLIBCXX_BUILDING_LIBCXXABI
)
# libcxx/include must come before the toolchain's own <stddef.h>/<stdint.h>
# etc. in the search list so #include_next chains resolve to those, but
# where in the list that is depends on the toolchain: real Apple clang's
# resource-dir include is added right after our own -I dirs (so ours must
# be a plain -I, ahead of it), while osxcross's from-source clang build
# puts its bundled libc/stdint headers ahead of everything, even beyond an
# explicit -idirafter (so ours must be -idirafter, or it shadows them and
# math.h/stdint.h contents go missing). PUREDARWIN_NIX_TOOLCHAIN (set by
# cmake/nix-toolchain.cmake) is a thin wrapper around vanilla nixpkgs
# clang, which follows ordinary -idirafter semantics (always dead last) -
# same as real Apple clang here, not osxcross's inverted ordering.
if((CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING) OR PUREDARWIN_NIX_TOOLCHAIN)
    list(APPEND _abi_flags -I ${_libcxx}/include)
else()
    list(APPEND _abi_flags -idirafter ${_libcxx}/include)
endif()

# The subset dyld actually needs. cxa_thread_atexit/cxa_vector/
# aix/noexception are deliberately excluded (not referenced by dyld).
set(_abi_srcs
    cxa_demangle
    stdlib_new_delete
    cxa_aux_runtime
    cxa_handlers
    cxa_default_handlers
    cxa_exception
    cxa_exception_storage
    cxa_personality
    cxa_virtual
    cxa_guard
    private_typeinfo
    stdlib_typeinfo
    stdlib_exception
    stdlib_stdexcept
    fallback_malloc
    abort_message
    pd_bootstrap_runtime
)

set(_abi_objs "")
foreach(s ${_abi_srcs})
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/${s}.o")
    set(_abi_extra "")
    if(s STREQUAL "cxa_exception_storage")
        set(_abi_extra -D_LIBCXXABI_HAS_NO_THREADS)
    endif()
    add_custom_command(
        OUTPUT "${_obj}"
        COMMAND "${_real_clang}" ${_abi_flags} ${_abi_extra}
                -c "${_libcxxabi}/src/${s}.cpp" -o "${_obj}"
        DEPENDS "${_libcxxabi}/src/${s}.cpp"
                "${_config}/__config_site" "${_config}/__assertion_handler"
        COMMENT "Building libc++abi ${s}.cpp (raw clang, vendored LLVM headers)"
        VERBATIM
    )
    list(APPEND _abi_objs "${_obj}")
endforeach()

set(_abi_archive "${CMAKE_CURRENT_BINARY_DIR}/libcxxabi_static.a")
add_custom_command(
    OUTPUT "${_abi_archive}"
    COMMAND "${CMAKE_AR}" crs "${_abi_archive}" ${_abi_objs}
    DEPENDS ${_abi_objs}
    COMMENT "Archiving libcxxabi_static.a"
    VERBATIM
)
add_custom_target(libcxxabi_static_build DEPENDS "${_abi_archive}")

# Expose it as an IMPORTED archive so dyld can link it by target.
add_library(libcxxabi_static STATIC IMPORTED GLOBAL)
set_target_properties(libcxxabi_static PROPERTIES
    IMPORTED_LOCATION "${_abi_archive}")
add_dependencies(libcxxabi_static libcxxabi_static_build)

if(NOT EXISTS "${_libunwind}/src/libunwind.cpp")
    message(FATAL_ERROR "vendored libunwind sources missing at ${_libunwind}/src.")
endif()

set(_uw_flags
    ${_target_flags}
    -isysroot ${_sdk}
    -nostdinc++ -funwind-tables -fPIC -Os -DNDEBUG
    -I ${_libunwind}/include
    -I ${_libunwind}/src
    -I ${_config}
    -D_LIBUNWIND_IS_NATIVE_ONLY
    -D_LIBUNWIND_BUILDING_LIBUNWIND=1
)
# See the matching comment on _abi_flags above for why this is conditional.
if((CMAKE_HOST_APPLE AND NOT CMAKE_CROSSCOMPILING) OR PUREDARWIN_NIX_TOOLCHAIN)
    list(APPEND _uw_flags -I ${_libcxx}/include)
else()
    list(APPEND _uw_flags -idirafter ${_libcxx}/include)
endif()

# x86_64 Itanium/DWARF unwinder subset. Excluded: Unwind-sjlj.c (SjLj EH),
# Unwind-EHABI (ARM32), Unwind-seh (Windows), Unwind-wasm, Unwind_AIXExtras.
set(_uw_cxx    libunwind)
set(_uw_c      UnwindLevel1 UnwindLevel1-gcc-ext)
set(_uw_asm    UnwindRegistersRestore UnwindRegistersSave)

set(_uw_objs "")
foreach(s ${_uw_cxx})
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/uw_${s}.o")
    add_custom_command(OUTPUT "${_obj}"
        COMMAND "${_real_clang}" ${_uw_flags} -std=c++23 -fno-exceptions -fno-rtti
                -c "${_libunwind}/src/${s}.cpp" -o "${_obj}"
        DEPENDS "${_libunwind}/src/${s}.cpp"
        COMMENT "Building libunwind ${s}.cpp (raw clang)" VERBATIM)
    list(APPEND _uw_objs "${_obj}")
endforeach()
foreach(s ${_uw_c})
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/uw_${s}.o")
    add_custom_command(OUTPUT "${_obj}"
        COMMAND "${_real_clang}" ${_uw_flags} -std=c11
                -c "${_libunwind}/src/${s}.c" -o "${_obj}"
        DEPENDS "${_libunwind}/src/${s}.c"
        COMMENT "Building libunwind ${s}.c (raw clang)" VERBATIM)
    list(APPEND _uw_objs "${_obj}")
endforeach()
foreach(s ${_uw_asm})
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/uw_${s}.o")
    add_custom_command(OUTPUT "${_obj}"
        COMMAND "${_real_clang}" ${_uw_flags}
                -c "${_libunwind}/src/${s}.S" -o "${_obj}"
        DEPENDS "${_libunwind}/src/${s}.S"
        COMMENT "Building libunwind ${s}.S (raw clang)" VERBATIM)
    list(APPEND _uw_objs "${_obj}")
endforeach()

set(_uw_archive "${CMAKE_CURRENT_BINARY_DIR}/libunwind_static.a")
add_custom_command(OUTPUT "${_uw_archive}"
    COMMAND "${CMAKE_AR}" crs "${_uw_archive}" ${_uw_objs}
    DEPENDS ${_uw_objs}
    COMMENT "Archiving libunwind_static.a" VERBATIM)
add_custom_target(libunwind_static_build DEPENDS "${_uw_archive}")

add_library(libunwind_static STATIC IMPORTED GLOBAL)
set_target_properties(libunwind_static PROPERTIES
    IMPORTED_LOCATION "${_uw_archive}")
add_dependencies(libunwind_static libunwind_static_build)

# The subset of real libc++ (not just libc++abi's ABI layer) that ICU's C++
# code actually calls: std::mutex/condition_variable/call_once and the
# std::system_error support they need to report pthread failures. Everything
# else in libc++ (containers, iostream, etc) stays unwired - add more source
# files here if/when something else needs them.
# LLVM 21 split several of these out of the files that used to hold them:
# call_once out of mutex, error_category out of system_error, new_helpers out
# of new, and verbose_abort is the new assertion/abort path every header calls.
set(_cxx_srcs
    mutex
    mutex_destructor
    call_once
    condition_variable
    condition_variable_destructor
    system_error
    error_category
    string
    stdexcept
    charconv
    vector
    algorithm
    new
    new_helpers
    verbose_abort
    hash
    # __hash_memory moved here from hash.cpp in LLVM 21.
    functional
    # __rs_default/__rs_get - libSystem has always exported these.
    random_shuffle
)

set(_cxx_objs "")
foreach(s ${_cxx_srcs})
    set(_obj "${CMAKE_CURRENT_BINARY_DIR}/cxx_${s}.o")
    set(_cxx_extra "")
    if(s STREQUAL "string")
        # We don't build a wide-character libc (no real swprintf/wcstod/etc
        # wired up), and nothing in our curated libcxx_static consumers uses
        # std::wstring. string.cpp's explicit basic_string<wchar_t>
        # instantiation and to_wstring() overloads are guarded behind this
        # macro (a small, clearly-marked local patch - see string.cpp) so
        # they don't drag in unimplemented wide-printf symbols.
        set(_cxx_extra -DPD_LIBCXX_NARROW_STRING_ONLY)
    endif()
    if(s STREQUAL "new")
        # operator new/delete themselves already come from libcxxabi's
        # stdlib_new_delete.cpp (in _abi_srcs above); this define skips
        # new.cpp's copies (would otherwise be duplicate symbols) and leaves
        # just the std::nothrow object + __throw_bad_alloc, which is all we
        # actually need this file for.
        set(_cxx_extra -D_LIBCPP_DISABLE_NEW_DELETE_DEFINITIONS)
    endif()
    add_custom_command(
        OUTPUT "${_obj}"
        COMMAND "${_real_clang}" ${_abi_flags} -D_LIBCPP_BUILDING_LIBRARY ${_cxx_extra}
                -c "${_libcxx}/src/${s}.cpp" -o "${_obj}"
        DEPENDS "${_libcxx}/src/${s}.cpp"
                "${_config}/__config_site" "${_config}/__assertion_handler"
        COMMENT "Building libc++ ${s}.cpp (raw clang, vendored LLVM headers)"
        VERBATIM
    )
    list(APPEND _cxx_objs "${_obj}")
endforeach()

set(_cxx_archive "${CMAKE_CURRENT_BINARY_DIR}/libcxx_static.a")
add_custom_command(
    OUTPUT "${_cxx_archive}"
    COMMAND "${CMAKE_AR}" crs "${_cxx_archive}" ${_cxx_objs}
    DEPENDS ${_cxx_objs}
    COMMENT "Archiving libcxx_static.a"
    VERBATIM
)
add_custom_target(libcxx_static_build DEPENDS "${_cxx_archive}")

add_library(libcxx_static STATIC IMPORTED GLOBAL)
set_target_properties(libcxx_static PROPERTIES
    IMPORTED_LOCATION "${_cxx_archive}")
add_dependencies(libcxx_static libcxx_static_build)
