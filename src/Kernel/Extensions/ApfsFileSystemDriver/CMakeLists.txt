add_kext_bundle(ApfsFileSystemDriver.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.ravynos.driver.ApfsFileSystemDriver BUNDLE_VERSION 1.0.0)
set_property(TARGET ApfsFileSystemDriver.kext PROPERTY OUTPUT_NAME ApfsFileSystemDriver)

target_sources(ApfsFileSystemDriver.kext PRIVATE
    ApfsFileSystemDriver.cpp
)

target_include_directories(ApfsFileSystemDriver.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(ApfsFileSystemDriver.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(ApfsFileSystemDriver.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET ApfsFileSystemDriver.kext PROPERTY CXX_STANDARD 14)
install(TARGETS ApfsFileSystemDriver.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
