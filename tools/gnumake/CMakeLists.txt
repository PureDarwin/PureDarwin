# gnumake. This is the make command used on MacOS/Darwin

# BEGIN EXECUTABLE GNUMAKE

add_executable(gnumake)

target_sources(gnumake PRIVATE
    src/glob/fnmatch.c
    src/glob/glob.c

    src/ar.c
    src/arscan.c
    src/commands.c
    src/default.c
    src/dir.c
    src/expand.c
    src/file.c
    src/function.c
    src/getopt.c
    src/getopt1.c
    src/hash.c
    src/implicit.c
    src/job.c
    src/main.c
    src/misc.c
    src/read.c
    src/remake.c
    src/remote-stub.c
    src/rule.c
    src/signame.c
    src/strcache.c
    src/variable.c
    src/version.c
    src/vpath.c
    src/next.c
)

target_include_directories(gnumake PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glob
)

target_compile_definitions(gnumake PRIVATE INCLUDEDIR=\"/usr/include\" LIBDIR=\"/usr/lib\" LOCALEDIR=\"/usr/share/locale\")
target_compile_definitions(gnumake PRIVATE HAVE_CONFIG_H USE_POSIX_SPAWN)

target_compile_options(gnumake PRIVATE -Wno-string-compare)

install(TARGETS gnumake DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE GNUMAKE
