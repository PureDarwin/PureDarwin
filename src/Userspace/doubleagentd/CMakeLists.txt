add_darwin_executable(doubleagentd MACOSX_VERSION_MIN 26.5)

get_target_property(_doubleagentd_opts doubleagentd COMPILE_OPTIONS)
if(_doubleagentd_opts)
    list(REMOVE_ITEM _doubleagentd_opts -nostdinc)
    set_target_properties(doubleagentd PROPERTIES COMPILE_OPTIONS "${_doubleagentd_opts}")
endif()

# the interface lives with the kernel, copied so the SDK mach headers stay ahead of osfmk's
set(_doubleagentd_xnu ${CMAKE_SOURCE_DIR}/src/Kernel/xnu/osfmk/mach)
configure_file(${_doubleagentd_xnu}/doubleagent_types.h ${CMAKE_CURRENT_BINARY_DIR}/include/mach/doubleagent_types.h COPYONLY)
configure_file(${_doubleagentd_xnu}/doubleagent_mig.defs ${CMAKE_CURRENT_BINARY_DIR}/include/mach/doubleagent_mig.defs COPYONLY)
target_include_directories(doubleagentd PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})

mig(${CMAKE_CURRENT_BINARY_DIR}/include/mach/doubleagent_mig.defs
    INCLUDES_FROM_TARGET doubleagentd
    SERVER_HEADER ${CMAKE_CURRENT_BINARY_DIR}/doubleagent_migServer.h
    SERVER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/doubleagent_migServer.c
    ARCH ${PUREDARWIN_ARCH}
    NOVOUCHERS)

target_sources(doubleagentd PRIVATE
    doubleagentd.c
    ${CMAKE_CURRENT_BINARY_DIR}/doubleagent_migServer.c)
target_compile_options(doubleagentd PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(doubleagentd PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(doubleagentd libSystem_B_stub dyld)

install(TARGETS doubleagentd DESTINATION usr/libexec COMPONENT BaseSystem)
install(FILES com.apple.doubleagentd.plist
        DESTINATION System/Library/LaunchDaemons
        COMPONENT BaseSystem)
