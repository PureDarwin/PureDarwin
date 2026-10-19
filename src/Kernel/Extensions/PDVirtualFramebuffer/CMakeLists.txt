# SPDX-License-Identifier: MIT
# PDVirtualFramebuffer: an IOFramebuffer in ram for a machine with no display, opt in with pdvfb=
add_kext_bundle(PDVirtualFramebuffer.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.PDVirtualFramebuffer BUNDLE_VERSION 1.0.0)
set_property(TARGET PDVirtualFramebuffer.kext PROPERTY OUTPUT_NAME PDVirtualFramebuffer)

target_sources(PDVirtualFramebuffer.kext PRIVATE
    PDVirtualFramebuffer.cpp
)

target_include_directories(PDVirtualFramebuffer.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem)
target_link_libraries(PDVirtualFramebuffer.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDVirtualFramebuffer.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDVirtualFramebuffer.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDVirtualFramebuffer.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
