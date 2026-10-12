add_kext_bundle(ext4.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.filesystems.ext4 BUNDLE_VERSION 1.0
)
set_property(TARGET ext4.kext PROPERTY OUTPUT_NAME ext4)

target_sources(ext4.kext PRIVATE
    ext4_subr.c
    ext4_csum.c
    ext4_jbd.c
    ext4_vfsops.c
    ext4_vnops.c
    ext4_xattr.c
    ext4_iokit.cpp
)

target_link_libraries(ext4.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(ext4.kext PRIVATE -Wno-sign-conversion -Wno-implicit-int-conversion)
install(TARGETS ext4.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
