add_darwin_executable(netsetup MACOSX_VERSION_MIN 11.0)
add_darwin_executable(pd-networkd MACOSX_VERSION_MIN 11.0)

foreach(_pd_net_target netsetup pd-networkd)
    get_target_property(_netsetup_opts ${_pd_net_target} COMPILE_OPTIONS)
    if(_netsetup_opts)
        list(REMOVE_ITEM _netsetup_opts -nostdinc)
        set_target_properties(${_pd_net_target} PROPERTIES COMPILE_OPTIONS "${_netsetup_opts}")
    endif()

    target_sources(${_pd_net_target} PRIVATE
        netsetup.c
    )

    target_compile_options(${_pd_net_target} PRIVATE
        -isysroot ${PUREDARWIN_MACOSX_SDK}
        -fno-builtin
        -fno-stack-protector
    )

    target_link_options(${_pd_net_target} PRIVATE
        -nostdlib
        -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
        -Wl,-dylinker_install_name,/usr/lib/dyld
        -Wl,-platform_version,macos,11.0,11.5
        -lSystem
    )

    add_dependencies(${_pd_net_target} libSystem_B_stub dyld)
endforeach()

install(TARGETS netsetup DESTINATION bin COMPONENT BaseSystem)
install(TARGETS pd-networkd DESTINATION usr/libexec COMPONENT BaseSystem)
install(FILES org.puredarwin.network.plist DESTINATION System/Library/LaunchDaemons COMPONENT BaseSystem)
