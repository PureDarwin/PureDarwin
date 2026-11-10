add_darwin_executable(pd-dmesg-save MACOSX_VERSION_MIN 26.5)

get_target_property(_pd_dmesg_save_opts pd-dmesg-save COMPILE_OPTIONS)
if(_pd_dmesg_save_opts)
    list(REMOVE_ITEM _pd_dmesg_save_opts -nostdinc)
    set_target_properties(pd-dmesg-save PROPERTIES COMPILE_OPTIONS "${_pd_dmesg_save_opts}")
endif()

target_sources(pd-dmesg-save PRIVATE pd-dmesg-save.c)
target_compile_options(pd-dmesg-save PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(pd-dmesg-save PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(pd-dmesg-save libSystem_B_stub dyld)

install(TARGETS pd-dmesg-save DESTINATION usr/libexec COMPONENT BaseSystem)
