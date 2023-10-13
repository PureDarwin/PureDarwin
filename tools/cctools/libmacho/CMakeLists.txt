add_library(libmacho STATIC)
target_sources(libmacho PRIVATE
    arch.c
)
set_property(TARGET libmacho PROPERTY OUTPUT_NAME macho)
target_include_directories(libmacho PRIVATE ../include ../include/foreign)
