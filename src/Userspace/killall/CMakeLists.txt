# killall from Apple's shell_cmds-207.11.1, the macOS one
add_darwin_executable(killall MACOSX_VERSION_MIN 26.5)

get_target_property(_killall_opts killall COMPILE_OPTIONS)
if(_killall_opts)
    list(REMOVE_ITEM _killall_opts -nostdinc)
    set_target_properties(killall PROPERTIES COMPILE_OPTIONS "${_killall_opts}")
endif()

target_sources(killall PRIVATE killall.c)
target_compile_options(killall PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)
target_link_options(killall PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)
add_dependencies(killall libSystem_B_stub dyld)
install(TARGETS killall DESTINATION usr/bin COMPONENT BaseSystem)
