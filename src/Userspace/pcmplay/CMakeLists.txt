add_darwin_executable(pcmplay MACOSX_VERSION_MIN 26.5)

get_target_property(_pcmplay_opts pcmplay COMPILE_OPTIONS)
if(_pcmplay_opts)
    list(REMOVE_ITEM _pcmplay_opts -nostdinc)
    set_target_properties(pcmplay PROPERTIES COMPILE_OPTIONS "${_pcmplay_opts}")
endif()

target_sources(pcmplay PRIVATE
    pcmplay.c
)

target_compile_options(pcmplay PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(pcmplay PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)

add_dependencies(pcmplay libSystem_B_stub dyld)

install(TARGETS pcmplay DESTINATION bin COMPONENT BaseSystem)
