# PureDarwin tools root
# This CMake script builds various source/binary modification tools we need to build
#   the PureDarwin Operating System.

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN-TOOLSET)

cmake_policy(SET CMP0114 NEW)

# These are generally what is expected
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

# Enable LTO for everything
add_compile_options(-flto)
add_link_options(-flto)

add_subdirectory(bison)
add_subdirectory(cctools)
add_subdirectory(ctfconvert)
add_subdirectory(developer_cmds)
add_subdirectory(gm4)
add_subdirectory(gnumake)
add_subdirectory(gperf)
add_subdirectory(ld64)
add_subdirectory(mig)
add_subdirectory(xar)

# There are various symlinks normally created by the above targets...
macro(install_link named target)
    install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${target} ${CMAKE_INSTALL_PREFIX}/${named})")
    install(CODE "message(\"-- Create alias: ${CMAKE_INSTALL_PREFIX}/${named} --> ${target}\")")
endmacro()

install_link(dwarfdump llvm-dwarfdump)
install_link(gcov llvm-cov)
install_link(m4 gm4)
install_link(make gnumake)

# These commands are as of yet missing from the host toolchain:
# docc, dwarfdump-classic, flex, flex++, lex, iig
# (flex, flex++, lex are in the source tree, just not built.)

# These are done
# bison --> bison, yacc
# bootstrap_cmds --> mig, migcom
# developer_cmds --> asa, ctags, indent, lorder, rpcgen, unifdef, unifdefall
# gm4 --> gm4, m4
# gnumake --> gnumake, make
# gperf --> gperf
# ld64 --> ld, dyldinfo, unwinddump, machocheck

# TODO:
# flex --> flex, flex++, lex

# Symlinks/other tools:
# c++filt, c89, c99, cpp
