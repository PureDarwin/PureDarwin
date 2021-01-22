project(dtrace_ctf_host)

add_library(libelf.host STATIC)
target_sources(libelf.host PRIVATE
    libelf/begin.c
    libelf/clscook.c
    libelf/clscook_ELF64.c
    libelf/cntl.c
    libelf/cook.c
    libelf/data.c
    libelf/end.c
    libelf/error.c
    libelf/gelf.c
    libelf/getdata.c
    libelf/getehdr.c
    libelf/getident.c
    libelf/getscn.c
    libelf/getshdr.c
    libelf/getshstrndx.c
    libelf/input.c
    libelf/kind.c
    libelf/ndxscn.c
    libelf/nextscn.c
    libelf/strptr.c
    libelf/xlate.c
    libelf/xlate64.c

    libelf/decl.h
    libelf/gelf.h
    libelf/machelf.h
    libelf/msg.h
)
set_property(TARGET libelf.host PROPERTY PREFIX "")
target_include_directories(libelf.host PRIVATE include include/sys)

add_library(libdwarf.host STATIC)
target_sources(libdwarf.host PRIVATE
    libdwarf/dwarf_abbrev.c
    libdwarf/dwarf_addr_finder.c
    libdwarf/dwarf_alloc.c
    libdwarf/dwarf_arange.c
    libdwarf/dwarf_die_deliv.c
    libdwarf/dwarf_error.c
    libdwarf/dwarf_form.c
    libdwarf/dwarf_frame.c
    libdwarf/dwarf_frame2.c
    libdwarf/dwarf_frame3.c
    libdwarf/dwarf_funcs.c
    libdwarf/dwarf_global.c
    libdwarf/dwarf_init_finish.c
    libdwarf/dwarf_leb.c
    libdwarf/dwarf_line.c
    libdwarf/dwarf_line2.c
    libdwarf/dwarf_loc.c
    libdwarf/dwarf_macro.c
    libdwarf/dwarf_print_lines.c
    libdwarf/dwarf_pubtypes.c
    libdwarf/dwarf_query.c
    libdwarf/dwarf_sort_line.c
    libdwarf/dwarf_string.c
    libdwarf/dwarf_types.c
    libdwarf/dwarf_util.c
    libdwarf/dwarf_vars.c
    libdwarf/dwarf_weaks.c
)
set_property(TARGET libdwarf.host PROPERTY PREFIX "")
target_include_directories(libdwarf.host PRIVATE include)
