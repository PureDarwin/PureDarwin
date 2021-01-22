add_executable(availability.pl IMPORTED)
set_property(TARGET availability.pl PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/availability.pl)

add_library(AvailabilityHeaders INTERFACE)
target_include_directories(AvailabilityHeaders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(AvailabilityHeaders INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include)

add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/include/dyld/VersionMap.h
        ${CMAKE_CURRENT_BINARY_DIR}/include/dyld/for_dyld_priv.inc
    COMMAND ${CMAKE_COMMAND} -D SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -D HEADER_DIR=${CMAKE_CURRENT_BINARY_DIR}/include -P ${CMAKE_CURRENT_SOURCE_DIR}/build_headers.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Creating VersionMap.h and for_dyld_priv.inc" VERBATIM
)

add_dependencies(AvailabilityHeaders
    ${CMAKE_CURRENT_BINARY_DIR}/include/dyld/VersionMap.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/dyld/for_dyld_priv.inc
)

install(FILES include/AvailabilityVersions.h DESTINATION usr/include COMPONENT BaseSystem)
install(DIRECTORY DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A/Headers COMPONENT BaseSystem)
install(CODE [[
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../../../../../usr/include/AvailabilityVersions.h AvailabilityVersions.h
        WORKING_DIRECTORY $ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers
    )

    if(EXISTS $ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/System/Library/Frameworks/Kernel.framework/Versions/A/Headers/AvailabilityVersions.h)
        message(STATUS "Symlinked AvailabilityVersions.h into Kernel.framework")
    endif()
]] COMPONENT BaseSystem)
