# Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT

add_kext_bundle(apfs.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.filesystems.apfs BUNDLE_VERSION 0.1
)
set_property(TARGET apfs.kext PROPERTY OUTPUT_NAME apfs)

# The multi-level B-tree write path is shared verbatim with the image builder
# (projects/libapfsrw) so there is one implementation to debug rather than two.
set(LIBAPFSRW_DIR ${CMAKE_SOURCE_DIR}/projects/libapfsrw)

target_sources(apfs.kext PRIVATE
    apfs_btree.c
    apfs_iokit.cpp
    apfs_vfsops.c
    apfs_vnops.c
    ${LIBAPFSRW_DIR}/src/apfsrw.c
    ${LIBAPFSRW_DIR}/src/apfsrw_kern.c
)

target_include_directories(apfs.kext PRIVATE
    ${LIBAPFSRW_DIR}/include)

set_source_files_properties(
    ${LIBAPFSRW_DIR}/src/apfsrw.c
    ${LIBAPFSRW_DIR}/src/apfsrw_kern.c
    PROPERTIES COMPILE_DEFINITIONS APFSRW_KERNEL=1)

target_link_libraries(apfs.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(apfs.kext PRIVATE
    -DKERNEL=1 -DKERNEL_PRIVATE=1
    -Wno-sign-conversion -Wno-implicit-int-conversion)
install(TARGETS apfs.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
