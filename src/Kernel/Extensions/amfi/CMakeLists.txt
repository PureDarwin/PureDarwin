add_kext_bundle(amfi.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.kec.amfi BUNDLE_VERSION 1.0.0d1
    MAIN_FUNCTION amfi_kext_start ANTIMAIN_FUNCTION amfi_kext_stop)
set_property(TARGET amfi.kext PROPERTY OUTPUT_NAME amfi)

target_sources(amfi.kext PRIVATE
    amfi_kext_main.c
)

target_compile_definitions(amfi.kext PRIVATE XNU_KERNEL_PRIVATE MACH_KERNEL_PRIVATE PRIVATE=1)
target_link_libraries(amfi.kext PRIVATE xnu_kernel_private_headers)

install(TARGETS amfi.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
