foreach(_pd_tgt mount umount)
    add_darwin_executable(${_pd_tgt} MACOSX_VERSION_MIN 11.0)

    get_target_property(_pd_opts ${_pd_tgt} COMPILE_OPTIONS)
    if(_pd_opts)
        list(REMOVE_ITEM _pd_opts -nostdinc)
        set_target_properties(${_pd_tgt} PROPERTIES COMPILE_OPTIONS "${_pd_opts}")
    endif()

    target_sources(${_pd_tgt} PRIVATE
        ${_pd_tgt}.c
    )

    target_compile_options(${_pd_tgt} PRIVATE
        -isysroot ${PUREDARWIN_MACOSX_SDK}
        -fno-builtin
        -fno-stack-protector
    )

    target_link_options(${_pd_tgt} PRIVATE
        -nostdlib
        -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
        -Wl,-dylinker_install_name,/usr/lib/dyld
        -Wl,-platform_version,macos,11.0,11.5
        -lSystem
    )

    add_dependencies(${_pd_tgt} libSystem_B_stub dyld)
    # sbin, not bin: diskarbitrationd execs /sbin/mount and /sbin/umount by
    # absolute path, and that is where Apple ships them.
    install(TARGETS ${_pd_tgt} DESTINATION sbin COMPONENT BaseSystem)
endforeach()
