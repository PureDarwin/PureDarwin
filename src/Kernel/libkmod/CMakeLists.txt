add_darwin_static_library(libkmod)
set_property(TARGET libkmod PROPERTY OUTPUT_NAME "kmod")
target_sources(libkmod PRIVATE c_start.c c_stop.c)
target_link_libraries(libkmod PRIVATE xnu_kernel_headers AvailabilityHeaders)
target_compile_definitions(libkmod PRIVATE TARGET_OS_OSX)

# cplus_start.c and cplus_stop.c are empty, thus no libkmodc++.
