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
install(TARGETS IOStorageFamily DESTINATION /System/Library/Extensions COMPONENT BaseSystem)

# FIXME: Re-enable these when we have our own tools for handling image resource files.
# add_bundle_iconset(IOStorageFamily IconResources/External.iconset)
# add_bundle_iconset(IOStorageFamily IconResources/Internal.iconset)
# add_bundle_iconset(IOStorageFamily IconResources/Removable.iconset)
# add_bundle_resources(IOStorageFamily en.lproj/InfoPlist.strings)


add_library(IOStorageFamilyHeaders INTERFACE)
target_include_directories(IOStorageFamilyHeaders INTERFACE include)


# The files in user_include were created by running the following command:
#
# for FILE in $(ls include/IOKit/storage); do
#   unifdef -UKERNEL -o user_include/IOKit/storage/$(basename $FILE) $FILE
# done
#
# Be sure to rerun this command if the contents of the kernel headers change.
# Also, IOBlockStoragePerfControlExports.h is not exported to user space.

add_library(IOStorageFamilyUserHeaders INTERFACE)
target_include_directories(IOStorageFamilyUserHeaders INTERFACE user_include)

install(DIRECTORY user_include/IOKit/storage DESTINATION /System/Library/Frameworks/IOKit.framework/Versions/A/Headers/IOKit COMPONENT DeveloperTools)
install(DIRECTORY include/IOKit/storage DESTINATION /System/Library/Frameworks/Kernel.framework/Versions/A/Headers/IOKit COMPONENT DeveloperTools)
