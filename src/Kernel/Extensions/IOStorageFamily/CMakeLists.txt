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
target_include_directories(IOStorageFamily PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/iokit
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/libkern)
target_compile_options(IOStorageFamily PRIVATE
    -include${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/libkern/libkern/OSMalloc.h
    -Wno-implicit-int-conversion -Wno-sign-conversion -Wno-deprecated-declarations)
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
    set(OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage/${file})
    add_custom_command(OUTPUT ${OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage
        COMMAND xcrun unifdef -UKERNEL -o ${OUTPUT_FILE}.tmp ${CMAKE_CURRENT_SOURCE_DIR}/include/IOKit/storage/${file} || cp ${CMAKE_CURRENT_SOURCE_DIR}/include/IOKit/storage/${file} ${OUTPUT_FILE}.tmp
        COMMAND mv ${OUTPUT_FILE}.tmp ${OUTPUT_FILE}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/IOKit/storage/${file}
        COMMENT "unifdef ${file} (fallback to copy if needed)"
        VERBATIM
    )
    target_sources(IOStorageFamilyUserHeaders PRIVATE ${OUTPUT_FILE})
endforeach()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user_include/IOKit/storage DESTINATION System/Library/Frameworks/IOKit.framework/Versions/A/Headers/IOKit COMPONENT DeveloperTools)
install(DIRECTORY include/IOKit/storage DESTINATION System/Library/Frameworks/Kernel.framework/Versions/A/Headers/IOKit COMPONENT DeveloperTools)
