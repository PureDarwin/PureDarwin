# Stub for now, will port libpthread to cmake later.
add_library(pthread_common_headers INTERFACE)
target_include_directories(pthread_common_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_library(pthread_common_private_headers INTERFACE)
target_include_directories(pthread_common_private_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/private)

add_subdirectory(kern)
add_subdirectory(src)
