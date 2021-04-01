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



preprocess_file(doconf.in ${CMAKE_CURRENT_BINARY_DIR}/doconf
    CONFIG_PATH=$<TARGET_FILE:xnu_config_tool>
    XNU_PROJECT_ROOT=${PUREDARWIN_SOURCE_DIR}/projects/xnu
)
add_custom_target(xnu_doconf_gen DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/doconf)
add_executable(xnu_doconf IMPORTED GLOBAL)
set_property(TARGET xnu_doconf PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/doconf)
add_dependencies(xnu_doconf xnu_doconf_gen xnu_config_tool)
