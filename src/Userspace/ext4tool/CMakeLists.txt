add_darwin_executable(ext4tool MACOSX_VERSION_MIN 26.5)

get_target_property(_ext4tool_opts ext4tool COMPILE_OPTIONS)
if(_ext4tool_opts)
    list(REMOVE_ITEM _ext4tool_opts -nostdinc)
    set_target_properties(ext4tool PROPERTIES COMPILE_OPTIONS "${_ext4tool_opts}")
endif()

target_sources(ext4tool PRIVATE
    ext4tool.c
)

target_compile_options(ext4tool PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(ext4tool PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(ext4tool libSystem_B_stub dyld)

install(TARGETS ext4tool DESTINATION bin COMPONENT BaseSystem)
