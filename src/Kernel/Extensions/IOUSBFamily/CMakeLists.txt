add_library(IOUSBFamilyHeaders INTERFACE)
target_include_directories(IOUSBFamilyHeaders INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IOKit/usb
)

add_subdirectory(IOUSBCompositeDriver)
add_subdirectory(AppleUSBMergeNub)
add_subdirectory(IOUSBHIDDriver)

# a dependent kext's own build configures only the headers above
if(PUREDARWIN_KEXT_HEADERS_ONLY)
    return()
endif()

add_kext_bundle(IOUSBFamily KERNEL_PRIVATE INFO_PLIST Info.plist
    BUNDLE_IDENTIFIER org.puredarwin.iokit.IOUSBFamily BUNDLE_VERSION 1.0.0)

target_sources(IOUSBFamily PRIVATE
    IOUSBFamily/IOUSBBus.cpp
    IOUSBFamily/IOUSBNub.cpp
    IOUSBFamily/IOUSBController.cpp
    IOUSBFamily/IOUSBControllerV2.cpp
    IOUSBFamily/IOUSBDevice.cpp
    IOUSBFamily/IOUSBInterface.cpp
    IOUSBFamily/IOUSBPipe.cpp
    IOUSBMassStorage/IOUSBMassStorageDisk.cpp
    IOUSBMassStorage/IOUSBMassStorageClass.cpp
)

set_property(TARGET IOUSBFamily PROPERTY CXX_STANDARD 11)
target_compile_options(IOUSBFamily PRIVATE
    -Wno-implicit-int-conversion -Wno-inconsistent-missing-override
    -Wno-sign-conversion -Wno-shadow -Wno-unused-parameter)
target_include_directories(IOUSBFamily PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(IOUSBFamily PRIVATE IOUSBFamilyHeaders)
install(TARGETS IOUSBFamily DESTINATION System/Library/Extensions COMPONENT BaseSystem)

add_kext_bundle(AppleUSBEHCI.kext KERNEL_PRIVATE INFO_PLIST AppleUSBEHCI/Info.plist
    BUNDLE_IDENTIFIER com.apple.driver.AppleUSBEHCI BUNDLE_VERSION 4.1.5)
target_sources(AppleUSBEHCI.kext PRIVATE
    AppleUSBEHCI/AppleUSBEHCI.cpp
)
set_property(TARGET AppleUSBEHCI.kext PROPERTY OUTPUT_NAME AppleUSBEHCI)
set_property(TARGET AppleUSBEHCI.kext PROPERTY CXX_STANDARD 11)
target_compile_options(AppleUSBEHCI.kext PRIVATE
    -Wno-deprecated-writable-strings -Wno-inconsistent-missing-override
    -Wno-unused-parameter -Wno-sign-conversion -Wno-shadow)
target_include_directories(AppleUSBEHCI.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AppleUSBEHCI
)
target_link_libraries(AppleUSBEHCI.kext PRIVATE IOUSBFamilyHeaders IOPCIFamilyHeaders)
install(TARGETS AppleUSBEHCI.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)

add_kext_bundle(AppleUSBOHCI.kext KERNEL_PRIVATE INFO_PLIST AppleUSBOHCI/Info.plist
    BUNDLE_IDENTIFIER com.apple.driver.AppleUSBOHCI BUNDLE_VERSION 4.1.5)
target_sources(AppleUSBOHCI.kext PRIVATE
    AppleUSBOHCI/AppleUSBOHCI.cpp
)
set_property(TARGET AppleUSBOHCI.kext PROPERTY OUTPUT_NAME AppleUSBOHCI)
set_property(TARGET AppleUSBOHCI.kext PROPERTY CXX_STANDARD 11)
target_compile_options(AppleUSBOHCI.kext PRIVATE
    -Wno-deprecated-writable-strings -Wno-inconsistent-missing-override
    -Wno-unused-parameter -Wno-sign-conversion -Wno-shadow)
target_include_directories(AppleUSBOHCI.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AppleUSBOHCI
    ${CMAKE_CURRENT_SOURCE_DIR}/AppleUSBEHCI
)
target_link_libraries(AppleUSBOHCI.kext PRIVATE IOUSBFamilyHeaders IOPCIFamilyHeaders)
install(TARGETS AppleUSBOHCI.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)

add_kext_bundle(AppleUSBUHCI.kext KERNEL_PRIVATE INFO_PLIST AppleUSBUHCI/Info.plist
    BUNDLE_IDENTIFIER com.apple.driver.AppleUSBUHCI BUNDLE_VERSION 4.1.5)
target_sources(AppleUSBUHCI.kext PRIVATE
    AppleUSBUHCI/AppleUSBUHCI.cpp
)
set_property(TARGET AppleUSBUHCI.kext PROPERTY OUTPUT_NAME AppleUSBUHCI)
set_property(TARGET AppleUSBUHCI.kext PROPERTY CXX_STANDARD 11)
target_compile_options(AppleUSBUHCI.kext PRIVATE
    -Wno-deprecated-writable-strings -Wno-inconsistent-missing-override
    -Wno-unused-parameter -Wno-sign-conversion -Wno-shadow)
target_include_directories(AppleUSBUHCI.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AppleUSBUHCI
)
target_link_libraries(AppleUSBUHCI.kext PRIVATE IOUSBFamilyHeaders IOPCIFamilyHeaders)
install(TARGETS AppleUSBUHCI.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
