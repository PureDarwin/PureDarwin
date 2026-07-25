if(NOT PUREDARWIN_XORG_INCLUDE_DIRS)
    message(STATUS "xf86-input-puredarwin: PUREDARWIN_XORG_INCLUDE_DIRS not set, skipping Xorg input driver")
    return()
endif()

# Build as a DYLIB (dyld3 here cannot dlopen an MH_BUNDLE - see the video
# driver's notes). install_name must be the on-image path dyld records.
add_darwin_shared_library(puredarwininput_drv
    MACOSX_VERSION_MIN 11.0
    INSTALL_NAME /usr/lib/xorg/modules/input/puredarwininput_drv.so)

# Xorg's loader looks the module up as "<name>_drv"; strip CMake's "lib".
set_target_properties(puredarwininput_drv PROPERTIES PREFIX "" SUFFIX ".so")

target_link_options(puredarwininput_drv PRIVATE
    -flat_namespace
    "LINKER:-undefined,warning"
    "LINKER:-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/puredarwininput_drv.exports"
    -Wl,-platform_version,macos,11.0,11.5
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

if(NOT TARGET migcom AND DEFINED ENV{NIX_MIGCOM_PATH})
    add_executable(migcom IMPORTED GLOBAL)
    set_target_properties(migcom PROPERTIES IMPORTED_LOCATION "$ENV{NIX_MIGCOM_PATH}")
endif()

add_library(puredarwininput_local_IOKit STATIC)
mig(${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/device/device.defs
    INCLUDES_FROM_TARGET puredarwininput_local_IOKit
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/device_user.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
    NOVOUCHERS)
target_sources(puredarwininput_local_IOKit PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/IOKitLib.c
    ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
)
target_compile_definitions(puredarwininput_local_IOKit PRIVATE IOKIT=1 KERNEL=0)
target_include_directories(puredarwininput_local_IOKit
    PUBLIC ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
)
target_link_libraries(puredarwininput_drv PRIVATE puredarwininput_local_IOKit)
if(TARGET libsystem_kernel_static)
    target_link_libraries(puredarwininput_drv PRIVATE libsystem_kernel_static)
endif()
if(TARGET libsyscall_traps)
    target_link_libraries(puredarwininput_drv PRIVATE libsyscall_traps)
endif()

target_link_options(puredarwininput_drv PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
)
target_link_libraries(puredarwininput_drv PRIVATE System)

install(TARGETS puredarwininput_drv
    DESTINATION usr/lib/xorg/modules/input
    COMPONENT BaseSystem)
