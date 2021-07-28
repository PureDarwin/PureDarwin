file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/public)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/private)

add_library(Libc_headers INTERFACE)
target_include_directories(Libc_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/public)
target_link_libraries(Libc_headers INTERFACE libmalloc_headers xnu_headers AvailabilityHeaders pthread_common_headers)

add_library(Libc_private_headers INTERFACE)
target_include_directories(Libc_private_headers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/private)

function(process_libc_headers)
    cmake_parse_arguments(AH "PRIVATE" "SUBDIRECTORY" "FILES" ${ARGN})

    if(AH_PRIVATE)
        set(install_dir ${CMAKE_CURRENT_BINARY_DIR}/private)
    else()
        set(install_dir ${CMAKE_CURRENT_BINARY_DIR}/public)
    endif()

    get_libc_feature_unifdef_args(feature_args)
    foreach(file IN LISTS AH_FILES)
        get_filename_component(base ${file} NAME)
        get_filename_component(abs ${file} ABSOLUTE)
        set(dest ${install_dir}/${AH_SUBDIRECTORY}/${base})

        add_custom_command(OUTPUT ${dest}
            COMMAND mkdir -p ${install_dir}/${AH_SUBDIRECTORY}
            COMMAND $<TARGET_FILE_NAME:unifdef> -n ${feature_args} -o ${dest}.in ${abs}
            COMMAND cat ${dest}.in | sed -Ee "/^\\/\\/Begin-Libc$/,/^\\/\\/End-Libc$/d" > ${dest}
            COMMAND rm ${dest}.in
            DEPENDS unifdef ${abs}
            COMMENT "Preprocess Libc header ${base}" VERBATIM
        )

        if(AH_PRIVATE)
            target_sources(Libc_private_headers PRIVATE ${dest})
        else()
            target_sources(Libc_headers PRIVATE ${dest})
        endif()
    endforeach()
endfunction()

process_libc_headers(FILES
    __wctype.h _ctype.h _locale.h _regex.h _stdio.h _types.h _wctype.h _xlocale.h _ctermid.h aio.h
    alloca.h ar.h assert.h asm.h bitstring.h cpio.h crt_externs.h ctype.h db.h dirent.h disktab.h
    err.h errno.h float.h fcntl.h fmtmsg.h fnmatch.h fsproperties.h fstab.h fts.h ftw.h getopt.h glob.h
    inttypes.h iso646.h langinfo.h libc.h libgen.h limits.h locale.h memory.h monetary.h monitor.h
    mpool.h ndbm.h nlist.h paths.h printf.h poll.h ranlib.h readpassphrase.h regex.h runetype.h
    search.h semaphore.h sgtty.h signal.h stab.h standards.h stdbool.h stddef.h stdio.h stdint.h stdlib.h
    strhash.h string.h stringlist.h strings.h struct.h sysexits.h syslog.h tar.h termios.h time.h
    timeconv.h ttyent.h ulimit.h unistd.h util.h utime.h vis.h wchar.h wctype.h wordexp.h xlocale.h
    FreeBSD/nl_types.h NetBSD/utmpx.h ../gen/get_compat.h ../gen/execinfo.h
)

if(FEATURE_LEGACY_RUNE_APIS)
    process_libc_headers(FILES rune.h)
endif()
if(FEATURE_LEGACY_UTMP_APIS)
    process_libc_headers(FILES utmp.h)
endif()
if(FEATURE_THERM_NOTIFICATION_APIS)
    process_libc_headers(SUBDIRECTORY libkern FILES libkern/OSThermalNotification.h)
endif()

process_libc_headers(SUBDIRECTORY _types FILES _types/_intmax_t.h _types/_nl_item.h _types/_uint16_t.h
    _types/_uint32_t.h _types/_uint64_t.h _types/_uint8_t.h _types/_uintmax_t.h _types/_wctrans_t.h _types/_wctype_t.h)
process_libc_headers(SUBDIRECTORY arpa FILES arpa/ftp.h arpa/inet.h arpa/nameser_compat.h arpa/telnet.h arpa/tftp.h)
process_libc_headers(SUBDIRECTORY protocols FILES protocols/routed.h protocols/rwhod.h protocols/talkd.h protocols/timed.h)
process_libc_headers(SUBDIRECTORY secure FILES secure/_common.h secure/_string.h secure/_strings.h secure/_stdio.h)
process_libc_headers(SUBDIRECTORY sys sys/statvfs.h)

process_libc_headers(PRIVATE FILES ../darwin/libc_private.h ../gen/utmpx_thread.h ../nls/FreeBSD/msgcat.h
    ../gen/thread_stack_pcs.h ../libdarwin/h/dirstat.h ../darwin/subsystem.h ../stdlib/FreeBSD/atexit.h)
process_libc_headers(PRIVATE SUBDIRECTORY os FILES ../os/debug_private.h)
process_libc_headers(PRIVATE SUDIRECTORY btree FILES ../db/btree/FreeBSD/btree.h ../db/btree/FreeBSD/bt_extern.h)
process_libc_headers(PRIVATE SUBDIRECTORY sys FILES sys/acl.h sys/rbtree.h sys/statvfs.h)
process_libc_headers(PRIVATE SUBDIRECTORY uuid FILES ../uuid/namespace.h)

# These two are just copied.
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/_types.modulemap DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/public/_types.modulemap)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/stdint.modulemap DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/public/stdint.modulemap)
