add_kext_bundle(PDSun60iDisplay.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDSun60iDisplay BUNDLE_VERSION 1.0.0)
set_property(TARGET PDSun60iDisplay.kext PROPERTY OUTPUT_NAME PDSun60iDisplay)

target_sources(PDSun60iDisplay.kext PRIVATE
    PDSun60iDisplay.cpp
)

target_include_directories(PDSun60iDisplay.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDSun60iDisplay.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDSun60iDisplay.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDSun60iDisplay.kext PROPERTY CXX_STANDARD 17)

install(TARGETS PDSun60iDisplay.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
