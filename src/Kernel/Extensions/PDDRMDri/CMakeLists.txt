# /dev/dri shared by every drm kext, in the boot KC so the aux KC drm kexts can link to it
add_kext_bundle(PDDRMDri.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDRMDri BUNDLE_VERSION 1.0.0
    MAIN_FUNCTION pd_dri_start ANTIMAIN_FUNCTION pd_dri_stop)
set_property(TARGET PDDRMDri.kext PROPERTY OUTPUT_NAME PDDRMDri)

target_sources(PDDRMDri.kext PRIVATE PDDRMDri.cpp)
target_include_directories(PDDRMDri.kext PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../fbsdkpi")
target_link_libraries(PDDRMDri.kext PRIVATE xnu_kernel_private_headers)

install(TARGETS PDDRMDri.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
