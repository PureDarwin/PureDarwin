add_darwin_shared_library(BlocksRuntime NO_STANDARD_LIBRARIES INSTALL_NAME_DIR /usr/lib/system)
target_sources(BlocksRuntime PRIVATE data.c runtime.c)
target_include_directories(BlocksRuntime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
