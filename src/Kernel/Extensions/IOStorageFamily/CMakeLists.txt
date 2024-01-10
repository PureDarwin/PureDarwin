add_kext_bundle(IOStorageFamily INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IOStorageFamily BUNDLE_VERSION 2.1)

target_sources(IOStorageFamily PRIVATE
    IOAppleLabelScheme.cpp
    IOApplePartitionScheme.cpp
    IOBlockStorageDevice.cpp
    IOBlockStorageDriver.cpp
    IOFDiskPartitionScheme.cpp
    IOFilterScheme.cpp
    IOGUIDPartitionScheme.cpp
    IOMedia.cpp
    IOMediaBSDClient.cpp
    IOPartitionScheme.cpp
    IOStorage.cpp
)
target_include_directories(IOStorageFamily PRIVATE include)
target_compile_options(IOStorageFamily PRIVATE -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-deprecated-declarations)
set_property(TARGET IOStorageFamily PROPERTY CXX_STANDARD 17)
install(TARGETS IOStorageFamily DESTINATION System/Library/Extensions COMPONENT BaseSystem)

# FIXME: Re-enable these when we have our own tools for handling image resource files.
# add_bundle_iconset(IOStorageFamily IconResources/External.iconset)
# add_bundle_iconset(IOStorageFamily IconResources/Internal.iconset)
# add_bundle_iconset(IOStorageFamily IconResources/Removable.iconset)
# add_bundle_resources(IOStorageFamily en.lproj/InfoPlist.strings)


add_library(IOStorageFamilyHeaders INTERFACE)
target_include_directories(IOStorageFamilyHeaders INTERFACE include)


add_darwin_object_library(IOStorageFamilyUserHeaders)
target_sources(IOStorageFamilyUserHeaders PRIVATE user_stub.c)
target_include_directories(IOStorageFamilyUserHeaders INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/user_include)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage)

set(HEADERS
    IOAppleLabelScheme.h
    IOApplePartitionScheme.h
    IOBlockStorageDevice.h
    IOBlockStorageDriver.h
    IOFDiskPartitionScheme.h
    IOFilterScheme.h
    IOGUIDPartitionScheme.h
    IOMedia.h
    IOMediaBSDClient.h
    IOPartitionScheme.h
    IOStorage.h
    IOStorageCardCharacteristics.h
    IOStorageControllerCharacteristics.h
    IOStorageDeviceCharacteristics.h
    IOStorageProtocolCharacteristics.h
)
foreach(file IN LISTS HEADERS)
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage/${file}
        COMMAND $<TARGET_FILE:unifdef> -UKERNEL -o ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage/${file} ${CMAKE_CURRENT_SOURCE_DIR}/include/IOKit/storage/${file}
        DEPENDS unifdef ${CMAKE_CURRENT_SOURCE_DIR}/include/IOKit/storage/${file}
        COMMENT "unifdef ${file}" VERBATIM
    )
    target_sources(IOStorageFamilyUserHeaders PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage/${file})
endforeach()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage DESTINATION System/Library/Frameworks/IOKit.framework/Versions/A/Headers/IOKit COMPONENT DeveloperTools)
install(DIRECTORY include/IOKit/storage DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A/Headers/IOKit COMPONENT DeveloperTools)
