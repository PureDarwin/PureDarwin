add_kext_bundle(PDPowerVR.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDPowerVR BUNDLE_VERSION 1.0.0)
set_property(TARGET PDPowerVR.kext PROPERTY OUTPUT_NAME PDPowerVR)

target_sources(PDPowerVR.kext PRIVATE
    PDPowerVR.cpp
    PDPowerVRUserClient.cpp
    pvr_fw_boot.c
    pvr_ccb.c
    pvr_mmu.c
    pvr_vm.c
    pvr_gem.cpp
    pvr_linux.cpp
    pvr_device_query.c
    pvr_stream.c
    pvr_stream_defs.c
    pvr_cccb.c
    pvr_context.c
    pvr_queue.c
    pvr_job.c
    pvr_sync.c
    pvr_hwrt.c
    pvr_free_list.c
    pvr_drv.c
    pvr_selftest.c
    pvr_device_info.c
    pvr_fw_blob.c
)

# the firmware image goes in as bytes, the collection has no room for bundle resources
set(PDPOWERVR_FW ${CMAKE_CURRENT_SOURCE_DIR}/firmware/rogue_36.56.104.183_v1.fw)
set_source_files_properties(pvr_fw_blob.c PROPERTIES
    COMPILE_DEFINITIONS "PDPOWERVR_FW_PATH=\"${PDPOWERVR_FW}\""
    OBJECT_DEPENDS ${PDPOWERVR_FW})

# imagination's interface headers stay unmodified, include/linux supplies the linux types they use
target_include_directories(PDPowerVR.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imagination
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../PDSun60iDisplay)
target_link_libraries(PDPowerVR.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDPowerVR.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    $<$<COMPILE_LANGUAGE:C>:-std=gnu11>)
set_property(TARGET PDPowerVR.kext PROPERTY CXX_STANDARD 17)

install(TARGETS PDPowerVR.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
install(FILES firmware/LICENSE.powervr
    DESTINATION System/Library/Extensions/PDPowerVR.kext/Contents/Resources COMPONENT BaseSystem)
