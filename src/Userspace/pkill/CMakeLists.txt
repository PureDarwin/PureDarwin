# pkill and pgrep from Apple's adv_cmds-163, one binary that checks the name it runs under.
# pd_sysmon.c answers the libsysmon process table the Apple code asks for.
add_darwin_executable(pkill MACOSX_VERSION_MIN 26.5)

get_target_property(_pkill_opts pkill COMPILE_OPTIONS)
if(_pkill_opts)
    list(REMOVE_ITEM _pkill_opts -nostdinc)
    set_target_properties(pkill PROPERTIES COMPILE_OPTIONS "${_pkill_opts}")
endif()

target_sources(pkill PRIVATE pkill.c pd_sysmon.c)
# os/assumes.h is libc-internal, Security reaches it the same way: links in a private include farm
set(_pkill_farm ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${_pkill_farm}/os)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libc/os/assumes.h ${_pkill_farm}/os/assumes.h SYMBOLIC)
file(CREATE_LINK ${CMAKE_SOURCE_DIR}/src/Libraries/libSystem/libplatform/private/_simple.h ${_pkill_farm}/_simple.h SYMBOLIC)
target_include_directories(pkill PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${_pkill_farm})
target_compile_options(pkill PRIVATE
    -isysroot ${PUREDARWIN_MACOSX_SDK}
    -fno-builtin
    -fno-stack-protector
    -fblocks
)
target_link_options(pkill PRIVATE
    -nostdlib
    -Wl,-L,$<TARGET_FILE_DIR:libSystem_B_stub>
    -Wl,-dylinker_install_name,/usr/lib/dyld
    -Wl,-platform_version,macos,26.5,26.5
    -lSystem
)
add_dependencies(pkill libSystem_B_stub dyld)
install(TARGETS pkill DESTINATION usr/bin COMPONENT BaseSystem)
# pgrep is pkill under another name, as on macOS
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink pkill \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/bin/pgrep\")" COMPONENT BaseSystem)
