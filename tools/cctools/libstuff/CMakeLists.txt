add_library(host_libstuff STATIC)
target_sources(host_libstuff PRIVATE
    allocate.c
    apple_version.c
    arch_usage.c
    arch.c
    args.c
    best_arch.c
    breakout.c
    bytesex.c
    checkout.c
    coff_bytesex.c
    crc32.c
    dylib_table.c
    dylib_roots.c
    errors.c
    execute.c
    fatal_arch.c
    fatals.c
    get_arch_from_host.c
    get_toc_byte_sex.c
    guess_short_name.c
    hash_string.c
    hppa.c
    llvm.c
    lto.c
    macosx_deployment_target.c
    ofile_error.c
    ofile_get_word.c
    ofile.c
    port.c
    print.c
    reloc.c
    rnd.c
    seg_addr_table.c
    set_arch_flag_name.c
    swap_headers.c
    symbol_list.c
    SymLoc.c
    unix_standard_mode.c
    version_number.c
    vm_flush_cache.c
    write64.c
    writeout.c
)

set_property(TARGET host_libstuff PROPERTY OUTPUT_NAME stuff)
target_compile_definitions(host_libstuff PRIVATE
    __DARWIN_UNIX03
    PROGRAM_PREFIX=\"\"
)
target_include_directories(host_libstuff PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/foreign
)

# Real x86_64-apple-darwin build, for self-hosted cctools (see libmacho's
# CMakeLists.txt for the rationale).
if(PUREDARWIN_ENABLE_SELFHOST_CCTOOLS)
    add_darwin_static_library(libstuff_static)
    target_sources(libstuff_static PRIVATE
        allocate.c
        apple_version.c
        arch_usage.c
        arch.c
        args.c
        best_arch.c
        breakout.c
        bytesex.c
        checkout.c
        coff_bytesex.c
        crc32.c
        dylib_table.c
        dylib_roots.c
        errors.c
        execute.c
        fatal_arch.c
        fatals.c
        get_arch_from_host.c
        get_toc_byte_sex.c
        guess_short_name.c
        hash_string.c
        hppa.c
        llvm.c
        lto.c
        macosx_deployment_target.c
        ofile_error.c
        ofile_get_word.c
        ofile.c
        port.c
        print.c
        reloc.c
        rnd.c
        seg_addr_table.c
        set_arch_flag_name.c
        swap_headers.c
        symbol_list.c
        SymLoc.c
        unix_standard_mode.c
        version_number.c
        vm_flush_cache.c
        write64.c
        writeout.c
    )
    set_property(TARGET libstuff_static PROPERTY OUTPUT_NAME stuff_selfhost)
    target_compile_definitions(libstuff_static PRIVATE
        __DARWIN_UNIX03=1
        _DARWIN_USE_64_BIT_INODE=1
        PROGRAM_PREFIX=\"\"
    )
    target_include_directories(libstuff_static PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/foreign
    )
    target_compile_options(libstuff_static PRIVATE -isysroot ${PUREDARWIN_MACOSX_SDK})
    # add_darwin_static_library() adds -nostdinc, but several sources here
    # (ofile.c etc.) reach the real SDK headers via the foreign/ shim's
    # #include_next <stdio.h> etc. - strip it so that chain resolves.
    get_target_property(_selfhost_libstuff_opts libstuff_static COMPILE_OPTIONS)
    if(_selfhost_libstuff_opts)
        list(REMOVE_ITEM _selfhost_libstuff_opts -nostdinc)
        set_target_properties(libstuff_static PROPERTIES COMPILE_OPTIONS "${_selfhost_libstuff_opts}")
    endif()
endif()
