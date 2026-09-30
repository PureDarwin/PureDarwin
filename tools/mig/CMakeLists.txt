set(MIGCOM_SRCS
    error.c
    global.c
    header.c
    mig.c
    routine.c
    server.c
    statement.c
    string.c
    type.c
    user.c
    utils.c

    ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
    ${CMAKE_CURRENT_BINARY_DIR}/lexxer.yy.c
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
    COMMAND yacc -d -b "${CMAKE_CURRENT_BINARY_DIR}/y" ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
    COMMENT "Yacc parser.y" VERBATIM
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lexxer.yy.c
    COMMAND lex -o "${CMAKE_CURRENT_BINARY_DIR}/lexxer.yy.c" ${CMAKE_CURRENT_SOURCE_DIR}/lexxer.l
    COMMENT "Lex lexxer.l" VERBATIM
)

if(CMAKE_HOST_APPLE AND NOT DEFINED ENV{NIX_MIGCOM_PATH})
    add_executable(migcom)
    target_sources(migcom PRIVATE ${MIGCOM_SRCS})
    target_compile_definitions(migcom PRIVATE MIG_VERSION=\"\")
    target_include_directories(migcom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
elseif(DEFINED ENV{NIX_MIGCOM_PATH})
    # Prebuilt by the standalone migcom.nix Nix derivation (see build.nix)
    # instead of built in-tree here, so an edit anywhere else in the
    # project doesn't force rebuilding it.
    add_executable(migcom IMPORTED GLOBAL)
    set_property(TARGET migcom PROPERTY IMPORTED_LOCATION "$ENV{NIX_MIGCOM_PATH}")
else()
    # migcom is a pure source-to-source generator (MIG stub .c files); it
    # never needs to be Mach-O, only to actually RUN during the build. But
    # add_executable() here would inherit the project's global cross-
    # compiler like everything else, making it one anyway - a real
    # Canadian-cross gap, not specific to any particular cross toolchain
    # (confirmed: even a from-source osxcross build produces a Mach-O
    # migcom, equally unable to execute on the Linux build host). CMake
    # has no supported way to give one target a different compiler than
    # the rest of the project within a single configure, so build it with
    # raw add_custom_command calls against whatever "cc" real native
    # compiler is on PATH instead - correct on any Linux host regardless
    # of which cross toolchain (osxcross or this one) is otherwise in use.
    find_program(MIGCOM_NATIVE_CC NAMES cc gcc clang REQUIRED)
    set(_migcom_native_cflags
        -std=gnu89
        -D__private_extern__=
        -D__LITTLE_ENDIAN__
    )

    # Stage just the SDK's mach/ header subtree into an isolated shim dir
    # rather than adding the whole SDK usr/include to the search path.
    # mig's sources only need mach-specific types (mach/boolean.h,
    # mach/message.h, ...) that glibc simply doesn't have; the SDK's
    # errno.h/stdio.h/string.h etc use __asm("_fputs")-style Mach-O
    # symbol-rename annotations that poison this native ELF build if
    # they're reachable at all - even as -idirafter (last-resort) entries,
    # they won races against the native-clang wrapper's own injected
    # glibc -idirafter for reasons not fully tracked down. A shim
    # directory containing only mach/ has no errno.h/stdio.h/string.h to
    # ever collide with, sidestepping the ordering fight entirely.
    # mach/i386/vm_types.h in turn wants i386/_types.h (a sibling top-level
    # dir in the SDK, not under mach/ itself) - stage that too.
    set(_mach_shim "${CMAKE_CURRENT_BINARY_DIR}/mach_shim")
    add_custom_command(OUTPUT "${_mach_shim}/mach/boolean.h"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/mach" "${_mach_shim}/mach"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/i386" "${_mach_shim}/i386"
        # i386/_types.h unconditionally (re)defines __int64_t/__uint64_t/
        # __mbstate_t with no include guards of its own - real, low-level
        # ABI typedefs that glibc's bits/types.h (already active via the
        # forced sys/types.h include below) ALSO defines, spelled
        # differently (long vs long long; both are 64-bit, but C treats
        # them as distinct types, so both definitions active at once is a
        # hard conflict, not a missing-file problem). Since glibc's
        # versions are already in scope by the time this header is ever
        # reached, just strip Apple's redundant redefinitions from the
        # staged copy rather than the original SDK file.
        COMMAND sed -i
                -e "/typedef long long *__int64_t;/d"
                -e "/typedef unsigned long long *__uint64_t;/d"
                -e "/^typedef union {/,/} __mbstate_t;/d"
                "${_mach_shim}/i386/_types.h"
        # Only the underscore-prefixed sys/*.h (type-only, glibc has none
        # of these - no collision risk) and sys/_types/ subdirectory, NOT
        # the whole sys/ tree: real (non-underscore) sys/*.h files like
        # sys/select.h, sys/socket.h duplicate territory glibc's own
        # sys/types.h transitively #include <sys/select.h>s into, and
        # Apple's copies need sys/cdefs.h machinery that can't coexist
        # with glibc's declarations (see below).
        COMMAND ${CMAKE_COMMAND} -E make_directory "${_mach_shim}/sys"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${PUREDARWIN_MACOSX_SDK}/usr/include/sys/_types.h"
                "${PUREDARWIN_MACOSX_SDK}/usr/include/sys/_endian.h"
                "${PUREDARWIN_MACOSX_SDK}/usr/include/sys/appleapiopts.h"
                "${_mach_shim}/sys/"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/sys/_types" "${_mach_shim}/sys/_types"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/sys/_pthread" "${_mach_shim}/sys/_pthread"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/machine" "${_mach_shim}/machine"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/architecture" "${_mach_shim}/architecture"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/_types" "${_mach_shim}/_types"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${PUREDARWIN_MACOSX_SDK}/usr/include/libkern" "${_mach_shim}/libkern"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${PUREDARWIN_MACOSX_SDK}/usr/include/Availability.h"
                "${PUREDARWIN_MACOSX_SDK}/usr/include/AvailabilityInternal.h"
                "${PUREDARWIN_MACOSX_SDK}/usr/include/AvailabilityVersions.h"
                "${PUREDARWIN_MACOSX_SDK}/usr/include/AvailabilityMacros.h"
                "${_mach_shim}/"
        COMMENT "Staging mach/ headers for native migcom build"
        VERBATIM
    )

    set(_migcom_objs "")
    foreach(_src ${MIGCOM_SRCS})
        # get_filename_component(... ABSOLUTE) resolves plain filenames
        # ("error.c") against CMAKE_CURRENT_SOURCE_DIR while leaving the
        # two already-absolute generated sources (y.tab.c, lexxer.yy.c,
        # under CMAKE_CURRENT_BINARY_DIR) unchanged - custom commands run
        # from the build directory, not the source directory, so a bare
        # relative filename resolves nowhere.
        get_filename_component(_src_abs "${_src}" ABSOLUTE)
        get_filename_component(_name "${_src}" NAME_WE)
        set(_obj "${CMAKE_CURRENT_BINARY_DIR}/migcom_native/${_name}.o")
        add_custom_command(OUTPUT "${_obj}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/migcom_native"
            # mig's own sources #include <mach/boolean.h> etc, and rely on
            # u_int/u_short/etc BSD typedefs normally pulled in
            # transitively - force sys/types.h (real glibc one; the mach
            # shim below contains no sys/types.h to conflict) so those are
            # always defined regardless of header include order. Also force
            # glibc's __mbstate_t (bits/types/__mbstate_t.h, guarded by
            # ____mbstate_t_defined) ahead of time: unlike __int64_t/
            # __uint64_t (no reusable per-typedef guard in glibc, so the
            # sed below just deletes Apple's redundant copies), sys/types.h
            # alone doesn't pull this one in, and i386/_types.h's own
            # __mbstate_t definition was stripped below too (same conflict
            # class), so something has to define it before mach headers
            # reference it.
            COMMAND "${MIGCOM_NATIVE_CC}" ${_migcom_native_cflags} -DMIG_VERSION=\"\"
                    -I "${CMAKE_CURRENT_SOURCE_DIR}" -I "${CMAKE_CURRENT_BINARY_DIR}"
                    -I "${_mach_shim}"
                    -include sys/types.h -include bits/types/__mbstate_t.h
                    -c "${_src_abs}" -o "${_obj}"
            DEPENDS "${_src_abs}" "${_mach_shim}/mach/boolean.h"
            COMMENT "Building native migcom: ${_name}.c"
            VERBATIM
        )
        list(APPEND _migcom_objs "${_obj}")
    endforeach()

    # Built as migcom_native/migcom.native, deliberately NOT named exactly
    # "migcom" anywhere on disk: CMake's dependency resolution otherwise
    # conflates a custom_target's DEPENDS file path with a same-named
    # IMPORTED target's IMPORTED_LOCATION, producing a bogus self-cycle
    # ("migcom_build depends on migcom_build") at generate time.
    set(_migcom_bin "${CMAKE_CURRENT_BINARY_DIR}/migcom_native/migcom.native")
    add_custom_command(OUTPUT "${_migcom_bin}"
        COMMAND "${MIGCOM_NATIVE_CC}" -o "${_migcom_bin}" ${_migcom_objs}
        DEPENDS ${_migcom_objs}
        COMMENT "Linking native migcom"
        VERBATIM
    )
    # Other CMakeLists (cmake/mig.cmake, libsystem_kernel, ...) reference
    # $<TARGET_FILE:migcom>, which only resolves for a real executable/
    # library target, not a custom_target - so migcom itself has to stay
    # an (IMPORTED) executable target pointing at the binary a separate
    # custom_target actually builds.
    add_custom_target(migcom_build ALL DEPENDS "${_migcom_bin}")
    add_executable(migcom IMPORTED GLOBAL)
    set_property(TARGET migcom PROPERTY IMPORTED_LOCATION "${_migcom_bin}")
    add_dependencies(migcom migcom_build)
endif()

add_executable(mig IMPORTED)
set_property(TARGET mig PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/mig.sh)
add_dependencies(mig migcom)
