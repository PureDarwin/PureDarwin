# PureDarwin tools root
# This CMake script builds various source/binary modification tools we need to build
#   the PureDarwin Operating System.

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

cmake_policy(SET CMP0114 NEW)

add_subdirectory(cctools)
add_subdirectory(ctfconvert)
add_subdirectory(developer_cmds)
add_subdirectory(ld64)
add_subdirectory(mig)
add_subdirectory(xar)

# WE NEED A LINKER!!

# These commands are as of yet missing from the host toolchain:
# asa, bison, ctags, docc, dwarfdump-classic, dyldinfo,
#   gm4, gperf, iig, indent, ld, lorder, m4, rpcgen, unwinddump, yacc

# We're still missing these few:
# docc, dwarfdump-classic, iig

# bison --> bison, yacc
# bootstrap_cmds --> mig, migcom
# developer_cmds --> asa, ctags, indent, lorder, rpcgen, unifdef, unifdefall
# flex --> flex, flex++, lex
# gm4 --> gm4, m4
# gnumake --> gnumake, make
# gperf --> gperf
# ld64 --> ld, dyldinfo, unwinddump, machocheck

# Symlinks/other tools:
# c++, c++filt, c89, c99, cc, cpp
# gcov --> llvm-cov
# dwarfdump --> llvm-dwarfdump
