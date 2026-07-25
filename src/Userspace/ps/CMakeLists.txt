add_darwin_executable(ps MACOSX_VERSION_MIN 11.0)

get_target_property(_ps_opts ps COMPILE_OPTIONS)
if(_ps_opts)
    list(REMOVE_ITEM _ps_opts -nostdinc)
    set_target_properties(ps PROPERTIES COMPILE_OPTIONS "${_ps_opts}")
endif()

target_sources(ps PRIVATE
    ps.c
    fmt.c
    keyword.c
    nlist.c
    print.c
    tasks.c
)

target_compile_options(ps PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(ps PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem
)

add_dependencies(ps libSystem_B_stub dyld)

install(TARGETS ps DESTINATION bin COMPONENT BaseSystem)
