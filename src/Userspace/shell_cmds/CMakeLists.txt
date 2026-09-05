function(add_shell_cmd name)
    cmake_parse_arguments(SC "" "OUTPUT" "" ${ARGN})

    add_darwin_executable(${name} MACOSX_VERSION_MIN 26.5)

    get_target_property(_shell_cmd_opts ${name} COMPILE_OPTIONS)
    if(_shell_cmd_opts)
        list(REMOVE_ITEM _shell_cmd_opts -nostdinc)
        set_target_properties(${name} PROPERTIES COMPILE_OPTIONS "${_shell_cmd_opts}")
    endif()

    if(SC_OUTPUT)
        set_target_properties(${name} PROPERTIES OUTPUT_NAME ${SC_OUTPUT})
    endif()

    target_sources(${name} PRIVATE ${SC_UNPARSED_ARGUMENTS})
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

add_shell_cmd(basename basename.c)
add_shell_cmd(chown chown.c)
add_shell_cmd(dirname dirname.c)
add_shell_cmd(echo echo.c)
add_shell_cmd(false false.c)
add_shell_cmd(getopt getopt.c)
add_shell_cmd(hostname hostname.c)
add_shell_cmd(jot jot.c)
add_shell_cmd(kill kill.c)
add_shell_cmd(logname logname.c)
add_shell_cmd(mktemp mktemp.c)
add_shell_cmd(nice nice.c)
add_shell_cmd(nohup nohup.c)
# nohup detaches from the console session through _vprocmgr_detach_from_console.
# vproc_priv.h is private and not in the SDK, so take it from our libxpc.
target_include_directories(nohup PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Libraries/XPC/libxpc/include)
add_shell_cmd(printenv printenv.c)
add_shell_cmd(pwd pwd.c)
add_shell_cmd(renice renice.c)
add_shell_cmd(seq seq.c)
add_shell_cmd(shlock shlock.c)
add_shell_cmd(sleep sleep.c)
add_shell_cmd(tee tee.c)
add_shell_cmd(test_cmd OUTPUT test test.c)
add_shell_cmd(true true.c)
add_shell_cmd(tsort tsort.c)
add_shell_cmd(uname uname.c)
add_shell_cmd(yes yes.c)
add_shell_cmd(uuencode uuencode/uuencode.c uuencode/base64.c)
add_shell_cmd(uudecode uudecode/uudecode.c uudecode/base64.c)
