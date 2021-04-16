include(./functions.cmake)
add_subdirectory(SETUP)

add_library(xnu_headers INTERFACE)
target_include_directories(xnu_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/user)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/user)

add_library(xnu_private_headers INTERFACE)
target_include_directories(xnu_private_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/user_private)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/user_private)

add_library(xnu_kernel_headers INTERFACE)
target_include_directories(xnu_kernel_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/kernel)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/kernel)

add_library(xnu_kernel_private_headers INTERFACE)
target_include_directories(xnu_kernel_private_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/include/kernel_private)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/kernel_private)

add_darwin_executable(xnu)
set_property(TARGET xnu PROPERTY OUTPUT_NAME kernel)

add_subdirectory(bsd)
add_subdirectory(libkern)
add_subdirectory(osfmk)
add_subdirectory(EXTERNAL_HEADERS)
