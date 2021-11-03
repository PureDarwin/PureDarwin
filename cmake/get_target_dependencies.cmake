# From: https://stackoverflow.com/questions/63977013/cmake-print-properties-of-a-target-including-his-dependencies

function(list_add_if_not_present list elem)
    list(FIND "${list}" "${elem}" exists)
    if(exists EQUAL -1)
        list(APPEND "${list}" "${elem}")
        set("${list}" "${${list}}" PARENT_SCOPE)
     endif()
endfunction()

macro(_target_get_linked_libraries_in _target _outlist)
    list_add_if_not_present("${_outlist}" "${_target}")

    # get libraries
    get_target_property(public_libs "${_target}" INTERFACE_LINK_LIBRARIES)
    get_target_property(libs "${_target}" LINK_LIBRARIES)

    set(libs_to_check)
    if(public_libs)
        foreach(lib IN LISTS public_libs)
            list(APPEND libs_to_check ${lib})
        endforeach()
    endif()
    if(libs)
        foreach(lib IN LISTS libs)
            list(APPEND libs_to_check ${lib})
        endforeach()
    endif()

    foreach(lib IN LISTS libs_to_check)
        if(NOT TARGET "${lib}")
            continue()
        endif()

        list(FIND "${_outlist}" "${lib}" exists)
        if(NOT exists EQUAL -1)
            continue()
        endif()

        _target_get_linked_libraries_in("${lib}" "${_outlist}")
    endforeach()
endmacro()

function(target_get_library_dependencies _target _outlist)
    set(${_outlist} "${_target}")
    _target_get_linked_libraries_in(${_target} ${_outlist})
    set(${_outlist} ${${_outlist}} PARENT_SCOPE)
endfunction()
