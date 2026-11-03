add_kext_bundle(PDRockchipDWMMC.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDRockchipDWMMC BUNDLE_VERSION 1.0.0)
set_property(TARGET PDRockchipDWMMC.kext PROPERTY OUTPUT_NAME PDRockchipDWMMC)

target_sources(PDRockchipDWMMC.kext PRIVATE
    PDRockchipDWMMC.cpp
    PDRockchipDWMMCDisk.cpp
)

target_include_directories(PDRockchipDWMMC.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDRockchipDWMMC.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDRockchipDWMMC.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDRockchipDWMMC.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDRockchipDWMMC.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
