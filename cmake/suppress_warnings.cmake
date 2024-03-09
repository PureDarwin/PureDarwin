macro(suppress_all_warnings target)
    target_compile_options(${target} PRIVATE -w)
    target_link_options(${target} PRIVATE -w)
endmacro()
