add_subdirectory(parsers)
add_subdirectory(passes)

find_package(OpenSSL REQUIRED)

add_host_tool(ld)
target_sources(ld PRIVATE
    debugline.c
    InputFiles.cpp
    ld.cpp
    Options.cpp
    OutputFile.cpp
    Resolver.cpp
    Snapshot.cpp
    SymbolTable.cpp
    PlatformSupport.cpp
    code-sign-blobs/blob.cpp
)

target_compile_definitions(ld PRIVATE __DARWIN_UNIX03)
target_compile_options(ld PRIVATE "-Wno-#warnings" -Wno-deprecated)
target_link_libraries(ld PRIVATE
    libxar ld_helper ld_BlocksRuntime ld_libparsers ld_libpasses libstuff
    OpenSSL::SSL OpenSSL::Crypto
)
set_property(TARGET ld PROPERTY OUTPUT_NAME ld)
set_property(TARGET ld PROPERTY CXX_STANDARD 11)
target_include_directories(ld PRIVATE
    ../../../include
    ../../../include/foreign
    ..
    ../..
    ../abstraction
    ../3rd
    ../3rd/BlocksRuntime
    ../3rd/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    parsers passes
    ${OPENSSL_INCLUDE_DIR}
)
