add_kext_bundle(cd9660.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.filesystems.cd9660 BUNDLE_VERSION 1.0
)
set_property(TARGET cd9660.kext PROPERTY OUTPUT_NAME cd9660)

target_sources(cd9660.kext PRIVATE
    isofs/cd9660/cd9660_bmap.c
    isofs/cd9660/cd9660_lookup.c
    isofs/cd9660/cd9660_module.c
    isofs/cd9660/cd9660_node.c
    isofs/cd9660/cd9660_rrip.c
    isofs/cd9660/cd9660_util.c
    isofs/cd9660/cd9660_vfsops.c
    isofs/cd9660/cd9660_vnops.c
    cd9660_iokit.cpp
)

target_include_directories(cd9660.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(cd9660.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(cd9660.kext PRIVATE KERNEL=1)
target_compile_options(cd9660.kext PRIVATE
    -Wno-deprecated-declarations
    -Wno-implicit-int-conversion
    -Wno-sign-conversion
    -Wno-shorten-64-to-32
)
install(TARGETS cd9660.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
