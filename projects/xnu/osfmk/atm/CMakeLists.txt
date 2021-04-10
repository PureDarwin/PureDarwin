process_mig_source(atm_notification.defs
    INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/..
    COMPILE_DEFINITIONS MACH_KERNEL_PRIVATE KERNEL_USER=1
    CLIENT_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/atm_notification_user.c
    CLIENT_HEADER ${CMAKE_CURRENT_BINARY_DIR}/atm_notification.h
)

add_darwin_object_library(xnu.osfmk_atm_module)
target_sources(xnu.osfmk_atm_module PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/atm_notification_user.c)
target_link_libraries(xnu.osfmk_atm_module PRIVATE xnu_kernel_headers pthread_kernel_headers)
target_include_directories(xnu.osfmk_atm_module PRIVATE
    ${XNU_SOURCE_DIR}/bsd ${XNU_SOURCE_DIR}/osfmk ${XNU_SOURCE_DIR}/osfmk/libsa
    ${XNU_SOURCE_DIR}/libkern
)
