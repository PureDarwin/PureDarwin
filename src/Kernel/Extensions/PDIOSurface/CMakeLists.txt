# IOSurfaceRoot service for WindowServer, interface learned by observing client calls
add_kext_bundle(PDIOSurface.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDIOSurface BUNDLE_VERSION 1.0.0)
set_property(TARGET PDIOSurface.kext PROPERTY OUTPUT_NAME PDIOSurface)

target_sources(PDIOSurface.kext PRIVATE
    IOSurfaceRoot.cpp
    PDAccelerator.cpp
    PDShaderBridge.cpp
    PDDisplay.cpp
    BootPolicy.cpp
    PDStubs.cpp
)

target_include_directories(PDIOSurface.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily)
target_link_libraries(PDIOSurface.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET PDIOSurface.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDIOSurface.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
