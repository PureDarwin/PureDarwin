# mig tool. This is pretty simple, we create an executable from a few source files,
#   make the parser and lexxer, and install the mig.sh shell script to invoke migcom

# BEGIN EXECUTABLE MIGCOM

add_executable(migcom)

target_sources(migcom PRIVATE
    src/error.c
    src/global.c
    src/header.c
    src/mig.c
    src/routine.c
    src/server.c
    src/statement.c
    src/string.c
    src/type.c
    src/user.c
    src/utils.c

    # Grammar/parser generated from parser.y and lexxer.l
    ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
    ${CMAKE_CURRENT_BINARY_DIR}/lexxer.yy.c
)

target_compile_definitions(migcom PRIVATE MIG_VERSION=\"\")
target_include_directories(migcom PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(migcom PRIVATE -flto)
target_link_options(migcom PRIVATE -flto)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
    COMMAND yacc -d -b "${CMAKE_CURRENT_BINARY_DIR}/y" ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y
    COMMENT "Yacc parser.y" VERBATIM
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lexxer.yy.c
    COMMAND lex -o "${CMAKE_CURRENT_BINARY_DIR}/lexxer.yy.c" ${CMAKE_CURRENT_SOURCE_DIR}/src/lexxer.l
    COMMENT "Lex lexxer.l" VERBATIM
)

# The mig.sh script expects migcom to be here
install(TARGETS migcom DESTINATION ${CMAKE_INSTALL_PREFIX}/../libexec COMPONENT HostTools)

# END EXECUTABLE MIGCOM

# BEGIN SCRIPT MIG

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/mig.sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    RENAME mig
    COMPONENT HostTools
)

# END SCRIPT MIG
