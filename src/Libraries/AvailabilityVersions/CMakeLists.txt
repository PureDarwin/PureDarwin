# AvailabilityVersions-157.2 plus a PureDarwin extension to availability.dsl
# carrying macOS through 26.5 (see the comment at the end of that file). The
# headers in include/ are generated output, checked in rather than built:
#
#   ./availability.pl --preprocess ./availability.pl obj/availability
#   for t in templates/*; do obj/availability --preprocess $t include/$(basename $t); done
#
# os_availability.h installs as os/availability.h, and VersionMap.h /
# for_dyld_priv.inc live under include/dyld for dyld3.
add_executable(availability.pl IMPORTED GLOBAL)
set_property(TARGET availability.pl PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/availability.pl)

add_library(AvailabilityHeaders INTERFACE)
target_include_directories(AvailabilityHeaders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_darwin_object_library(dyld_availability_headers)
target_include_directories(dyld_availability_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_sources(dyld_availability_headers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/dyld/VersionMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/dyld/for_dyld_priv.inc
)

install(FILES include/Availability.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES include/AvailabilityInternal.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES include/AvailabilityInternalLegacy.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES include/AvailabilityMacros.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES include/AvailabilityVersions.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES include/ConditionalMacros.h DESTINATION usr/include COMPONENT DeveloperTools)
install(FILES include/TargetConditionals.h DESTINATION usr/include COMPONENT DeveloperTools)

install(FILES include/os/availability.h DESTINATION usr/include/os COMPONENT DeveloperTools)

# The install scripts from Apple keep these around, stripping out the local/ directory later for release builds.
install(FILES include/AvailabilityInternalPrivate.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
install(FILES include/AvailabilityProhibitedInternal.h DESTINATION usr/local/include COMPONENT DeveloperInternal)
install(FILES include/dyld/VersionMap.h DESTINATION usr/local/include/dyld COMPONENT DeveloperInternal)
install(FILES include/dyld/for_dyld_priv.inc DESTINATION usr/local/include/dyld COMPONENT DeveloperInternal)
install(FILES include/mach-o/dyld_version_defines.h DESTINATION usr/local/include/mach-o COMPONENT DeveloperInternal)

install(FILES availability.pl DESTINATION usr/local/libexec COMPONENT DeveloperInternal)

# Technically, I'm pretty sure this should be installed by XNU
install(DIRECTORY DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A/Headers COMPONENT DeveloperTools)

install_symlink(
    System/Library/Frameworks/Kernel.framework/Versions/A/Headers/AvailabilityVersions.h
    ../../../../../../../usr/include/AvailabilityVersions.h
    COMPONENT DeveloperTools
)
