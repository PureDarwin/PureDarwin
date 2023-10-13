cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN_TOOLS)

function(add_host_tool name)
    add_executable(${name})
    install(TARGETS ${name})

    if(TOOLS_FOLDER)
        set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TOOLS_FOLDER})
    endif()
endfunction()

add_subdirectory(cctools)
add_subdirectory(dtrace_ctf)
add_subdirectory(mig)
add_subdirectory(unifdef)
add_subdirectory(xar)
