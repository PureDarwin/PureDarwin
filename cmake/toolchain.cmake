set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64) # TODO: arm64 support

set(triple x86_64-apple-darwin${RC_DARWIN_VERSION})

set(CMAKE_C_COMPILER ${RC_HOST_BIN}/clang)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER ${RC_HOST_BIN}/clang++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})

add_compile_options(-nostdinc)
add_link_options(-nostdlib)

# Ensure we can still find compiler internal headers
include_directories(SYSTEM ${RC_HOST_BIN}/../lib/clang/15.0.0/include)

# Set compiler flags from the passed variable.
set(CMAKE_CXX_FLAGS ${RC_NONARCH_CXXFLAGS})
set(CMAKE_C_FLAGS ${RC_NONARCH_CFLAGS})

# Enable LTO for the whole build
add_compile_options(-flto)
add_link_options(-flto)

# ASM flags
set(CMAKE_ASM_FLAGS "-x assembler-with-cpp")
