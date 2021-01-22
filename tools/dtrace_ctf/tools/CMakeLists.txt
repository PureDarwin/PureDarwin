find_package(ZLIB REQUIRED)

add_executable(ctfconvert.host)
target_sources(ctfconvert.host PRIVATE
    alist.cpp
    array.c
    atom.cpp
    barrier.c
    ctf.c
    ctfconvert.c
    darwin_shim.c
    dwarf.c
    fifo.c
    hash.c
    iidesc.c
    input.c
    list.c
    memory.c
    merge.c
    output.c
    stack.c
    strtab.cpp
    symbol.c
    tdata.c
    traverse.c
    util.c
)

target_include_directories(ctfconvert.host PRIVATE ../include ../include/sys ../libelf)
target_link_libraries(ctfconvert.host PRIVATE libelf.host libdwarf.host libctf.host ${ZLIB_LIBRARY_RELEASE})
set_property(TARGET ctfconvert.host PROPERTY CXX_STANDARD 14)

add_executable(ctfdump.host)
target_sources(ctfdump.host PRIVATE
    alist.cpp
    array.c
    atom.cpp
    ctf.c
    darwin_shim.c
    dump.c
    fifo.c
    hash.c
    iidesc.c
    input.c
    list.c
    memory.c
    output.c
    stack.c
    strtab.cpp
    symbol.c
    tdata.c
    traverse.c
    util.c
    utils.c
)

target_include_directories(ctfdump.host PRIVATE ../include ../include/sys ../libelf)
target_link_libraries(ctfdump.host PRIVATE libelf.host libctf.host ${ZLIB_LIBRARY_RELEASE})
set_property(TARGET ctfdump.host PROPERTY CXX_STANDARD 14)
