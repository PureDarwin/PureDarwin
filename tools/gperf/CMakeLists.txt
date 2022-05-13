# gperf. This command generates perfect hashes

# BEGIN EXECUTABLE GPERF

add_executable(gperf)

target_sources(gperf PRIVATE
    gperf/lib/getline.cc
    gperf/lib/getopt.c
    gperf/lib/getopt1.c
    gperf/lib/hash.cc

    gperf/src/bool-array.cc
    gperf/src/hash-table.cc
    gperf/src/input.cc
    gperf/src/keyword-list.cc
    gperf/src/keyword.cc
    gperf/src/main.cc
    gperf/src/options.cc
    gperf/src/output.cc
    gperf/src/positions.cc
    gperf/src/search.cc
    gperf/src/version.cc
)

target_include_directories(gperf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/gperf/lib
)

target_compile_options(gperf PRIVATE -flto)
target_link_options(gperf PRIVATE -flto)

install(TARGETS gperf DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)

# END EXECUTABLE GPERF
