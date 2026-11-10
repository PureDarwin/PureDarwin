add_kext_bundle(PDTegraDisplay.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDTegraDisplay BUNDLE_VERSION 1.0.0)
set_property(TARGET PDTegraDisplay.kext PROPERTY OUTPUT_NAME PDTegraDisplay)

# FreeBSD's tegra210 car and pmc excerpts and NetBSD's polled i2c, shared with PDDRMnouveau
set(PD_TEGRA_FBSDKPI ${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi)
set(PD_TEGRA_BSD_SOURCES
    ${PD_TEGRA_FBSDKPI}/freebsd/arm64/nvidia/tegra210/tegra210_gpu.c
    ${PD_TEGRA_FBSDKPI}/netbsd/arch/arm/nvidia/tegra_i2c_poll.c)

target_sources(PDTegraDisplay.kext PRIVATE
    PDTegraDisplay.cpp
    ${PD_TEGRA_BSD_SOURCES}
)
set_source_files_properties(${PD_TEGRA_BSD_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/pd_tegra_bsd_shim.h;-Wno-sign-compare;-Wno-unused-function")

target_include_directories(PDTegraDisplay.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${PD_TEGRA_FBSDKPI}/freebsd
    ${PD_TEGRA_FBSDKPI}/freebsd/arm64/nvidia/tegra210
    ${PD_TEGRA_FBSDKPI}/netbsd/arch/arm/nvidia)
target_link_libraries(PDTegraDisplay.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDTegraDisplay.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDTegraDisplay.kext PROPERTY CXX_STANDARD 17)

install(TARGETS PDTegraDisplay.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
