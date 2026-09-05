add_darwin_executable(sw_vers MACOSX_VERSION_MIN 26.5)

get_target_property(_sw_vers_opts sw_vers COMPILE_OPTIONS)
if(_sw_vers_opts)
    list(REMOVE_ITEM _sw_vers_opts -nostdinc)
    set_target_properties(sw_vers PROPERTIES COMPILE_OPTIONS "${_sw_vers_opts}")
endif()

target_sources(sw_vers PRIVATE
    sw_vers.c
)

target_compile_options(sw_vers PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(sw_vers PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(sw_vers libSystem_B_stub dyld)

install(TARGETS sw_vers DESTINATION bin COMPONENT BaseSystem)
