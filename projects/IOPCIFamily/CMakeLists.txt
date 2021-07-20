add_kext_bundle(IOPCIFamily INFO_PLIST Info.plist BUNDLE_IDENTIFIER com.apple.iokit.IOPCIFamily BUNDLE_VERSION 2.4.1)
target_sources(IOPCIFamily PRIVATE
    IOPCIBridge.cpp
    IOPCIConfigurator.cpp
    IOPCIDevice.cpp
    IOPCIDeviceI386.cpp
    IOPCIDeviceMappedIO.cpp
    IOPCIMessagedInterruptController.cpp
    IOPCIRange.cpp
)

target_link_libraries(IOPCIFamily PRIVATE IOPCIFamilyHeaders)
install(TARGETS IOPCIFamily DESTINATION /System/Library/Extensions COMPONENT BaseSystem)

add_library(IOPCIFamilyHeaders INTERFACE)
target_include_directories(IOPCIFamilyHeaders INTERFACE include)

add_subdirectory(ApplePIODMA)
