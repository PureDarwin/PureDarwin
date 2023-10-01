find_package(LLVM REQUIRED)

add_executable(host_iig)
target_sources(host_iig PRIVATE iig.cpp)
target_link_libraries(host_iig PRIVATE libclang_imported)
set_property(TARGET host_iig PROPERTY CXX_STANDARD 14)
