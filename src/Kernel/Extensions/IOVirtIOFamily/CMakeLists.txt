# IOVirtIOFamily: shared virtio-1.0 PCI transport (capability walking,
# device handshake, split virtqueues, polling-only completion) used by
# IOVirtIOGPU/IOVirtIOBlk/IOVirtIONet/IOVirtIOInput/IOVirtIOSnd. Pure
# code, no IOKitPersonalities - IOVirtIOTransport is a composed member,
# not an IOService (each concrete driver already subclasses its own
# Apple family base class).
add_kext_bundle(IOVirtIOFamily.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.iokit.IOVirtIOFamily BUNDLE_VERSION 1.0.0)
set_property(TARGET IOVirtIOFamily.kext PROPERTY OUTPUT_NAME IOVirtIOFamily)

target_sources(IOVirtIOFamily.kext PRIVATE
    IOVirtIOTransport.cpp
)

target_include_directories(IOVirtIOFamily.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include)
target_link_libraries(IOVirtIOFamily.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(IOVirtIOFamily.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(IOVirtIOFamily.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET IOVirtIOFamily.kext PROPERTY CXX_STANDARD 14)
install(TARGETS IOVirtIOFamily.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
