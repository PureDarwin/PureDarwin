add_executable(xnu_config_tool)

target_sources(xnu_config_tool PRIVATE
    externs.c
    main.c
    mkheaders.c
    mkioconf.c
    mkmakefile.c
    openp.c
    searchp.c

    ${CMAKE_CURRENT_BINARY_DIR}/parser.c
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.c
)
target_include_directories(xnu_config_tool PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser.c
    COMMAND bison -y -d -d -o ${CMAKE_CURRENT_BINARY_DIR}/parser.c ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
    COMMENT "Yacc parser.y" VERBATIM
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lexer.c ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.h
    COMMAND lex --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.h -o ${CMAKE_CURRENT_BINARY_DIR}/lexer.c ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/lexer.l
    COMMENT "Lex lexer.l" VERBATIM
)
