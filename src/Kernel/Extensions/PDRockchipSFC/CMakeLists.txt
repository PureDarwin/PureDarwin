add_kext_bundle(PDRockchipSFC.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDRockchipSFC BUNDLE_VERSION 1.0.0)
set_property(TARGET PDRockchipSFC.kext PROPERTY OUTPUT_NAME PDRockchipSFC)

target_sources(PDRockchipSFC.kext PRIVATE
    PDRockchipSFC.cpp
    PDRockchipSFCDisk.cpp
)

target_include_directories(PDRockchipSFC.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDRockchipSFC.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDRockchipSFC.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDRockchipSFC.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDRockchipSFC.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
