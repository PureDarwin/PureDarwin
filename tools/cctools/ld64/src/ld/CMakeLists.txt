add_subdirectory(parsers)
add_subdirectory(passes)

find_package(OpenSSL REQUIRED)

add_executable(host_ld)
target_sources(host_ld PRIVATE
    debugline.c
    InputFiles.cpp
    ld.cpp
    libcodedirectory.c
    Options.cpp
    OutputFile.cpp
    Resolver.cpp
    ResponseFiles.cpp
    Snapshot.cpp
    SymbolTable.cpp
    PlatformSupport.cpp
    code-sign-blobs/blob.cpp
)

target_compile_definitions(host_ld PRIVATE __DARWIN_UNIX03)
target_compile_options(host_ld PRIVATE "-Wno-#warnings" -Wno-deprecated)
target_link_libraries(host_ld PRIVATE
    host_libxar host_ld_helper host_ld_BlocksRuntime host_ld_libparsers host_ld_libpasses host_libstuff
    OpenSSL::SSL OpenSSL::Crypto
    host_commoncrypto_static
    host_commoncrypto_private_headers
    host_commoncrypto_headers
    host_corecrypto_headers
)
set_property(TARGET host_ld PROPERTY OUTPUT_NAME ld)
set_property(TARGET host_ld PROPERTY CXX_STANDARD 11)
target_include_directories(host_ld PRIVATE
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
