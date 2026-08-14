add_kext_bundle(IOCDStorageFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IOCDStorageFamily BUNDLE_VERSION 1.8)

set_property(TARGET IOCDStorageFamily.kext PROPERTY OUTPUT_NAME IOCDStorageFamily)
target_sources(IOCDStorageFamily.kext PRIVATE
    IOCDBlockStorageDevice.cpp
    IOCDBlockStorageDriver.cpp
    IOCDMedia.cpp
    IOCDMediaBSDClient.cpp
    IOCDPartitionScheme.cpp
)
target_include_directories(IOCDStorageFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(IOCDStorageFamily.kext PRIVATE
    xnu_kernel_private_headers IOStorageFamilyHeaders)
target_compile_options(IOCDStorageFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion
    -Wno-deprecated-declarations -Wno-unused-variable)
set_property(TARGET IOCDStorageFamily.kext PROPERTY CXX_STANDARD 17)
install(TARGETS IOCDStorageFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
