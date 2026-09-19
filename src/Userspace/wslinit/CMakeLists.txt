add_darwin_executable(wslinit MACOSX_VERSION_MIN 26.5)

get_target_property(_wslinit_opts wslinit COMPILE_OPTIONS)
if(_wslinit_opts)
    list(REMOVE_ITEM _wslinit_opts -nostdinc)
    set_target_properties(wslinit PROPERTIES COMPILE_OPTIONS "${_wslinit_opts}")
endif()

target_sources(wslinit PRIVATE
    wslinit.c
)

target_compile_options(wslinit PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(wslinit PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(wslinit libSystem_B_stub dyld)

install(TARGETS wslinit DESTINATION usr/libexec COMPONENT BaseSystem)
