file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/gssd)
process_mig_source(gssd_mach.defs
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMPILE_DEFINITIONS MACH_KERNEL_PRIVATE KERNEL_USER KERNEL
    CLIENT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/gssd/gssd_mach.c
    CLIENT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/gssd/gssd_mach.h
)

add_darwin_object_library(xnu.osfmk_gssd_module)
target_include_directories(xnu.osfmk_gssd_module PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_sources(xnu.osfmk_gssd_module PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/gssd/gssd_mach.c)
target_link_libraries(xnu.osfmk_gssd_module PRIVATE xnu_kernel_headers)
