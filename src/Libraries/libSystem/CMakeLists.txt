# user-space corecrypto comes from the external corecrypto tree, src/Libraries/libSystem/corecrypto
# now only serves Security.framework's private copy
if(PD_CORECRYPTO_DIR)
    include(${PD_CORECRYPTO_DIR}/pd/libsystem.cmake)
endif()
foreach(_puredarwin_libsystem_dir
    libsystem_kernel
    libc
    libplatform
    libmalloc
    pthread
    libdispatch
    libdyld
    stub
)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_puredarwin_libsystem_dir}/CMakeLists.txt")
        add_subdirectory("${_puredarwin_libsystem_dir}")
    endif()
endforeach()
