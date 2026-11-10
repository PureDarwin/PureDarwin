if(NOT PUREDARWIN_XORG_INCLUDE_DIRS)
    message(STATUS "xf86-input-puredarwin: PUREDARWIN_XORG_INCLUDE_DIRS not set, skipping Xorg input driver")
    return()
endif()

# Build as a DYLIB (dyld3 here cannot dlopen an MH_BUNDLE - see the video
# driver's notes). install_name must be the on-image path dyld records.
add_darwin_shared_library(puredarwininput_drv
    MACOSX_VERSION_MIN 26.5
    INSTALL_NAME /usr/lib/xorg/modules/input/puredarwininput_drv.so)

# Xorg's loader looks the module up as "<name>_drv"; strip CMake's "lib".
set_target_properties(puredarwininput_drv PROPERTIES PREFIX "" SUFFIX ".so")

target_link_options(puredarwininput_drv PRIVATE
    -flat_namespace
    "LINKER:-undefined,warning"
    "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/puredarwininput_drv.exports"
    -Wl,-platform_version,macos,26.5,26.5
    -Wl,-lSystem
)
set_property(TARGET puredarwininput_drv APPEND PROPERTY LINK_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/puredarwininput_drv.exports)

get_target_property(_opts puredarwininput_drv COMPILE_OPTIONS)
if(_opts)
    list(REMOVE_ITEM _opts -nostdinc)
    set_target_properties(puredarwininput_drv PROPERTIES COMPILE_OPTIONS "${_opts}")
endif()

target_compile_options(puredarwininput_drv PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_sources(puredarwininput_drv PRIVATE
    puredarwininput_drv.c
)

target_compile_definitions(puredarwininput_drv PRIVATE
    HAVE_XORG_CONFIG_H=1
    IN_MODULE=1
)

target_include_directories(puredarwininput_drv PRIVATE
    ${PUREDARWIN_XORG_INCLUDE_DIRS}
)

target_link_options(puredarwininput_drv PRIVATE
    # the SDK's IOKit.tbd, a flat-namespace link would otherwise pull in the dylib's dependents
    -F${PUREDARWIN_MACOSX_SDK}/System/Library/Frameworks
    -Wl,-framework,IOKit)

target_link_options(puredarwininput_drv PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
)
target_link_libraries(puredarwininput_drv PRIVATE System)

install(TARGETS puredarwininput_drv
    DESTINATION usr/lib/xorg/modules/input
    COMPONENT BaseSystem)
