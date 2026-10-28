# SPDX-License-Identifier: MIT
# PDGadgetFamily: the usb device framework (PDGadgetController) and its functions, for controller backend kexts
add_kext_bundle(PDGadgetFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.iokit.PDGadgetFamily BUNDLE_VERSION 1.0.0)
set_property(TARGET PDGadgetFamily.kext PROPERTY OUTPUT_NAME PDGadgetFamily)

target_sources(PDGadgetFamily.kext PRIVATE
    PDGadget.cpp
    PDGadgetACM.cpp
    PDGadgetPipe.cpp
)

target_include_directories(PDGadgetFamily.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDGadgetFamily.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDGadgetFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDGadgetFamily.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDGadgetFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
