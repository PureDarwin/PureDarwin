add_executable(xnu_kextsymboltool)
target_sources(xnu_kextsymboltool PRIVATE kextsymboltool.c)
target_link_libraries(xnu_kextsymboltool PRIVATE -lstdc++)
