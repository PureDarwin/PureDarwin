add_host_tool(otool)
target_sources(otool PRIVATE
    main.c
    ofile_print.c
    m68k_disasm.c
    m88k_disasm.c
    i386_disasm.c
    i860_disasm.c
    ppc_disasm.c
    hppa_disasm.c
    sparc_disasm.c
    arm_disasm.c
    arm64_disasm.c
    print_objc.c
    print_objc2_32bit.c
    print_objc2_64bit.c
    print_bitcode.c
    coff_print.c
    dyld_bind_info.c

    ../libstuff/ofile.c
)
target_include_directories(otool PRIVATE ../include ../include/foreign)
target_link_libraries(otool PRIVATE libxar libstuff -lc++)

if(APPLE)
    target_sources(otool PRIVATE
        print_objc.c
        print_objc2_32bit.c
        print_objc2_64bit.c
    )

    target_compile_definitions(otool PRIVATE USE_LIBOBJC)
    target_link_libraries(otool PRIVATE -lobjc)
endif()
