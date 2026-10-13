add_darwin_executable(sh MACOSX_VERSION_MIN 26.5)

get_target_property(_sh_opts sh COMPILE_OPTIONS)
if(_sh_opts)
    list(REMOVE_ITEM _sh_opts -nostdinc)
    set_target_properties(sh PROPERTIES COMPILE_OPTIONS "${_sh_opts}")
endif()

target_sources(sh PRIVATE sh.c)
target_compile_options(sh PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(sh PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(sh libSystem_B_stub dyld)

install(TARGETS sh DESTINATION bin COMPONENT BaseSystem)
