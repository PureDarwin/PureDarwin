add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/systrace_args.c
        ${CMAKE_CURRENT_BINARY_DIR}/audit_kevents.c
        ${CMAKE_CURRENT_BINARY_DIR}/init_sysent.c
        ${CMAKE_CURRENT_BINARY_DIR}/syscalls.c
        ${CMAKE_CURRENT_BINARY_DIR}/ioconf.c
        ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/config
    COMMAND xnu_doconf -c -cpu x86_64 -platform x86_64 -platform MacOSX -soc "" -d ${CMAKE_CURRENT_BINARY_DIR}/config -s ${CMAKE_CURRENT_SOURCE_DIR} -m ${XNU_SOURCE_DIR}/config RELEASE
    COMMAND ${XNU_SOURCE_DIR}/SETUP/makefile_to_prefix.sh ${CMAKE_CURRENT_BINARY_DIR}/Makefile ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h
    DEPENDS xnu_doconf xnu_config_tool COMMENT "Run xnu_doconf for bsd component" VERBATIM
)

add_darwin_object_library(xnu.bsd_conf)
target_sources(xnu.bsd_conf PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/ioconf.c
    ${CMAKE_CURRENT_BINARY_DIR}/systrace_args.c
    ${CMAKE_CURRENT_BINARY_DIR}/audit_kevents.c
    ${CMAKE_CURRENT_BINARY_DIR}/init_sysent.c
    ${CMAKE_CURRENT_BINARY_DIR}/syscalls.c
)

target_include_directories(xnu.bsd_conf PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(xnu.bsd_conf PUBLIC "SHELL:-include ${CMAKE_CURRENT_BINARY_DIR}/config/config-prefix.h")
target_compile_options(xnu.bsd_conf PUBLIC "SHELL:-include ${CMAKE_CURRENT_BINARY_DIR}/meta_features.h")
target_compile_definitions(xnu.bsd_conf PUBLIC DRIVER_PRIVATE _KERNEL_BUILD MACH_KERNEL BSD_BUILD BSD_KERNEL_PRIVATE LD64_DEBUG=0)
