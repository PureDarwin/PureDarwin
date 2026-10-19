add_darwin_executable(drmprobe MACOSX_VERSION_MIN 26.5)

get_target_property(_drmprobe_opts drmprobe COMPILE_OPTIONS)
if(_drmprobe_opts)
    list(REMOVE_ITEM _drmprobe_opts -nostdinc)
    set_target_properties(drmprobe PROPERTIES COMPILE_OPTIONS "${_drmprobe_opts}")
endif()

target_sources(drmprobe PRIVATE drmprobe.c)
target_compile_options(drmprobe PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(drmprobe PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(drmprobe libSystem_B_stub dyld)

install(TARGETS drmprobe DESTINATION usr/sbin COMPONENT BaseSystem)
