# xnu-tools. This directory builds various tools normally built as part of the
#   setup stage of an XNU build. We include these with our host toolchian.

# Here, we build a few host tools:
# setsegname - this is the same as seg_hack in cctools, but xnu comes with its own version
# kextsymboltool
# decomment
# installfile
# replacecontents
# json_compilation_db

macro(add_tool named)
    add_executable(${named})

    target_compile_options(${named} PRIVATE -O0)

    install(TARGETS ${named} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)
endmacro()

macro(add_tool_simple named)
    add_tool(${named})

    target_sources(${named} PRIVATE src/${named}/${named}.c)
endmacro()

add_tool_simple(decomment)
add_tool_simple(installfile)
add_tool_simple(json_compilation_db)
add_tool_simple(replacecontents)
add_tool_simple(setsegname)

add_tool_simple(kextsymboltool)
target_link_options(kextsymboltool PRIVATE -lc++) # For __cxa_demangle

# BEGIN EXECUTABLE config

# TODO: Should we name this something else?
add_tool(xnu-config)

target_sources(xnu-config PRIVATE
    src/config/externs.c
    src/config/main.c
    src/config/mkheaders.c
    src/config/mkioconf.c
    src/config/mkmakefile.c
    src/config/openp.c
    src/config/searchp.c

    # Grammar/parser generated from parser.y and lexer.l
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.c
    ${CMAKE_CURRENT_BINARY_DIR}/parser.c
)

target_include_directories(xnu-config PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/config)
target_include_directories(xnu-config PRIVATE ${CMAKE_CURRENT_BINARY_DIR})


add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser.c
    COMMAND yacc -y -d -d -o "${CMAKE_CURRENT_BINARY_DIR}/parser.c" ${CMAKE_CURRENT_SOURCE_DIR}/src/config/parser.y
    COMMENT "Yacc parser.y" VERBATIM
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.c
    COMMAND lex --header-file=lexer.yy.h -o "${CMAKE_CURRENT_BINARY_DIR}/lexer.yy.c" ${CMAKE_CURRENT_SOURCE_DIR}/src/config/lexer.l
    COMMENT "Lex lexer.l" VERBATIM
)

# END EXECUTABLE config
