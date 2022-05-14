# gm4. This is a macro processor.

# BEGIN EXECUTABLE GM4

add_executable(gm4)

target_sources(gm4 PRIVATE
    gm4/lib/dup-safer.c
    gm4/lib/__fpending.c
    gm4/lib/asnprintf.c
    gm4/lib/cloexec.c
    gm4/lib/close-stream.c
    gm4/lib/error.c
    gm4/lib/exitfail.c
    gm4/lib/fd-safer.c
    gm4/lib/fopen-safer.c
    gm4/lib/getopt.c
    gm4/lib/getopt1.c
    gm4/lib/mbchar.c
    gm4/lib/memchr.c
    gm4/lib/mkstemp-safer.c
    gm4/lib/obstack.c
    gm4/lib/pipe-safer.c
    gm4/lib/printf-args.c
    gm4/lib/printf-parse.c
    gm4/lib/regex.c
    gm4/lib/strcasecmp.c
    gm4/lib/strncasecmp.c
    gm4/lib/strnlen1.c
    gm4/lib/tmpfile-safer.c
    gm4/lib/vasnprintf.c
    gm4/lib/verror.c
    gm4/lib/xalloc-die.c
    gm4/lib/xasprintf.c
    gm4/lib/xmalloc.c
    gm4/lib/xvasprintf.c

    gm4/src/builtin.c
    gm4/src/debug.c
    gm4/src/eval.c
    gm4/src/format.c
    gm4/src/freeze.c
    gm4/src/input.c
    gm4/src/m4.c
    gm4/src/macro.c
    gm4/src/output.c
    gm4/src/path.c
    gm4/src/stackovf.c
    gm4/src/symtab.c
)

target_include_directories(gm4 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gm4
    ${CMAKE_CURRENT_SOURCE_DIR}/gm4/lib
)

target_compile_definitions(gm4 PRIVATE HAVE_CONFIG_H)

install(TARGETS gm4 DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE BISON
