find_package(LibClang REQUIRED)

add_executable(host_iig)
target_sources(host_iig PRIVATE iig.cpp)
target_include_directories(host_iig PRIVATE ${LIBCLANG_INCLUDE_DIR})
target_link_libraries(host_iig PRIVATE ${LIBCLANG_LIBRARIES})
set_property(TARGET host_iig PROPERTY CXX_STANDARD 14)
