install_xnu_headers(TARGET_NAME xnu.headers.external DESTINATION kernel FILES
    stdarg.h stdatomic.h stdbool.h stddef.h stdint.h ptrauth.h
    Availability.h AvailabilityInternal.h AvailabilityMacros.h
)

install_xnu_headers(TARGET_NAME xnu.headers.external DESTINATION kernel SUBDIRECTORY sys/_pthread FILES sys/_pthread/_pthread_types.h)

install_xnu_headers(TARGET_NAME xnu.headers.external DESTINATION kernel SUBDIRECTORY mach-o FILES
    mach-o/fat.h mach-o/fixup-chains.h mach-o/loader.h mach-o/nlist.h mach-o/reloc.h
)

install_xnu_headers(TARGET_NAME xnu.headers.external DESTINATION kernel SUBDIRECTORY architecture FILES
    architecture/byte_order.h
)

install_xnu_headers(TARGET_NAME xnu.headers.external DESTINATION kernel SUBDIRECTORY architecture/i386 FILES
    architecture/i386/asm_help.h architecture/i386/cpu.h architecture/i386/io.h architecture/i386/pio.h
    architecture/i386/sel.h architecture/i386/tss.h architecture/i386/desc.h architecture/i386/reg_help.h
    architecture/i386/table.h
)

add_library(xnu.extheaders INTERFACE)
target_include_directories(xnu.extheaders INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
