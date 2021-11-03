macro(add_bundle_resources target)
    cmake_parse_arguments(SL "" "SUBDIRECTORY" "FILES" ${ARGN})

    foreach(file IN LISTS SL_FILES)
        set_property(TARGET ${target} APPEND PROPERTY RESOURCE ${file})
        set_property(SOURCE ${file} PROPERTY MACOSX_PACKAGE_LOCATION Resources/${SL_SUBDIRECTORY})
        target_sources(${target} PRIVATE ${file})
    endforeach()
endmacro()

function(add_bundle_iconset target iconset)
    get_filename_component(base ${iconset} NAME_WE)
    get_filename_component(iconset ${iconset} ABSOLUTE)

    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${base}.icns
        COMMAND iconutil convert --convert icns --output ${CMAKE_CURRENT_BINARY_DIR}/${base}.icns ${iconset}
        DEPENDS ${iconset}
        COMMENT "iconutil ${base}.icns" VERBATIM
    )
    add_bundle_resources(${target} FILES ${CMAKE_CURRENT_BINARY_DIR}/${base}.icns)
endfunction()
