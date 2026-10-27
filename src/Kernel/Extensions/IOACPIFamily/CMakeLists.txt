add_library(IOACPIFamilyHeaders INTERFACE)
target_include_directories(IOACPIFamilyHeaders INTERFACE include)

# a dependent kext's own build configures only the headers above
if(PUREDARWIN_KEXT_HEADERS_ONLY)
    return()
endif()

add_kext_bundle(IOACPIFamily KERNEL_PRIVATE INFO_PLIST Info.plist BUNDLE_IDENTIFIER com.apple.iokit.IOACPIFamily BUNDLE_VERSION 1.0.0d1)
target_sources(IOACPIFamily PRIVATE
    IOACPIPlatformDevice.cpp
    IOACPIPlatformExpert.cpp
)

target_compile_options(IOACPIFamily PRIVATE -Wno-implicit-int-conversion -Wno-sign-conversion)
target_link_libraries(IOACPIFamily PRIVATE IOACPIFamilyHeaders)
install(TARGETS IOACPIFamily DESTINATION System/Library/Extensions COMPONENT BaseSystem)
