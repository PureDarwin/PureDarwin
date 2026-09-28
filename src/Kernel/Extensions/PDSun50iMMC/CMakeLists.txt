add_kext_bundle(PDSun50iMMC.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDSun50iMMC BUNDLE_VERSION 1.0.0)
set_property(TARGET PDSun50iMMC.kext PROPERTY OUTPUT_NAME PDSun50iMMC)

target_sources(PDSun50iMMC.kext PRIVATE
    PDSun50iMMC.cpp
    PDSun50iMMCDisk.cpp
)

target_include_directories(PDSun50iMMC.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDSun50iMMC.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDSun50iMMC.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDSun50iMMC.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDSun50iMMC.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
