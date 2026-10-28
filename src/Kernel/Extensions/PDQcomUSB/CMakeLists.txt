# SPDX-License-Identifier: MIT
# PDQcomUSB: the msm8916 chipidea device controller, a PDGadgetFamily backend
add_kext_bundle(PDQcomUSB.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDQcomUSB BUNDLE_VERSION 1.0.0)
set_property(TARGET PDQcomUSB.kext PROPERTY OUTPUT_NAME PDQcomUSB)

target_sources(PDQcomUSB.kext PRIVATE
    PDQcomCIUDC.cpp
)

# the framework is its own kext, linked through OSBundleLibraries
target_include_directories(PDQcomUSB.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../PDGadgetFamily)
target_link_libraries(PDQcomUSB.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDQcomUSB.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDQcomUSB.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDQcomUSB.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
