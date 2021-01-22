execute_process(
    COMMAND ./build_version_map.rb availability.pl
    OUTPUT_FILE $ENV{OUT_DIR}/dyld/VersionMap.h
)

execute_process(
    COMMAND ./print_dyld_os_versions.rb availability.pl
    OUTPUT_FILE $ENV{OUT_DIR}/dyld/for_dyld_priv.inc
)
