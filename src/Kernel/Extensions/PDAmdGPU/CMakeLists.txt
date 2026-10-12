add_kext_bundle(PDAmdGPU.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDAmdGPU BUNDLE_VERSION 1.0.0)
set_property(TARGET PDAmdGPU.kext PROPERTY OUTPUT_NAME PDAmdGPU)

target_sources(PDAmdGPU.kext PRIVATE
    PDAmdGPU.cpp
    AmdDiscovery.cpp
    PDAmdPsp.cpp
    PDAmdSdma.cpp
    PDAmdSmu.cpp
    PDAmdGfx.cpp
    PDAmdGmc.cpp
    PDAmdKiq.cpp
    PDAmdIh.cpp
    PDAmdDmub.cpp
    PDAmdClk.cpp
    PDAmdDisplay.cpp
    PDAmdAtom.cpp
    PDAmdFramebuffer.cpp
    PDAmdFirmware.cpp
    atom/atom.c)

# every chip's firmware is embedded with .incbin, a changed file has to rebuild the object
file(GLOB PDAMDGPU_FIRMWARE ${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.bin)
set_source_files_properties(PDAmdFirmware.cpp PROPERTIES
    COMPILE_DEFINITIONS "PDAMDGPU_FW_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/firmware\""
    OBJECT_DEPENDS "${PDAMDGPU_FIRMWARE}")
install(FILES firmware/LICENSE.amdgpu
    DESTINATION System/Library/Extensions/PDAmdGPU.kext/Contents/Resources COMPONENT BaseSystem)

target_include_directories(PDAmdGPU.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include)
target_link_libraries(PDAmdGPU.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(PDAmdGPU.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDAmdGPU.kext PRIVATE -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
install(TARGETS PDAmdGPU.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
