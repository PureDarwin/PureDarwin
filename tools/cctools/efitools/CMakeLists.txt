add_executable(host_mtoc mtoc.c)
set_property(TARGET host_mtoc PROPERTY OUTPUT_NAME mtoc)
target_link_libraries(host_mtoc PRIVATE host_libmacho host_libstuff)
target_include_directories(host_mtoc PRIVATE ../include ../include/foreign ../libstuff)
