add_subdirectory(Libraries)

if(KERNEL_BUILD_XNU)
    add_subdirectory(Kernel_XNU)
endif()
