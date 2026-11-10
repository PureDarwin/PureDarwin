add_kext_bundle(PDTegraSDMMC.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDTegraSDMMC BUNDLE_VERSION 1.0.0)
set_property(TARGET PDTegraSDMMC.kext PROPERTY OUTPUT_NAME PDTegraSDMMC)

target_sources(PDTegraSDMMC.kext PRIVATE
    PDTegraSDMMC.cpp
    PDTegraSDMMCDisk.cpp
)

target_include_directories(PDTegraSDMMC.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDTegraSDMMC.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDTegraSDMMC.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDTegraSDMMC.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDTegraSDMMC.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
