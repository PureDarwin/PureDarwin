if(NOT PUREDARWIN_TCC_SOURCE AND DEFINED ENV{PUREDARWIN_TCC_SOURCE})
    set(PUREDARWIN_TCC_SOURCE "$ENV{PUREDARWIN_TCC_SOURCE}")
endif()

if(NOT PUREDARWIN_TCC_SOURCE)
    message(FATAL_ERROR "PUREDARWIN_TCC_SOURCE must point to a TinyCC source tree")
endif()

find_program(PUREDARWIN_MAKE_PROGRAM NAMES gmake make REQUIRED)

set(PUREDARWIN_TCC_SOURCE_COPY "${CMAKE_CURRENT_BINARY_DIR}/src")
set(PUREDARWIN_TCC_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/build")
set(PUREDARWIN_TCC_BIN "${PUREDARWIN_TCC_BUILD_DIR}/tcc")
set(PUREDARWIN_TCC_CONFIGURE "${PUREDARWIN_TCC_SOURCE_COPY}/configure")

set(PUREDARWIN_TCC_LINKER_FLAG "")
if(PUREDARWIN_NIX_TOOLCHAIN AND DEFINED ENV{NIX_NATIVE_LD_PATH})
    set(PUREDARWIN_TCC_LINKER_FLAG "-fuse-ld=$ENV{NIX_NATIVE_LD_PATH}")
endif()

set(PUREDARWIN_TCC_LDFLAGS
    "${PUREDARWIN_TCC_LINKER_FLAG} -nostdlib -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub> -Wl,-dylib_file,/usr/lib/system/libdyld.dylib:$<TARGET_FILE:libdyld> -Wl,-dylinker_install_name,/usr/lib/dyld -Wl,-platform_version,macos,26.5,26.5 -Wl,-fixup_chains -Wl,-lSystem"
)

file(GLOB_RECURSE PUREDARWIN_TCC_SOURCES CONFIGURE_DEPENDS
    "${PUREDARWIN_TCC_SOURCE}/*"
)

add_custom_command(
    OUTPUT "${PUREDARWIN_TCC_BIN}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PUREDARWIN_TCC_SOURCE_COPY}" "${PUREDARWIN_TCC_BUILD_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PUREDARWIN_TCC_SOURCE}" "${PUREDARWIN_TCC_SOURCE_COPY}"
    COMMAND "${CMAKE_COMMAND}" -D "PUREDARWIN_TCC_SOURCE_COPY=${PUREDARWIN_TCC_SOURCE_COPY}" -P "${CMAKE_CURRENT_SOURCE_DIR}/patch-tcc.cmake"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PUREDARWIN_TCC_BUILD_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${PUREDARWIN_TCC_BUILD_DIR}"
            "${CMAKE_COMMAND}" -E env
            CC=${CMAKE_C_COMPILER}
            AR=${CMAKE_AR}
            CFLAGS=-Wall\ -O2\ -I${PUREDARWIN_TCC_BUILD_DIR}\ -I${PUREDARWIN_TCC_SOURCE_COPY}\ -isysroot\ ${PUREDARWIN_MACOSX_SDK}
            LDFLAGS=${PUREDARWIN_TCC_LDFLAGS}
            "${PUREDARWIN_TCC_CONFIGURE}"
                --source-path=${PUREDARWIN_TCC_SOURCE_COPY}
            --prefix=/usr
            --bindir=/bin
            --tccdir=/usr/lib/tcc
            --targetos=Darwin
            --cpu=x86_64
            --sysroot=/
            --sysincludepaths=/usr/lib/tcc/include:/usr/include
            --libpaths=/usr/lib/tcc:/usr/lib:/usr/lib/system
            --crtprefix=/usr/lib
            --config-codesign=no
            --config-ldl=no
            --config-pthread=no
            --config-predefs=no
            --config-new_macho=yes
        COMMAND "${PUREDARWIN_MAKE_PROGRAM}"
            -C "${PUREDARWIN_TCC_BUILD_DIR}"
            -f "${PUREDARWIN_TCC_SOURCE_COPY}/Makefile"
            TOP=${PUREDARWIN_TCC_BUILD_DIR}
            CC=${CMAKE_C_COMPILER}
            AR=${CMAKE_AR}
            LIBS=
            tcc
        DEPENDS
            "$<TARGET_FILE:libSystem_B_stub>"
            "$<TARGET_FILE:libdyld>"
            "${CMAKE_CURRENT_SOURCE_DIR}/patch-tcc.cmake"
            ${PUREDARWIN_TCC_SOURCES}
        VERBATIM
    )

# tcc needs runtime support files under /usr/lib/tcc: its private headers
# (tccdefs.h and friends, since --config-predefs=no) and libtcc1.a. libtcc1.a
# must be compiled BY tcc itself (its linker consumes tcc's ELF objects, even
# when emitting Mach-O), and the cross tcc above can't run on the build host.
# tinycc's Makefile supports exactly this: a native-host build's
# `x86_64-osx-libtcc1.a` target first builds a host-runnable x86_64-osx-tcc
# cross compiler, then uses it to compile lib/*.c.
set(PUREDARWIN_TCC_NATIVE_DIR "${CMAKE_CURRENT_BINARY_DIR}/build-native")
set(PUREDARWIN_TCC_LIBTCC1 "${PUREDARWIN_TCC_NATIVE_DIR}/x86_64-osx-libtcc1.a")

add_custom_command(
    OUTPUT "${PUREDARWIN_TCC_LIBTCC1}"
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PUREDARWIN_TCC_NATIVE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PUREDARWIN_TCC_NATIVE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${PUREDARWIN_TCC_NATIVE_DIR}"
        "${CMAKE_COMMAND}" -E env CC=clang --unset=CFLAGS --unset=LDFLAGS
        "${PUREDARWIN_TCC_CONFIGURE}"
            --source-path=${PUREDARWIN_TCC_SOURCE_COPY}
    # No TOP= here: the Makefile only defines its build rules when TOP is
    # unset at parse time (TOP set means "included from a sub-make").
    COMMAND "${PUREDARWIN_MAKE_PROGRAM}"
        -C "${PUREDARWIN_TCC_NATIVE_DIR}"
        -f "${PUREDARWIN_TCC_SOURCE_COPY}/Makefile"
        CC=clang
        x86_64-osx-libtcc1.a
    # runmain.o (tcc -run support) is an EXTRA_O the Makefile only builds for
    # native compilers; produce it with the freshly built cross tcc.
    COMMAND "${PUREDARWIN_TCC_NATIVE_DIR}/x86_64-osx-tcc"
        -c "${PUREDARWIN_TCC_SOURCE_COPY}/lib/runmain.c"
        -o "${PUREDARWIN_TCC_NATIVE_DIR}/runmain.o"
        -B "${PUREDARWIN_TCC_SOURCE_COPY}"
    DEPENDS "${PUREDARWIN_TCC_BIN}"
    VERBATIM
)

add_custom_target(tcc ALL DEPENDS "${PUREDARWIN_TCC_BIN}" "${PUREDARWIN_TCC_LIBTCC1}")
add_dependencies(tcc libSystem_B_stub dyld)

install(PROGRAMS "${PUREDARWIN_TCC_BIN}" DESTINATION bin COMPONENT BaseSystem)
install(FILES "${PUREDARWIN_TCC_LIBTCC1}" DESTINATION usr/lib/tcc
    RENAME libtcc1.a COMPONENT BaseSystem)
install(DIRECTORY "${PUREDARWIN_TCC_SOURCE_COPY}/include/"
    DESTINATION usr/lib/tcc/include COMPONENT BaseSystem
    FILES_MATCHING PATTERN "*.h")
