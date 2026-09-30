if(CMAKE_HOST_APPLE AND NOT DEFINED ENV{NIX_UNIFDEF_PATH})
    add_executable(unifdef)
    target_sources(unifdef PRIVATE unifdef.c)
elseif(DEFINED ENV{NIX_UNIFDEF_PATH})
    # Prebuilt by the standalone unifdef.nix Nix derivation (see
    # build.nix) instead of built in-tree here, so an edit anywhere else
    # in the project doesn't force rebuilding this trivial one-file tool.
    add_executable(unifdef IMPORTED GLOBAL)
    set_property(TARGET unifdef PROPERTY IMPORTED_LOCATION "$ENV{NIX_UNIFDEF_PATH}")
else()
    # Same Canadian-cross gap as tools/mig/migcom (see that CMakeLists for
    # the full explanation): plain add_executable() here would inherit the
    # project's global cross-compiler and produce a Mach-O binary that
    # can't run as a build-time tool on this Linux host. unifdef.c only
    # uses plain POSIX headers (sys/types.h, stdio.h, ...) - no Mach
    # types - so unlike migcom this needs no SDK header shim at all, just
    # a real native compile.
    find_program(UNIFDEF_NATIVE_CC NAMES cc gcc clang REQUIRED)
    set(_unifdef_bin "${CMAKE_CURRENT_BINARY_DIR}/unifdef_native/unifdef.native")
    add_custom_command(OUTPUT "${_unifdef_bin}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/unifdef_native"
        COMMAND "${UNIFDEF_NATIVE_CC}" -std=gnu89 -O2
                "${CMAKE_CURRENT_SOURCE_DIR}/unifdef.c" -o "${_unifdef_bin}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/unifdef.c"
        COMMENT "Building native unifdef"
        VERBATIM
    )
    add_custom_target(unifdef_build ALL DEPENDS "${_unifdef_bin}")
    add_executable(unifdef IMPORTED GLOBAL)
    set_property(TARGET unifdef PROPERTY IMPORTED_LOCATION "${_unifdef_bin}")
    add_dependencies(unifdef unifdef_build)
endif()
