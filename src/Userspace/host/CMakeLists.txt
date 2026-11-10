add_darwin_executable(host MACOSX_VERSION_MIN 26.5)

get_target_property(_host_opts host COMPILE_OPTIONS)
if(_host_opts)
    list(REMOVE_ITEM _host_opts -nostdinc)
    set_target_properties(host PROPERTIES COMPILE_OPTIONS "${_host_opts}")
endif()

target_sources(host PRIVATE
    host.c
)

target_compile_options(host PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    # libresolv's own headers name everything res_9_*, as libSystem exports it
    -I${CMAKE_SOURCE_DIR}/src/Libraries/libresolv
    -fno-builtin
    -fno-stack-protector
)

target_link_options(host PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(host libSystem_B_stub dyld)

install(TARGETS host DESTINATION bin COMPONENT BaseSystem)
