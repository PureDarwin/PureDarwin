# DiskImages2's AppleDiskImagesController for RAM images, as diskimagescontroller and diskimagesiod use it
add_kext_bundle(PDDiskImages2.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDDiskImages2 BUNDLE_VERSION 1.0.0)
set_property(TARGET PDDiskImages2.kext PROPERTY OUTPUT_NAME PDDiskImages2)

target_sources(PDDiskImages2.kext PRIVATE
    PDDiskImages2.cpp
)

target_include_directories(PDDiskImages2.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(PDDiskImages2.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET PDDiskImages2.kext PROPERTY CXX_STANDARD 14)
add_dependencies(PDDiskImages2.kext IOStorageFamily)
install(TARGETS PDDiskImages2.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
