# Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.15)

project(libapfsrw C)

add_library(apfsrw STATIC
    src/apfsrw.c
    src/apfsrw_lzvn.c
    third_party/lzfse/lzvn_decode_base.c
    third_party/libzbitmap/libzbitmap.c
)

# Vendored upstream (BSD-3-Clause); built as-is, so keep our -Werror off it.
set_source_files_properties(
    third_party/lzfse/lzvn_decode_base.c
    third_party/libzbitmap/libzbitmap.c
    PROPERTIES COMPILE_OPTIONS "-w")

target_include_directories(apfsrw SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lzfse
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libzbitmap
)

target_include_directories(apfsrw PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(apfsrw PUBLIC c_std_99)
target_compile_options(apfsrw PRIVATE
    -Wall
    -Wextra
    -Werror
)

add_executable(apfsrw-tool
    tools/apfsrw.c
)

set_target_properties(apfsrw-tool PROPERTIES
    OUTPUT_NAME apfsrw
)

target_link_libraries(apfsrw-tool PRIVATE apfsrw)

install(TARGETS apfsrw ARCHIVE DESTINATION lib)
install(TARGETS apfsrw-tool RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)
