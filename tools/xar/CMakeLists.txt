find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

add_library(libxar STATIC)
target_sources(libxar PRIVATE
    lib/archive.c
    lib/arcmod.c
    lib/b64.c
    lib/bzxar.c
    lib/darwinattr.c
    lib/data.c
    lib/ea.c
    lib/err.c
    lib/ext2.c
    lib/fbsdattr.c
    lib/filetree.c
    lib/hash.c
    lib/io.c
    lib/linuxattr.c
    lib/lzmaxar.c
    lib/macho.c
    lib/script.c
    lib/signature.c
    lib/stat.c
    lib/subdoc.c
    lib/util.c
    lib/zxar.c
)
set_property(TARGET libxar PROPERTY OUTPUT_NAME xar)
target_include_directories(libxar PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(libxar PRIVATE ${OPENSSL_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR})
target_link_libraries(libxar PRIVATE ZLIB::ZLIB LibXml2::LibXml2)

add_host_tool(xar)
target_sources(xar PRIVATE
    src/xar.c
)
target_link_libraries(xar PRIVATE libxar OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB LibXml2::LibXml2)
