if(NOT CMAKE_HOST_APPLE)
    add_compile_options(-fblocks)
endif()

add_subdirectory(3rd)
add_subdirectory(ld)
