# SPDX-License-Identifier: MIT
# PDTegraXUSB: the tegra210 xusb host's partitions, clocks, pads and falcon, the xhci itself is RavynXHCIPort
add_kext_bundle(PDTegraXUSB.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDTegraXUSB BUNDLE_VERSION 1.0.0)
set_property(TARGET PDTegraXUSB.kext PROPERTY OUTPUT_NAME PDTegraXUSB)

# FreeBSD's tegra210 car, pmc, xhci and padctl excerpts, shared with PDTegraDisplay and PDDRMnouveau
set(PD_TEGRA_FBSDKPI ${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi)
set(PD_TEGRA_XUSB_BSD_SOURCES
    ${PD_TEGRA_FBSDKPI}/freebsd/arm64/nvidia/tegra210/tegra210_gpu.c
    ${PD_TEGRA_FBSDKPI}/freebsd/arm64/nvidia/tegra210/tegra210_xusb.c)

target_sources(PDTegraXUSB.kext PRIVATE
    PDTegraXUSB.cpp
    ${PD_TEGRA_XUSB_BSD_SOURCES}
)
set_source_files_properties(${PD_TEGRA_XUSB_BSD_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/../PDTegraDisplay/pd_tegra_bsd_shim.h;-Wno-sign-compare;-Wno-unused-function")

target_include_directories(PDTegraXUSB.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PD_TEGRA_FBSDKPI}/freebsd
    ${PD_TEGRA_FBSDKPI}/freebsd/arm64/nvidia/tegra210)
target_link_libraries(PDTegraXUSB.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDTegraXUSB.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDTegraXUSB.kext PROPERTY CXX_STANDARD 17)

install(TARGETS PDTegraXUSB.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
