add_darwin_executable(getent MACOSX_VERSION_MIN 26.5)

get_target_property(_getent_opts getent COMPILE_OPTIONS)
if(_getent_opts)
    list(REMOVE_ITEM _getent_opts -nostdinc)
    set_target_properties(getent PROPERTIES COMPILE_OPTIONS "${_getent_opts}")
endif()

target_sources(getent PRIVATE
    getent.c
)

target_compile_options(getent PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(getent PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(getent libSystem_B_stub dyld)

install(TARGETS getent DESTINATION bin COMPONENT BaseSystem)
