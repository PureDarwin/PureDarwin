add_kext_bundle(IODVDStorageFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IODVDStorageFamily BUNDLE_VERSION 1.8)

set_property(TARGET IODVDStorageFamily.kext PROPERTY OUTPUT_NAME IODVDStorageFamily)
target_sources(IODVDStorageFamily.kext PRIVATE
    IODVDBlockStorageDevice.cpp
    IODVDBlockStorageDriver.cpp
    IODVDMedia.cpp
    IODVDMediaBSDClient.cpp
)
target_include_directories(IODVDStorageFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOCDStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(IODVDStorageFamily.kext PRIVATE
    xnu_kernel_private_headers IOStorageFamilyHeaders)
add_dependencies(IODVDStorageFamily.kext IOCDStorageFamily.kext)
target_compile_options(IODVDStorageFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion
    -Wno-deprecated-declarations -Wno-unused-variable)
set_property(TARGET IODVDStorageFamily.kext PROPERTY CXX_STANDARD 17)
install(TARGETS IODVDStorageFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
