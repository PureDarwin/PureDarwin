if(NOT PUREDARWIN_ENABLE_GLXGEARS)
    return()
endif()

if(NOT PUREDARWIN_MESA_PREFIX)
    message(FATAL_ERROR
        "PUREDARWIN_MESA_PREFIX must point to Mesa's usr prefix")
endif()

if(NOT PUREDARWIN_X11_INCLUDE_DIR)
    message(FATAL_ERROR
        "PUREDARWIN_X11_INCLUDE_DIR must contain X11/Xlib.h")
endif()

if(NOT PUREDARWIN_X11_LIBRARY)
    message(FATAL_ERROR
        "PUREDARWIN_X11_LIBRARY must point to the libX11 library")
endif()

if(NOT PUREDARWIN_XORGPROTO_INCLUDE_DIR)
    message(FATAL_ERROR
        "PUREDARWIN_XORGPROTO_INCLUDE_DIR must contain X11/X.h")
endif()

add_darwin_executable(glxgears MACOSX_VERSION_MIN 11.0)
target_sources(glxgears PRIVATE glxgears.c)

get_target_property(_glxgears_opts glxgears COMPILE_OPTIONS)
if(_glxgears_opts)
    list(REMOVE_ITEM _glxgears_opts -nostdinc)
    set_target_properties(
        glxgears
        PROPERTIES COMPILE_OPTIONS "${_glxgears_opts}")
endif()

target_include_directories(glxgears PRIVATE
    "${PUREDARWIN_MESA_PREFIX}/include"
    "${PUREDARWIN_X11_INCLUDE_DIR}"
    "${PUREDARWIN_XORGPROTO_INCLUDE_DIR}"
)

target_compile_options(glxgears PRIVATE
    -isysroot "${PUREDARWIN_MACOSX_SDK}"
    -Wno-everything
)

target_link_options(glxgears PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -Wl,-fixup_chains

    "${PUREDARWIN_MESA_PREFIX}/lib/libGL.1.dylib"

    "${PUREDARWIN_X11_LIBRARY}"
    "${PUREDARWIN_XCB_LIBRARY}"
    "${PUREDARWIN_XAU_LIBRARY}"
    "${PUREDARWIN_XDMCP_LIBRARY}"

    -lSystem
)

add_dependencies(glxgears
    libSystem_B_stub
    dyld
)

install(
    TARGETS glxgears
    DESTINATION usr/bin
    COMPONENT BaseSystem
)
