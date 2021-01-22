add_library(libelf STATIC)
target_sources(libelf PRIVATE
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
    libelf/libelf.h
    libelf/machelf.h
    libelf/msg.h
)
set_property(TARGET libelf PROPERTY PREFIX "")
target_include_directories(libelf PRIVATE include include/sys)
