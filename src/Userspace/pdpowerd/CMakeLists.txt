add_darwin_executable(pdpowerd MACOSX_VERSION_MIN 26.5)

get_target_property(_pdpowerd_opts pdpowerd COMPILE_OPTIONS)
if(_pdpowerd_opts)
    list(REMOVE_ITEM _pdpowerd_opts -nostdinc)
    set_target_properties(pdpowerd PROPERTIES COMPILE_OPTIONS "${_pdpowerd_opts}")
endif()

target_sources(pdpowerd PRIVATE pdpowerd.c)
target_compile_options(pdpowerd PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
target_link_options(pdpowerd PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem)
add_dependencies(pdpowerd libSystem_B_stub dyld)

install(TARGETS pdpowerd DESTINATION usr/sbin COMPONENT BaseSystem)
install(FILES org.puredarwin.pdpowerd.plist
        DESTINATION System/Library/LaunchDaemons
        COMPONENT BaseSystem)
