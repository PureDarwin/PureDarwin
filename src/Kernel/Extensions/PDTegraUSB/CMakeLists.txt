# SPDX-License-Identifier: MIT
# PDTegraUSB: the tegra210 legacy usb1 chipidea device controller, a PDGadgetFamily backend
add_kext_bundle(PDTegraUSB.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDTegraUSB BUNDLE_VERSION 1.0.0)
set_property(TARGET PDTegraUSB.kext PROPERTY OUTPUT_NAME PDTegraUSB)

target_sources(PDTegraUSB.kext PRIVATE
    PDTegraUDC.cpp
)

# the framework is its own kext, linked through OSBundleLibraries
target_include_directories(PDTegraUSB.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../PDGadgetFamily)
target_link_libraries(PDTegraUSB.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDTegraUSB.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDTegraUSB.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDTegraUSB.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
