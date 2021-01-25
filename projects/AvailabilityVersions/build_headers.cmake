file(MAKE_DIRECTORY ${HEADER_DIR}/dyld)

execute_process(
    COMMAND ${SOURCE_DIR}/build_version_map.rb ${SOURCE_DIR}/availability.pl
    OUTPUT_FILE ${HEADER_DIR}/dyld/VersionMap.h
)

execute_process(
    COMMAND ${SOURCE_DIR}/print_dyld_os_versions.rb ${SOURCE_DIR}/availability.pl
    OUTPUT_FILE ${HEADER_DIR}/dyld/for_dyld_priv.inc
)
