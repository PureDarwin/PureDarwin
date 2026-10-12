add_darwin_executable(pdkext MACOSX_VERSION_MIN 26.5)

get_target_property(_pdkext_opts pdkext COMPILE_OPTIONS)
if(_pdkext_opts)
    list(REMOVE_ITEM _pdkext_opts -nostdinc)
    set_target_properties(pdkext PROPERTIES COMPILE_OPTIONS "${_pdkext_opts}")
endif()

target_sources(pdkext PRIVATE pdkext.c)
target_compile_options(pdkext PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(pdkext PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(pdkext libSystem_B_stub dyld)

install(TARGETS pdkext DESTINATION usr/sbin COMPONENT BaseSystem)
install(FILES org.puredarwin.pdkext.plist
        DESTINATION System/Library/LaunchDaemons
        COMPONENT BaseSystem)
