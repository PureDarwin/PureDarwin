# CMakeLists.txt - the msm8916 wcnss (pronto) loader
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
add_kext_bundle(PDQcomWCNSS.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.PDQcomWCNSS BUNDLE_VERSION 1.0.0)
set_property(TARGET PDQcomWCNSS.kext PROPERTY OUTPUT_NAME PDQcomWCNSS)

target_sources(PDQcomWCNSS.kext PRIVATE
    PDQcomWCNSS.cpp
    PDQcomSMEM.cpp
    PDQcomSMD.cpp
)

target_link_libraries(PDQcomWCNSS.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDQcomWCNSS.kext PRIVATE -Wno-sign-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDQcomWCNSS.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDQcomWCNSS.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
