# FSKit's kernel half (lifs) as fskitd and FSKit extensions open it, without the LiveFS VFS
add_kext_bundle(PDLifs.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.filesystems.PDLifs BUNDLE_VERSION 1.0.0)
set_property(TARGET PDLifs.kext PROPERTY OUTPUT_NAME PDLifs)

target_sources(PDLifs.kext PRIVATE
    PDLifs.cpp
)

target_link_libraries(PDLifs.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET PDLifs.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDLifs.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
