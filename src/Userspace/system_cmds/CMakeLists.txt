function(add_system_cmd name)
    add_darwin_executable(${name} MACOSX_VERSION_MIN 26.5)

    get_target_property(_system_cmd_opts ${name} COMPILE_OPTIONS)
    if(_system_cmd_opts)
        list(REMOVE_ITEM _system_cmd_opts -nostdinc)
        set_target_properties(${name} PROPERTIES COMPILE_OPTIONS "${_system_cmd_opts}")
    endif()

    target_sources(${name} PRIVATE ${ARGN})
    target_compile_options(${name} PRIVATE
        -isysroot ${PUREDARWIN_MACOSX_SDK}
        -fno-builtin
        -fno-stack-protector
    )
    target_link_options(${name} PRIVATE
        -nostdlib
        -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
        -Wl,-dylinker_install_name,/usr/lib/dyld
        -Wl,-platform_version,macos,26.5,26.5
        -lSystem
    )
    add_dependencies(${name} libSystem_B_stub dyld)
    install(TARGETS ${name} DESTINATION bin COMPONENT BaseSystem)
endfunction()

add_system_cmd(mkfile mkfile.c)
add_system_cmd(sync sync.c)
add_system_cmd(sysctl sysctl.c)
add_system_cmd(vm_stat vm_stat.c)
add_system_cmd(hostinfo hostinfo.c)
add_system_cmd(dmesg dmesg.c)
add_system_cmd(purge purge.c)
add_system_cmd(cpuctl cpuctl.c)
add_system_cmd(mean mean.c)

add_system_cmd(passwd passwd.tproj/passwd.c passwd.tproj/file_passwd.c)
target_include_directories(passwd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/passwd.tproj)

foreach(_pd_ctl reboot halt poweroff shutdown)
    add_system_cmd(${_pd_ctl} pd_shutdown.c)
endforeach()
if(NOT PUREDARWIN_COREFOUNDATION_PREFIX AND DEFINED ENV{PUREDARWIN_COREFOUNDATION_PREFIX})
    set(PUREDARWIN_COREFOUNDATION_PREFIX "$ENV{PUREDARWIN_COREFOUNDATION_PREFIX}")
endif()
if(NOT PUREDARWIN_COREFOUNDATION_PREFIX)
    message(FATAL_ERROR "PUREDARWIN_COREFOUNDATION_PREFIX must point to the CoreFoundation install prefix")
endif()

add_system_cmd(sudo sudo.c ../loginwindow/pdaccounts.c)
target_include_directories(sudo PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../loginwindow"
    "${CMAKE_SOURCE_DIR}/src/Libraries/CommonCrypto/include"
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/include"
)
target_compile_options(sudo PRIVATE
    -F${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks
)
target_link_options(sudo PRIVATE
    "${PUREDARWIN_COREFOUNDATION_PREFIX}/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation"
)
