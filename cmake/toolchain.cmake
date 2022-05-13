set(CMAKE_SYSTEM_NAME Darwin)
set(CMAKE_SYSTEM_PROCESSOR x86_64) # TODO: arm64 support

set(triple x86_64-apple-darwin${RC_DARWIN_VERSION})

set(CMAKE_C_COMPILER ${RC_HOST_BIN}/clang)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER ${RC_HOST_BIN}/clang++)
set(CMAKE_CXX_COMPILER_TARGET ${triple})