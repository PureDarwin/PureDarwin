# PureDarwin tools root
# This CMake script builds various source/binary modification tools we need to build
#   the PureDarwin Operating System.

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

cmake_policy(SET CMP0114 NEW)

add_subdirectory(cctools)
add_subdirectory(ctfconvert)
add_subdirectory(ld64)
add_subdirectory(mig)
add_subdirectory(unifdef)
add_subdirectory(xar)

# WE NEED A LINKER!!

# These commands are as of yet missing from the host toolchain:
# asa, bison, ctags, docc, dwarfdump-classic, dyldinfo, flex, flex++,
#   gm4, gperf, iig, indent, ld, lex, lorder, m4, rpcgen, unwinddump, yacc

# Symlinks/other tools:
# c++, c++filt, c89, c99, cc, cpp
# gcov --> llvm-cov
# dwarfdump --> llvm-dwarfdump
