find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibXml2 REQUIRED)

add_library(host_libxar STATIC)
target_sources(host_libxar PRIVATE
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
set_property(TARGET host_libxar PROPERTY OUTPUT_NAME xar)
target_include_directories(host_libxar PUBLIC include)
target_include_directories(host_libxar PRIVATE ${OPENSSL_INCLUDE_DIR} ${LIBXML2_INCLUDE_DIR})
target_link_libraries(host_libxar PRIVATE ZLIB::ZLIB LibXml2::LibXml2)
suppress_all_warnings(host_libxar)

add_executable(host_xar)
target_sources(host_xar PRIVATE
    src/xar.c
)
set_property(TARGET host_xar PROPERTY OUTPUT_NAME xar)
target_link_libraries(host_xar PRIVATE host_libxar OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB LibXml2::LibXml2)

include(CheckIncludeFile)
include(CheckStructHasMember)
include(CheckSymbolExists)

function(define_check_include_file file macro)
    check_include_file(${file} ${macro})
    if (${macro} EQUAL 1)
        add_compile_definitions(${macro})
    endif()
endfunction()

function(define_check_struct_has_member struct member header macro)
    check_struct_has_member(${struct} ${member} ${header} ${macro})
    if(${macro} EQUAL 1)
        add_compile_definitions(${macro})
    endif()
endfunction()

function(define_check_symbol_exists func files macro)
    check_symbol_exists(${func} ${files} ${macro})
    if (${macro} EQUAL 1)
        add_compile_definitions(${macro})
    endif()
endfunction()

define_check_include_file("ext2fs/ext2_fs.h" HAVE_EXT2FS_EXT2_FS_H)
define_check_include_file("libutil.h" HAVE_LIBUTIL_H)
define_check_include_file("sys/acl.h" HAVE_SYS_ACL_H)
define_check_include_file("sys/extattr.h" HAVE_SYS_EXTATTR_H)
define_check_include_file("sys/param.h" HAVE_SYS_PARAM_H)
define_check_include_file("sys/statfs.h" HAVE_SYS_STATFS_H)
define_check_include_file("sys/xattr.h" HAVE_SYS_XATTR_H)
define_check_struct_has_member("struct stat" st_flags sys/stat.h HAVE_STRUCT_STAT_ST_FLAGS LANGUAGE C)
define_check_struct_has_member("struct statfs" f_typename sys/statfs.h HAVE_STRUCT_STATFS_F_FSTYPENAME LANGUAGE C)
define_check_struct_has_member("struct statvfs" f_typename sys/statvfs.h HAVE_STRUCT_STATVFS_F_FSTYPENAME LANGUAGE C)
define_check_symbol_exists(asprintf "stdio.h" HAVE_ASPRINTF)
define_check_symbol_exists(chflags "sys/stat.h;unistd.h" HAVE_CHFLAGS)
define_check_symbol_exists(getattrlist "sys/attr.h;unistd.h" HAVE_GETATTRLIST)
define_check_symbol_exists(lchmod "sys/stat.h;unistd.h" HAVE_LCHMOD)
define_check_symbol_exists(lchown "sys/stat.h;unistd.h" HAVE_LCHOWN)
define_check_symbol_exists(lgetxattr "sys/xattr.h" HAVE_LGETXATTR)
define_check_symbol_exists(lsetxattr "sys/xattr.h" HAVE_LSETXATTR)
define_check_symbol_exists(setattrlist "sys/attr.h;unistd.h" HAVE_SETATTRLIST)
define_check_symbol_exists(statfs "sys/statfs.h" HAVE_STATFS)
define_check_symbol_exists(statvfs "sys/statvfs.h" HAVE_STATVFS)
define_check_symbol_exists(strmode "string.h" HAVE_STRMODE)
define_check_symbol_exists(vasprintf "stdio.h" HAVE_VASPRINTF)
