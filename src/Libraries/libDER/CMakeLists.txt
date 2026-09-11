# Copyright (c) 2026 The PureDarwin Project
#
# SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

add_library(libDER_static STATIC
    lib/DER_Decode.c
    lib/DER_Encode.c
    lib/DER_CertCrl.c
    lib/DER_Keys.c
    lib/oids.c
)
set_target_properties(libDER_static PROPERTIES OUTPUT_NAME DER)

target_include_directories(libDER_static PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Parses attacker-supplied certificates: keep the warnings on.
target_compile_options(libDER_static PRIVATE -Wall -Wextra)
