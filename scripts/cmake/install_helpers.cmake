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
