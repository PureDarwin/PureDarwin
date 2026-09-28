# IOWatchdog service for watchdogd, interface learned by observing client calls
add_kext_bundle(PDWatchdog.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDWatchdog BUNDLE_VERSION 1.0.0)
set_property(TARGET PDWatchdog.kext PROPERTY OUTPUT_NAME PDWatchdog)

target_sources(PDWatchdog.kext PRIVATE
    PDWatchdog.cpp
)

target_link_libraries(PDWatchdog.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET PDWatchdog.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDWatchdog.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
