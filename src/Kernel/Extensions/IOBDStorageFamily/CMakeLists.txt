add_kext_bundle(IOBDStorageFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IOBDStorageFamily BUNDLE_VERSION 1.8)

set_property(TARGET IOBDStorageFamily.kext PROPERTY OUTPUT_NAME IOBDStorageFamily)
target_sources(IOBDStorageFamily.kext PRIVATE
    IOBDBlockStorageDevice.cpp
    IOBDBlockStorageDriver.cpp
    IOBDMedia.cpp
    IOBDMediaBSDClient.cpp
)
target_include_directories(IOBDStorageFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IODVDStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOCDStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(IOBDStorageFamily.kext PRIVATE
    xnu_kernel_private_headers IOStorageFamilyHeaders)
add_dependencies(IOBDStorageFamily.kext
    IOCDStorageFamily.kext IODVDStorageFamily.kext)
target_compile_options(IOBDStorageFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion
    -Wno-deprecated-declarations -Wno-unused-variable)
set_property(TARGET IOBDStorageFamily.kext PROPERTY CXX_STANDARD 17)
install(TARGETS IOBDStorageFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
