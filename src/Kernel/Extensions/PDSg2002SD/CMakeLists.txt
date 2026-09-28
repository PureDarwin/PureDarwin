add_kext_bundle(PDSg2002SD.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDSg2002SD BUNDLE_VERSION 1.0.0)
set_property(TARGET PDSg2002SD.kext PROPERTY OUTPUT_NAME PDSg2002SD)

target_sources(PDSg2002SD.kext PRIVATE
    PDSg2002SD.cpp
    PDSg2002SDDisk.cpp
)

target_include_directories(PDSg2002SD.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDSg2002SD.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDSg2002SD.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDSg2002SD.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDSg2002SD.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
