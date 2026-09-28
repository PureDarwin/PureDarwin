# CMake toolchain file for the darwin-cross-toolchain-nix toolchain (see
# ../toolchain.nix) - an alternative to osxcross/toolchain.cmake that uses
# nixpkgs' own unwrapped LLVM/clang/lld instead of a from-source osxcross
# build. Verified to produce byte-for-byte equivalent Mach-O output (modulo
# a cosmetic LC_SOURCE_VERSION load command) for real PureDarwin targets.
#
# Usage:
#   NIX_DARWIN_TOOLCHAIN=$(nix build --no-link --print-out-paths .#darwin-cross-toolchain)
#   cmake -B build-nix -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/nix-toolchain.cmake \
#     -DNIX_DARWIN_TOOLCHAIN_DIR=$NIX_DARWIN_TOOLCHAIN/bin \
#     ...
#
# DARWIN_SDK_ROOT (env var, same one the wrapper scripts read) still
# defaults to /usr/local/osxcross/SDK/MacOSX11.3.sdk if unset.

if(NOT NIX_DARWIN_TOOLCHAIN_DIR)
    set(NIX_DARWIN_TOOLCHAIN_DIR "$ENV{NIX_DARWIN_TOOLCHAIN_DIR}")
    if(NOT NIX_DARWIN_TOOLCHAIN_DIR)
        message(FATAL_ERROR "NIX_DARWIN_TOOLCHAIN_DIR not set (pass -DNIX_DARWIN_TOOLCHAIN_DIR=<nix-osxcross-result>/bin or export it)")
    endif()
endif()

if(NOT NIX_DARWIN_HOST)
    set(NIX_DARWIN_HOST "$ENV{NIX_DARWIN_HOST}")
    if(NOT NIX_DARWIN_HOST)
        set(NIX_DARWIN_HOST "x86_64-apple-darwin20.4")
    endif()
endif()

if(NOT NIX_DARWIN_SDK_ROOT)
    set(NIX_DARWIN_SDK_ROOT "$ENV{DARWIN_SDK_ROOT}")
    if(NOT NIX_DARWIN_SDK_ROOT)
        set(NIX_DARWIN_SDK_ROOT "/usr/local/osxcross/SDK/MacOSX11.3.sdk")
    endif()
endif()

set(CMAKE_SYSTEM_NAME "Darwin")
if(NIX_DARWIN_HOST MATCHES "^arm64")
    set(CMAKE_SYSTEM_PROCESSOR "arm64")
elseif(NIX_DARWIN_HOST MATCHES "^riscv64")
    set(CMAKE_SYSTEM_PROCESSOR "riscv64")
else()
    set(CMAKE_SYSTEM_PROCESSOR "x86_64")
endif()

set(CMAKE_C_COMPILER "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-clang")
set(CMAKE_CXX_COMPILER "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-clang++")
set(CMAKE_OBJC_COMPILER "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-clang")
set(CMAKE_OBJCXX_COMPILER "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-clang++")

set(CMAKE_FIND_ROOT_PATH
  "${CMAKE_FIND_ROOT_PATH}"
  "${NIX_DARWIN_SDK_ROOT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_AR "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-ar" CACHE FILEPATH "ar")
set(CMAKE_RANLIB "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-ranlib" CACHE FILEPATH "ranlib")
set(CMAKE_INSTALL_NAME_TOOL "${NIX_DARWIN_TOOLCHAIN_DIR}/${NIX_DARWIN_HOST}-install_name_tool" CACHE FILEPATH "install_name_tool")

# Flags a handful of CMakeLists (src/Libraries/libcxxabi) that otherwise
# only distinguish "real Apple clang" vs "osxcross" for header-search-order
# decisions: unlike osxcross's own from-source clang build (which injects
# its bundled libc headers even after an explicit -idirafter), this
# toolchain is a thin wrapper around nixpkgs' vanilla clang, whose resource-
# dir headers follow ordinary -idirafter semantics (always last) - so code
# written assuming osxcross's inverted ordering needs a plain -I instead.
set(PUREDARWIN_NIX_TOOLCHAIN 1 CACHE BOOL "Using darwin-cross-toolchain-nix (nixpkgs LLVM) instead of osxcross")
