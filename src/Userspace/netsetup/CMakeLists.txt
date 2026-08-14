add_darwin_executable(netsetup MACOSX_VERSION_MIN 11.0)

get_target_property(_netsetup_opts netsetup COMPILE_OPTIONS)
if(_netsetup_opts)
    list(REMOVE_ITEM _netsetup_opts -nostdinc)
    set_target_properties(netsetup PROPERTIES COMPILE_OPTIONS "${_netsetup_opts}")
endif()

target_sources(netsetup PRIVATE
    netsetup.c
)

target_compile_options(netsetup PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
)

target_link_options(netsetup PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,11.0,11.5
    -lSystem
)

add_dependencies(netsetup libSystem_B_stub dyld)

install(TARGETS netsetup DESTINATION bin COMPONENT BaseSystem)
