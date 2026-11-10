# CMakeLists.txt - the uz801 stick's status leds as the sysctl hw.pdled
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
add_kext_bundle(PDUZ801LED.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.PDUZ801LED BUNDLE_VERSION 1.0.0)
set_property(TARGET PDUZ801LED.kext PROPERTY OUTPUT_NAME PDUZ801LED)

target_sources(PDUZ801LED.kext PRIVATE
    PDUZ801LED.cpp
)

target_link_libraries(PDUZ801LED.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDUZ801LED.kext PRIVATE -Wno-sign-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDUZ801LED.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDUZ801LED.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
