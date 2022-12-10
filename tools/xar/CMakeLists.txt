# xar - XAR Archive tool. Build static library + link with final file to create
#   executable image for the host.

# Host libraries we need to link with
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

# BEGIN STATICLIB LIBXAR

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

target_link_libraries(libxar PUBLIC OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB LibXml2::LibXml2)

# External host libraries we link we
target_include_directories(libxar PRIVATE ${OPENSSL_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR})

# Our header directory
target_include_directories(libxar PUBLIC include)

# END STATICLIB LIBXAR

# BEGIN EXECUTABLE XAR

add_executable(xar)

target_sources(xar PRIVATE
    src/xar.c
)

target_link_libraries(xar PRIVATE libxar)

install(TARGETS xar DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE XAR
