# xf86-video-puredarwingop: loadable Xorg video driver module for
# IOGOPFramebuffer. Built only when the Xorg module SDK path is provided
# (PUREDARWIN_XORG_INCLUDE_DIRS, threaded in from the nix xorgBuild); the SDK
# is a large separate build, so plain `cmake` runs without it just skip this.

if(NOT PUREDARWIN_XORG_INCLUDE_DIRS)
    message(STATUS "xf86-video-puredarwingop: PUREDARWIN_XORG_INCLUDE_DIRS not set, skipping Xorg driver")
    return()
endif()

# Build the driver as a DYLIB, not an MH_BUNDLE.
#
# The natural choice for an xf86 loadable module is -bundle (MH_BUNDLE), and
# that is what this was. But PD's dyld3 cannot dlopen an MH_BUNDLE: its closure
# builder aborts inside dlopen_internal (freeing a pointer libmalloc never
# handed out) before it even reports an error. Reproduced standalone in
# malloctest - dlopen of a dylib succeeds 20x in a row, dlopen of this exact
# bundle kills the process. That dyld bug is worth fixing on its own, but Xorg's
# loader only calls dlopen() on the file; it does not care about the Mach-O
# filetype. A dylib is the format we know loads, so use it.
#
# The install_name must be the path the module actually lives at on the image,
# because that is what dyld records and resolves.
add_darwin_shared_library(puredarwingop_drv
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /usr/lib/xorg/modules/drivers/puredarwingop_drv.so)

# Strip CMake's default "lib" prefix and emit the filename Xorg's loader
# probes: <name>_drv.so. The Mach-O filetype is still a dylib; only the
# filesystem suffix is .so.
set_target_properties(puredarwingop_drv PROPERTIES PREFIX "" SUFFIX ".so")

# xf86 modules resolve the server's own symbols (xf86DrvMsg, fbScreenInit,
# XNFcallocarray, ...) at dlopen time from the already-loaded Xorg image, so
# they must be left undefined here.
#
# NOT -undefined dynamic_lookup: this ld64 rejects it outright in a dylib ("-
# undefined dynamic_lookup cannot be used to find '_XNFcallocarray' in dylib in
# dyld shared cache"). Use the flat-namespace form instead - the same thing PD's
# own tcc build does (-flat_namespace -undefined warning) - which leaves the
# xf86 symbols undefined for dyld to bind flatly against the Xorg image.
#
# -exported_symbols_list is not optional here. A dylib exports every global
# symbol by default, and this one statically links libsystem_kernel.a +
# syscalls.a - so without a list it would publish _NDR_record, mach_msg and the
# whole mach/libc surface. Combined with -flat_namespace that pollutes the
# global symbol namespace and lets the module rebind symbols out from under Xorg
# and libSystem, which is a fine way to crash the server in its own loader
# before the driver's first line ever runs. Export only the two entry points
# Xorg looks up.
target_link_options(puredarwingop_drv PRIVATE
    -flat_namespace
    "LINKER:-undefined,warning"
    "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/puredarwingop_drv.exports"
    -Wl,-platform_version,macos,11.0,11.5
    # -lSystem MUST precede the static archives below.
    #
    # We link libsystem_kernel.a for _NDR_record and the mach_msg entry points
    # (libSystem.B.dylib does not export them). But that archive ALSO contains
    # libkernel's _malloc/_free shims, which dispatch through a function-pointer
    # table that only libSystem initializes - never a plugin. With -lSystem last,
    # ld satisfied our free(devSections) from the archive, so the driver called a
    # shim whose table was NULL and faulted at table+0x18 inside PDGOPProbe.
    # ld resolves left to right: with libSystem first, _malloc/_free are already
    # defined by the time ld reaches the archive, so those members are never
    # pulled, while _NDR_record/mach_msg still are.
    -Wl,-lSystem
)
set_property(TARGET puredarwingop_drv APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/puredarwingop_drv.exports)

# Drop the -nostdinc the helper adds so we can reach the Xorg SDK headers,
# then re-add the SDK sysroot explicitly.
get_target_property(_opts puredarwingop_drv COMPILE_OPTIONS)
if(_opts)
    list(REMOVE_ITEM _opts -nostdinc)
    set_target_properties(puredarwingop_drv PROPERTIES COMPILE_OPTIONS "${_opts}")
endif()

target_compile_options(puredarwingop_drv PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_sources(puredarwingop_drv PRIVATE
    puredarwingop_drv.c
)

# The xf86 SDK headers ship without the usual autotools -D flags; the server
# is built with these and its headers assume them.
target_compile_definitions(puredarwingop_drv PRIVATE
    HAVE_XORG_CONFIG_H=1
    IN_MODULE=1
)

# PUREDARWIN_XORG_INCLUDE_DIRS is a ;-list of: the xorg module SDK
# (usr/include/xorg), xorgproto (X11/*.h), and pixman-1.
target_include_directories(puredarwingop_drv PRIVATE
    ${PUREDARWIN_XORG_INCLUDE_DIRS}
)

# PDGOP (static) pulls in IOKit + the mig device_user stubs transitively.
# The userland build has enableLibraries=OFF, so the top-level PDGOP target
# isn't defined there - build local pdgop_local_IOKit/_PDGOP static libs with
# the same sources + mig stubs (this used to reuse targets the now-removed
# iokittest build happened to define as a side effect; build them directly
# here instead so this doesn't depend on an unrelated tool existing).
if(TARGET PDGOP)
    target_link_libraries(puredarwingop_drv PRIVATE PDGOP)
else()
    if(NOT TARGET migcom AND DEFINED ENV{NIX_MIGCOM_PATH})
        add_executable(migcom IMPORTED GLOBAL)
        set_target_properties(migcom PROPERTIES IMPORTED_LOCATION "$ENV{NIX_MIGCOM_PATH}")
    endif()

    add_library(pdgop_local_IOKit STATIC)
    mig(${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/device/device.defs
        INCLUDES_FROM_TARGET pdgop_local_IOKit
        USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/device_user.h
        USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
        NOVOUCHERS)
    target_sources(pdgop_local_IOKit PRIVATE
        ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/IOKitLib.c
        ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
    )
    target_compile_definitions(pdgop_local_IOKit PRIVATE IOKIT=1 KERNEL=0)
    # See xf86-input-puredarwin: -DKERNEL=0 still defines KERNEL.
    target_compile_options(pdgop_local_IOKit PRIVATE -UKERNEL)
    target_include_directories(pdgop_local_IOKit
        PUBLIC ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/include
        PRIVATE
            ${CMAKE_CURRENT_BINARY_DIR}
            ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    )

    add_library(pdgop_local_PDGOP STATIC)
    target_sources(pdgop_local_PDGOP PRIVATE
        ${CMAKE_SOURCE_DIR}/src/Libraries/PDGOP/PDGOP.c
    )
    target_include_directories(pdgop_local_PDGOP
        PUBLIC
            ${CMAKE_SOURCE_DIR}/src/Libraries/PDGOP/include
            ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/include
        PRIVATE ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
    )
    target_link_libraries(puredarwingop_drv PRIVATE pdgop_local_PDGOP pdgop_local_IOKit)
    if(TARGET libsystem_kernel_static)
        target_link_libraries(puredarwingop_drv PRIVATE libsystem_kernel_static)
    endif()
    if(TARGET libsyscall_traps)
        target_link_libraries(puredarwingop_drv PRIVATE libsyscall_traps)
    endif()
endif()

# Deliberately do NOT link libsystem_kernel_static / libsyscall_traps here.
#
# The mig-generated IOKit stubs need _NDR_record and the mach_msg/mach_port
# entry points, and the obvious move is to static-link libsystem_kernel like
# iokittest does. That is wrong for a plugin: the archive also carries
# libkernel's _malloc/_free shims, which dispatch through a function-pointer
# table that only libSystem's initializer fills in. Linked into a module, those
# shims are live but their table is NULL, so the first free() in the driver
# (free(devSections) in Probe) faulted at table+0x18 - the SIGSEGV at 0x18 that
# killed the server before any driver log line appeared.
#
# libSystem.B.dylib already links libsystem_kernel internally, so it *has* these
# symbols; they were simply missing from its export list (see
# libSystem/stub/libSystem.exports). Export them there and link -lSystem only,
# so malloc/free resolve to the real libmalloc.

# Link against the libSystem stub for the C runtime; server symbols stay
# dynamic_lookup (above).
target_link_options(puredarwingop_drv PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
)
target_link_libraries(puredarwingop_drv PRIVATE System)

install(TARGETS puredarwingop_drv
    DESTINATION usr/lib/xorg/modules/drivers
    COMPONENT BaseSystem)
