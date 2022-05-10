# PureDarwin tools root
# This CMake script builds various source/binary modification tools we need to build
#   the PureDarwin Operating System.

cmake_minimum_required(VERSION 3.15.1)
project(PUREDARWIN)

cmake_policy(SET CMP0114 NEW)

#add_subdirectory(cctools)
add_subdirectory(ctfconvert)
add_subdirectory(mig)
add_subdirectory(unifdef)
add_subdirectory(xar)

# cctools (from cctools)
# dtrace tools
# ld64 (from ld64)
