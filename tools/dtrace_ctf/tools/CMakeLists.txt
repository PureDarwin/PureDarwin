find_package(ZLIB REQUIRED)

add_host_tool(ctfconvert)
target_sources(ctfconvert PRIVATE
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

target_include_directories(ctfconvert PRIVATE ../include ../include/sys ../libelf)
target_link_libraries(ctfconvert PRIVATE libelf.host libdwarf.host libctf.host ${ZLIB_LIBRARY_RELEASE})
set_property(TARGET ctfconvert PROPERTY CXX_STANDARD 14)

add_host_tool(ctfdump)
target_sources(ctfdump PRIVATE
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

target_include_directories(ctfdump PRIVATE ../include ../include/sys ../libelf)
target_link_libraries(ctfdump PRIVATE libelf.host libctf.host ${ZLIB_LIBRARY_RELEASE})
set_property(TARGET ctfdump PROPERTY CXX_STANDARD 14)

add_host_tool(ctfmerge)
target_sources(ctfmerge PRIVATE
    alist.cpp
    array.c
    atom.cpp
    barrier.c
    ctf.c
    ctfmerge.c
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
    utils.c
)

target_include_directories(ctfmerge PRIVATE ../include ../include/sys ../libelf)
target_link_libraries(ctfmerge PRIVATE libelf.host libdwarf.host libctf.host ${ZLIB_LIBRARY_RELEASE})
set_property(TARGET ctfmerge PROPERTY CXX_STANDARD 14)
