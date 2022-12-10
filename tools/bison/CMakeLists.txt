# bison. This builds bison and the yacc shell script

# BEGIN EXECUTABLE BISON

add_executable(bison)

target_sources(bison PRIVATE
    bison/lib/abitset.c
    bison/lib/argmatch.c
    bison/lib/basename.c
    bison/lib/bitset.c
    bison/lib/bitset_stats.c
    bison/lib/bitsetv-print.c
    bison/lib/bitsetv.c
    bison/lib/dirname.c
    bison/lib/dup-safer.c
    bison/lib/ebitset.c
    bison/lib/error.c
    bison/lib/exitfail.c
    bison/lib/fd-safer.c
    bison/lib/fopen-safer.c
    bison/lib/get-errno.c
    bison/lib/getopt.c
    bison/lib/getopt1.c
    bison/lib/hard-locale.c
    bison/lib/hash.c
    bison/lib/lbitset.c
    bison/lib/mbswidth.c
    bison/lib/obstack.c
    bison/lib/pipe-safer.c
    bison/lib/quote.c
    bison/lib/quotearg.c
    bison/lib/stripslash.c
    bison/lib/strverscmp.c
    bison/lib/subpipe.c
    bison/lib/timevar.c
    bison/lib/vbitset.c
    bison/lib/xalloc-die.c
    bison/lib/xmalloc.c
    bison/lib/xstrndup.c

    bison/src/assoc.c
    bison/src/closure.c
    bison/src/complain.c
    bison/src/conflicts.c
    bison/src/derives.c
    bison/src/files.c
    bison/src/getargs.c
    bison/src/gram.c
    bison/src/lalr.c
    bison/src/location.c
    bison/src/LR0.c
    bison/src/main.c
    bison/src/muscle_tab.c
    bison/src/nullable.c
    bison/src/output.c
    bison/src/parse-gram.c
    bison/src/print.c
    bison/src/print_graph.c
    bison/src/reader.c
    bison/src/reduce.c
    bison/src/relation.c
    bison/src/scan-gram-c.c
    bison/src/scan-skel-c.c
    bison/src/state.c
    bison/src/symlist.c
    bison/src/symtab.c
    bison/src/tables.c
    bison/src/uniqstr.c
    bison/src/vcg.c
)

target_include_directories(bison PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bison
    ${CMAKE_CURRENT_SOURCE_DIR}/bison/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bison/lib
)

target_compile_definitions(bison PRIVATE HAVE_CONFIG_H LOCALEDIR=\"/usr/share/locale\" PKGDATADIR=\"/usr/share/bison\")
target_compile_options(bison PRIVATE -flto)
target_link_options(bison PRIVATE -flto)

install(TARGETS bison DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE BISON

# BEGIN SCRIPT YACC

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/bison/yacc.sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    RENAME yacc
    COMPONENT HostTools
)

# END SCRIPT YACC

# bison needs these files to run properly...

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/m4sugar/m4sugar.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}/../share/bison/m4sugar
    COMPONENT HostTools
)

install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/c++.m4
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/c.m4
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/glr.c
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/glr.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/lalr1.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/location.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/README
        ${CMAKE_CURRENT_SOURCE_DIR}/bison/data/yacc.c
    DESTINATION ${CMAKE_INSTALL_PREFIX}/../share/bison
    COMPONENT HostTools
)
