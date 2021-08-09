add_subdirectory(Libraries)

if(KERNEL_BUILD_XNU)
    add_subdirectry(Kernel_XNU)
endif()