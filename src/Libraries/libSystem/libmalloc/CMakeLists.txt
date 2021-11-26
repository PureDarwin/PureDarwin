# Currently, just expose headers under include/

add_library(libmalloc_headers INTERFACE)

target_include_directories(libmalloc_headers INTERFACE include)
