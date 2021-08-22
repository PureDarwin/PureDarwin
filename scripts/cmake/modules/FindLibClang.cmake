#
# Try to find libclang
#
# Once done this will define:
# - LIBCLANG_FOUND
#               System has libclang
# - LIBCLANG_INCLUDE_DIRS
#               The libclang include directories
# - LIBCLANG_LIBRARIES
#               The libraries needed to use libclang
# - LIBCLANG_LIBRARY_DIR
#               The path to the directory containing libclang.
#
# At the CMake invocation level it is possible to specify some hints for the
# libclang installation, e.g: for non-standard libclang installations.
#
# To specify the include directory use:
#   -DLIBCLANG_INCLUDE_PATH=/path/to/libclang/include-dir
# The specified directory should contain the header file 'clang-c/Index.h'
#
# To specify the library directory use:
#   -DLIBCLANG_LIBRARY_PATH=/path/to/libclang/libraries
# The specified directory should contain the libclang library, e.g: libclang.so
# on Linux.
#
# CMake invocation example with a custom libclang installation:
#     cmake -DLIBCLANG_INCLUDE_PATH=~/llvm-3.4/include/ \
#           -DLIBCLANG_LIBRARY_PATH=~/llvm-3.4/lib/ <args...>

set(libclang_llvm_header_search_paths)
set(libclang_llvm_lib_search_paths)

if(CMAKE_HOST_APPLE)
    list(APPEND libclang_llvm_header_search_paths "/usr/local/opt/llvm/include")
    list(APPEND libclang_llvm_lib_search_paths "/usr/local/opt/llvm/lib")
else()
    # TODO: Add default search locations for non-macOS platforms
endif()

find_path(LIBCLANG_INCLUDE_DIR clang-c/Index.h
  HINTS ${LIBCLANG_INCLUDE_PATH}
  PATHS ${libclang_llvm_header_search_paths})

find_library(LIBCLANG_LIBRARY NAMES clang libclang
  HINTS ${LIBCLANG_LIBRARY_PATH}
  PATHS ${libclang_llvm_lib_search_paths})

get_filename_component(LIBCLANG_LIBRARY_DIR ${LIBCLANG_LIBRARY} PATH)

set(LIBCLANG_LIBRARIES ${LIBCLANG_LIBRARY})
set(LIBCLANG_INCLUDE_DIRS ${LIBCLANG_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBCLANG_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(LibClang DEFAULT_MSG
  LIBCLANG_LIBRARIES LIBCLANG_INCLUDE_DIR)

mark_as_advanced(LIBCLANG_INCLUDE_DIR LIBCLANG_LIBRARY)
