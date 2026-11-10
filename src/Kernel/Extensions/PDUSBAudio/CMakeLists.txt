# PDUSBAudio: USB Audio Class 1.0 output, from the USB-IF Audio Device Class 1.0 specification
add_kext_bundle(PDUSBAudio.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDUSBAudio BUNDLE_VERSION 1.0.0)
set_property(TARGET PDUSBAudio.kext PROPERTY OUTPUT_NAME PDUSBAudio)

target_sources(PDUSBAudio.kext PRIVATE
    PDUSBAudio.cpp
    PDUSBAudioEngine.cpp
)

target_include_directories(PDUSBAudio.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOAudioFamily)
target_link_libraries(PDUSBAudio.kext PRIVATE IOUSBFamilyHeaders xnu_kernel_private_headers)
target_compile_definitions(PDUSBAudio.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDUSBAudio.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual -Wno-deprecated-declarations)
set_property(TARGET PDUSBAudio.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDUSBAudio.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
