add_library(host_libmacho STATIC)
target_sources(host_libmacho PRIVATE
    arch.c
)
target_include_directories(host_libmacho PRIVATE ../include ../include/foreign)
