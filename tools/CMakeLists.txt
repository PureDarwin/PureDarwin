# PureDarwin tools root
# This CMake script builds various source/binary modification tools we need to build
#   the PureDarwin Operating System.

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

cmake_policy(SET CMP0114 NEW)

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

install_link(m4 gm4)
install_link(make gnumake)

# WE NEED A LINKER!!

# These commands are as of yet missing from the host toolchain:
# docc, dwarfdump-classic, flex, flex++, lex, iig, ld

# We're still missing these few:
# docc, dwarfdump-classic, iig

# These are done
# bison --> bison, yacc
# bootstrap_cmds --> mig, migcom
# developer_cmds --> asa, ctags, indent, lorder, rpcgen, unifdef, unifdefall
# gm4 --> gm4, m4
# gnumake --> gnumake, make
# gperf --> gperf

# TODO:
# flex --> flex, flex++, lex
# ld64 --> ld, dyldinfo, unwinddump, machocheck

# Symlinks/other tools:
# c++, c++filt, c89, c99, cc, cpp
# gcov --> llvm-cov
# dwarfdump --> llvm-dwarfdump
