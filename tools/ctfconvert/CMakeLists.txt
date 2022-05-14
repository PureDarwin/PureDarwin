# ctf{convert,dump,merge} tools. These tools share 3 static libraries, and
#   nearly the same ~20 source files. Just compile + link everything.

# We need to link with zlib
find_package(ZLIB REQUIRED)

# Shared source files for all ctf tools
set(CTF_SOURCES
    dtrace/tools/ctfconvert/alist.cpp
    dtrace/tools/ctfconvert/array.c
    dtrace/tools/ctfconvert/atom.cpp
    dtrace/tools/ctfconvert/ctf.c
    dtrace/tools/ctfconvert/fifo.c
    dtrace/tools/ctfconvert/hash.c
    dtrace/tools/ctfconvert/iidesc.c
    dtrace/tools/ctfconvert/input.c
    dtrace/tools/ctfconvert/list.c
    dtrace/tools/ctfconvert/memory.c
    dtrace/tools/ctfconvert/output.c
    dtrace/tools/ctfconvert/stack.c
    dtrace/tools/ctfconvert/strtab.cpp
    dtrace/tools/ctfconvert/symbol.c
    dtrace/tools/ctfconvert/tdata.c
    dtrace/tools/ctfconvert/traverse.c
    dtrace/tools/ctfconvert/util.c

    dtrace/compat/opensolaris/darwin_shim.c
)

# Compiler flags for all targets
set(CTF_CFLAGS
    -Wno-pedantic
    -Wno-unknown-warning-option
    -Wno-bad-function-cast
    -Wno-c++-compat
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-cast-align
    -Wno-cast-qual
    -Wno-disabled-macro-expansion
    -Wno-documentation-unknown-command
    -Wno-format-nonliteral
    -Wno-missing-variable-declarations
    -Wno-old-style-cast
    -Wno-padded
    -Wno-reserved-id-macro
    -Wno-shift-sign-overflow
    -Wno-undef
    -Wno-unreachable-code-aggressive
    -Wno-unused-macros
    -Wno-used-but-marked-unused
    -Wno-vla
)

# BEGIN STATIC LIBRARY LIBCTF

add_library(libctf STATIC)

target_sources(libctf PRIVATE
    dtrace/lib/libctf/common/ctf_create.c
    dtrace/lib/libctf/common/ctf_decl.c
    dtrace/lib/libctf/common/ctf_error.c
    dtrace/lib/libctf/common/ctf_hash.c
    dtrace/lib/libctf/common/ctf_labels.c
    dtrace/lib/libctf/common/ctf_lib.c
    dtrace/lib/libctf/common/ctf_lookup.c
    dtrace/lib/libctf/common/ctf_open.c
    dtrace/lib/libctf/common/ctf_subr.c
    dtrace/lib/libctf/common/ctf_types.c
    dtrace/lib/libctf/common/ctf_util.c

    dtrace/lib/libctf/common/ctf_impl.h
)

target_include_directories(libctf PUBLIC dtrace/lib/libctf/common)
target_include_directories(libctf PUBLIC include include/sys)

target_compile_definitions(libctf PRIVATE CTF_OLD_VERSIONS)
target_compile_options(libctf PRIVATE ${CTF_CFLAGS})

# END STATIC LIBRARY LIBCTF

# BEGIN STATIC LIBRARY LIBDWARF

add_library(libdwarf STATIC)

target_sources(libdwarf PRIVATE
    dtrace/lib/libdwarf/dwarf_abbrev.c
    dtrace/lib/libdwarf/dwarf_addr_finder.c
    dtrace/lib/libdwarf/dwarf_alloc.c
    dtrace/lib/libdwarf/dwarf_arange.c
    dtrace/lib/libdwarf/dwarf_die_deliv.c
    dtrace/lib/libdwarf/dwarf_error.c
    dtrace/lib/libdwarf/dwarf_form.c
    dtrace/lib/libdwarf/dwarf_frame.c
    dtrace/lib/libdwarf/dwarf_frame2.c
    dtrace/lib/libdwarf/dwarf_frame3.c
    dtrace/lib/libdwarf/dwarf_funcs.c
    dtrace/lib/libdwarf/dwarf_global.c
    dtrace/lib/libdwarf/dwarf_init_finish.c
    dtrace/lib/libdwarf/dwarf_leb.c
    dtrace/lib/libdwarf/dwarf_line.c
    dtrace/lib/libdwarf/dwarf_line2.c
    dtrace/lib/libdwarf/dwarf_loc.c
    dtrace/lib/libdwarf/dwarf_macro.c
    dtrace/lib/libdwarf/dwarf_print_lines.c
    dtrace/lib/libdwarf/dwarf_pubtypes.c
    dtrace/lib/libdwarf/dwarf_query.c
    dtrace/lib/libdwarf/dwarf_sort_line.c
    dtrace/lib/libdwarf/dwarf_string.c
    dtrace/lib/libdwarf/dwarf_types.c
    dtrace/lib/libdwarf/dwarf_util.c
    dtrace/lib/libdwarf/dwarf_vars.c
    dtrace/lib/libdwarf/dwarf_weaks.c
)

target_include_directories(libdwarf PUBLIC dtrace/lib/libdwarf dtrace/lib/libdwarf/cmplrs)
target_include_directories(libdwarf PUBLIC include include/sys)

target_compile_definitions(libdwarf PRIVATE DWARF_SIMPLE_MALLOC)
target_compile_options(libdwarf PRIVATE ${CTF_CFLAGS})

# END STATIC LIBRARY LIBDWARF

# BEGIN STATIC LIBRARY LIBELF

add_library(libelf STATIC)

target_sources(libelf PRIVATE
    dtrace/lib/libelf/begin.c
    dtrace/lib/libelf/clscook.c
    dtrace/lib/libelf/clscook_ELF64.c
    dtrace/lib/libelf/cntl.c
    dtrace/lib/libelf/cook.c
    dtrace/lib/libelf/data.c
    dtrace/lib/libelf/end.c
    dtrace/lib/libelf/error.c
    dtrace/lib/libelf/gelf.c
    dtrace/lib/libelf/getdata.c
    dtrace/lib/libelf/getehdr.c
    dtrace/lib/libelf/getident.c
    dtrace/lib/libelf/getscn.c
    dtrace/lib/libelf/getshdr.c
    dtrace/lib/libelf/getshstrndx.c
    dtrace/lib/libelf/input.c
    dtrace/lib/libelf/kind.c
    dtrace/lib/libelf/ndxscn.c
    dtrace/lib/libelf/nextscn.c
    dtrace/lib/libelf/strptr.c
    dtrace/lib/libelf/xlate.c
    dtrace/lib/libelf/xlate64.c
)

target_include_directories(libelf PUBLIC include include/sys)
target_include_directories(libelf PUBLIC dtrace/lib/libelf)

target_compile_definitions(libelf PRIVATE DWARF_SIMPLE_MALLOC)
target_compile_options(libelf PRIVATE ${CTF_CFLAGS})

# END STATIC LIBRARY LIBELF

# BEGIN EXECUTABLE CTFCONVERT

add_executable(ctfconvert)

target_sources(ctfconvert PRIVATE
    ${CTF_SOURCES}
    dtrace/tools/ctfconvert/barrier.c
    dtrace/tools/ctfconvert/ctfconvert.c
    dtrace/tools/ctfconvert/dwarf.c
    dtrace/tools/ctfconvert/merge.c
)

target_link_libraries(ctfconvert PRIVATE libctf libdwarf libelf ZLIB::ZLIB)

target_compile_definitions(ctfconvert PRIVATE DWARF_SIMPLE_MALLOC)
target_compile_options(ctfconvert PRIVATE ${CTF_CFLAGS})

install(TARGETS ctfconvert DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE CTFCONVERT

# BEGIN EXECUTABLE CTFDUMP

add_executable(ctfdump)

target_sources(ctfdump PRIVATE
    ${CTF_SOURCES}
    dtrace/tools/ctfconvert/dump.c
    dtrace/tools/ctfconvert/utils.c
)

target_link_libraries(ctfdump PRIVATE libctf libelf ZLIB::ZLIB)

target_compile_definitions(ctfdump PRIVATE DWARF_SIMPLE_MALLOC)
target_compile_options(ctfdump PRIVATE ${CTF_CFLAGS})

install(TARGETS ctfdump DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE CTFDUMP

# BEGIN EXECUTABLE CTFMERGE

add_executable(ctfmerge)

target_sources(ctfmerge PRIVATE
    ${CTF_SOURCES}
    dtrace/tools/ctfconvert/barrier.c
    dtrace/tools/ctfconvert/ctfmerge.c
    dtrace/tools/ctfconvert/dwarf.c
    dtrace/tools/ctfconvert/merge.c
    dtrace/tools/ctfconvert/utils.c
)

target_link_libraries(ctfmerge PRIVATE libctf libdwarf libelf ZLIB::ZLIB)

target_compile_definitions(ctfmerge PRIVATE DWARF_SIMPLE_MALLOC)
target_compile_options(ctfmerge PRIVATE ${CTF_CFLAGS})

install(TARGETS ctfmerge DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE CTFMERGE
