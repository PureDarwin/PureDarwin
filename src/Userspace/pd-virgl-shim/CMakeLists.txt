add_library(pd_virgl_shim STATIC)

if(NOT TARGET migcom AND DEFINED ENV{NIX_MIGCOM_PATH})
    add_executable(migcom IMPORTED GLOBAL)
    set_target_properties(migcom PROPERTIES IMPORTED_LOCATION "$ENV{NIX_MIGCOM_PATH}")
endif()

mig(${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/device/device.defs
    INCLUDES_FROM_TARGET pd_virgl_shim
    USER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/device_user.h
    USER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
    NOVOUCHERS)

target_sources(pd_virgl_shim PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Libraries/PDVirglShim/pd_virgl_shim.c
    ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/IOKitLib.c
    ${CMAKE_CURRENT_BINARY_DIR}/device_user.c
)

target_compile_definitions(pd_virgl_shim PRIVATE IOKIT=1 KERNEL=0)
# -UKERNEL: -DKERNEL=0 still *defines* KERNEL, hiding mach/message.h's userspace
#   mach_msg() decl - undo it for the C compile (see IOKit/CMakeLists.txt).
# -mmacosx-version-min: stamp a platform load command on every object, else ld64
#   ignores the archive when Mesa force_loads it ("building for macOS-x86_64 but
#   attempting to link with file built for macOS-x86_64").
target_compile_options(pd_virgl_shim PRIVATE -UKERNEL -mmacosx-version-min=11.0)

target_include_directories(pd_virgl_shim
    PUBLIC ${CMAKE_SOURCE_DIR}/src/Libraries/PDVirglShim/include
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src/Libraries/IOKit/include
        ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libsystem_kernel/mach
        ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk
        ${CMAKE_SOURCE_DIR}/src/Kernel/Extensions/IOVirtIOGPU
)
