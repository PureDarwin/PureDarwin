add_kext_bundle(IOATAFamily INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.apple.iokit.IOATAFamily BUNDLE_VERSION 2.5.3)

target_sources(IOATAFamily PRIVATE
    ATADeviceNub.cpp
    ATATimerEventSource.cpp
    IOATABusCommand.cpp
    IOATABusInfo.cpp
    IOATACommand.cpp
    IOATAController.cpp
    IOATADevConfig.cpp
    IOATADevice.cpp
    IOATARegI386.cpp
)
target_include_directories(IOATAFamily PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} include/IOKit/ata private_include/IOKit/ata)
target_link_libraries(IOATAFamily PRIVATE IOStorageFamilyHeaders IOATAFamilyHeaders)
target_compile_options(IOATAFamily PRIVATE -Wno-implicit-int-conversion -Wno-sign-conversion)

add_library(IOATAFamilyHeaders INTERFACE)
target_include_directories(IOATAFamilyHeaders INTERFACE include)
add_library(IOATAFamilyPrivateHeaders INTERFACE)
target_include_directories(IOATAFamilyPrivateHeaders INTERFACE private_include)

install(TARGETS IOATAFamily DESTINATION System/Library/Extensions COMPONENT BaseSystem)
