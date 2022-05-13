# developer_cmds. This include the following tools:
# asa, ctags, indent, lorder, rpcgen, unifdef, unifdefall

macro(add_tool named)
    add_executable(${named})

    target_compile_options(${named} PRIVATE -flto)
    target_link_options(${named} PRIVATE -flto)

    install(TARGETS ${named} DESTINATION ${CMAKE_INSTALL_PREFIX} COMPONENT HostTools)
endmacro()

# BEGIN EXECUTABLE ASA

add_tool(asa)

target_sources(asa PRIVATE src/asa/asa.c)

# END EXECUTABLE ASA

# BEGIN EXECUTABLE CTAGS

add_tool(ctags)

target_sources(ctags PRIVATE
    src/ctags/C.c
    src/ctags/ctags.c
    src/ctags/fortran.c
    src/ctags/lisp.c
    src/ctags/print.c
    src/ctags/tree.c
    src/ctags/yacc.c
)

# END EXECUTABLE CTAGS

# BEGIN EXECUTABLE INDENT

add_tool(indent)

target_sources(indent PRIVATE
    src/indent/args.c
    src/indent/indent.c
    src/indent/io.c
    src/indent/lexi.c
    src/indent/parse.c
    src/indent/pr_comment.c
)

# END EXECUTABLE INDENT

# BEGIN SCRIPT LORDER

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/lorder/lorder.sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    RENAME lorder
    COMPONENT HostTools
)

# END SCRIPT LORDER

# BEGIN EXECUTABLE RPCGEN

add_tool(rpcgen)

target_sources(rpcgen PRIVATE
    src/rpcgen/rpc_clntout.c
    src/rpcgen/rpc_cout.c
    src/rpcgen/rpc_hout.c
    src/rpcgen/rpc_main.c
    src/rpcgen/rpc_parse.c
    src/rpcgen/rpc_sample.c
    src/rpcgen/rpc_scan.c
    src/rpcgen/rpc_svcout.c
    src/rpcgen/rpc_tblout.c
    src/rpcgen/rpc_util.c
)

# END EXECUTABLE RPCGEN

# BEGIN EXECUTABLE UNIFDEF

add_tool(unifdef)

target_sources(unifdef PRIVATE src/unifdef/unifdef.c)

# END EXECUTABLE UNIFDEF

# BEGIN SCRIPT UNIFDEFALL

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/src/unifdef/unifdefall.sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    RENAME unifdefall
    COMPONENT HostTools
)

# END SCRIPT UNIFDEFALL
