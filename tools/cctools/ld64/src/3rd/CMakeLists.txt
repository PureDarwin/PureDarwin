add_library(host_ld_helper STATIC)
target_sources(host_ld_helper PRIVATE
    helper.c
    qsort_r.c
    strlcat.c
    strlcpy.c
    eprintf.c
    md5.c
    mkpath_np.c
    find_executable.c
)
set_property(TARGET host_ld_helper PROPERTY OUTPUT_NAME ld_helper)
target_include_directories(host_ld_helper PRIVATE include ../../../include ../../../include/foreign)
target_compile_definitions(host_ld_helper PRIVATE -D__DARWIN_UNIX03)

add_library(host_ld_BlocksRuntime STATIC)
set_property(TARGET host_ld_BlocksRuntime PROPERTY OUTPUT_NAME ld_BlocksRuntime)
target_sources(host_ld_BlocksRuntime PRIVATE
    BlocksRuntime/data.c
    BlocksRuntime/runtime.c
)
