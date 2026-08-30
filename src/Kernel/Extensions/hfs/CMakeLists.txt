add_kext_bundle(hfs.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.filesystems.hfs.kext BUNDLE_VERSION 1.7)
set_property(TARGET hfs.kext PROPERTY OUTPUT_NAME hfs)

target_sources(hfs.kext PRIVATE
    core/BTree.c
    core/BTreeAllocate.c
    core/BTreeMiscOps.c
    core/BTreeNodeOps.c
    core/BTreeNodeReserve.c
    core/BTreeScanner.c
    core/BTreeTreeOps.c
    core/BTreeWrapper.c
    core/CatalogUtilities.c
    core/FileExtentMapping.c
    core/FileIDsServices.c
    core/hfs_attrlist.c
    core/hfs_btreeio.c
    core/hfs_catalog.c
    core/hfs_chash.c
    core/hfs_cnode.c
    core/hfs_cprotect.c
    core/hfs_endian.c
    core/hfs_extents.c
    core/hfs_fsinfo.c
    core/hfs_hotfiles.c
    core/hfs_iokit.cpp
    core/hfs_journal.c
    core/hfs_link.c
    core/hfs_lookup.c
    core/hfs_notification.c
    core/hfs_quota.c
    core/hfs_readwrite.c
    core/hfs_resize.c
    core/hfs_search.c
    core/hfs_vfsops.c
    core/hfs_vfsutils.c
    core/hfs_vnops.c
    core/hfs_xattr.c
    core/MacOSStubs.c
    core/rangelist.c
    core/UnicodeWrappers.c
    core/VolumeAllocation.c
)

target_include_directories(hfs.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/hfs_encodings
    ${CMAKE_CURRENT_SOURCE_DIR}/../corecrypto/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(hfs.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(hfs.kext PRIVATE
    -include${CMAKE_CURRENT_SOURCE_DIR}/core/kext-config.h
    -DKERNEL=1 -DKERNEL_PRIVATE=1
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual
    -Wno-missing-field-initializers -Wno-deprecated-declarations
    -Wno-format -Wno-pointer-sign -Wno-incompatible-pointer-types)
set_property(TARGET hfs.kext PROPERTY CXX_STANDARD 14)
add_dependencies(hfs.kext HFSEncodings.kext)
install(TARGETS hfs.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
