project(dtrace_ctf_host)

add_compile_options(-Wno-implicit-function-declaration -Wno-int-conversion)
include_directories(include/cctools include/cctools/foreign)

add_subdirectory(libctf)
add_subdirectory(libdwarf)
add_subdirectory(libelf)
add_subdirectory(tools)
