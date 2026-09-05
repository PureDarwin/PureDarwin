function(add_text_cmd name)
    add_darwin_executable(${name} MACOSX_VERSION_MIN 26.5)

    get_target_property(_text_cmd_opts ${name} COMPILE_OPTIONS)
    if(_text_cmd_opts)
        list(REMOVE_ITEM _text_cmd_opts -nostdinc)
        set_target_properties(${name} PROPERTIES COMPILE_OPTIONS "${_text_cmd_opts}")
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

add_text_cmd(banner banner.c)
add_text_cmd(cat cat.c)
add_text_cmd(colrm colrm.c)
add_text_cmd(comm comm.c)
add_text_cmd(cut cut.c)
add_text_cmd(expand expand.c)
add_text_cmd(fold fold.c)
add_text_cmd(head head.c)
add_text_cmd(lam lam.c)
add_text_cmd(look look/look.c)
add_text_cmd(nl nl.c)
add_text_cmd(paste paste.c)
add_text_cmd(rev rev.c)
add_text_cmd(split split.c)
add_text_cmd(tail tail/tail.c tail/forward.c tail/misc.c tail/read.c tail/reverse.c)
add_text_cmd(tr tr/tr.c tr/cmap.c tr/cset.c tr/str.c)
add_text_cmd(unexpand unexpand.c)
add_text_cmd(uniq uniq.c)
add_text_cmd(wc wc.c)

target_include_directories(look PRIVATE look)
target_include_directories(tail PRIVATE tail)
target_include_directories(tr PRIVATE tr)
