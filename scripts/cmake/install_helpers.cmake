function(install_symlink source_path target)
    cmake_parse_arguments(SL "" "COMPONENT" "" ${ARGN})

    get_filename_component(source_dir ${source_path} DIRECTORY)
    get_filename_component(source_name ${source_path} NAME)

    if(SL_COMPONENT)
        install(CODE "
            execute_process(
                COMMAND \${CMAKE_COMMAND} -E create_symlink \"${target}\" \"${source_name}\"
                WORKING_DIRECTORY \"\$ENV{DESTDIR}/\${CMAKE_INSTALL_PREFIX}/${source_dir}\"
            )
            message(STATUS \"Installed symlink ${source_path}\")
        " COMPONENT ${SL_COMPONENT})
    else()
        install(CODE "
            execute_process(
                COMMAND \${CMAKE_COMMAND} -E create_symlink \"${target}\" \"${source_name}\"
                WORKING_DIRECTORY \"\$ENV{DESTDIR}/\${CMAKE_INSTALL_PREFIX}/${source_dir}\"
            )
            message(STATUS \"Installed symlink ${source_path}\")
        ")
    endif()
endfunction()

function(install_manpage source)
    cmake_parse_arguments(MAN "DEVELOPER" "DIRECTORY" "SYMLINKS" ${ARGN})

    if(NOT MAN_DIRECTORY)
        get_filename_component(source_base ${source} NAME)
        get_filename_component(cat ${source} LAST_EXT)
        string(REGEX REPLACE "^\.(.)" "\\1" cat ${cat})

        if(cat STREQUAL "")
            message(SEND_ERROR "Cannot determine section for manpage ${source_base}")
        endif()

        set(MAN_DIRECTORY man${cat})
    endif()

    if(MAN_DEVELOPER)
        set(component DeveloperTools)
    else()
        set(component BaseSystem)
    endif()

    install(FILES ${source} DESTINATION usr/share/man/${MAN_DIRECTORY} COMPONENT ${component})

    foreach(alias ${MAN_SYMLINKS})
        install_symlink(usr/share/man/${MAN_DIRECTORY}/${alias} ${source_base} COMPONENT ${component})
    endforeach()
endfunction()
