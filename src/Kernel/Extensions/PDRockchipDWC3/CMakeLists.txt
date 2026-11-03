add_kext_bundle(PDRockchipDWC3.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDRockchipDWC3 BUNDLE_VERSION 1.0.0)
set_property(TARGET PDRockchipDWC3.kext PROPERTY OUTPUT_NAME PDRockchipDWC3)

target_sources(PDRockchipDWC3.kext PRIVATE
    PDRockchipDWC3.cpp
    PDRockchipDWC2.cpp
    PDGadgetACM.cpp
    PDGadgetADB.cpp
)

target_include_directories(PDRockchipDWC3.kext PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDRockchipDWC3.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDRockchipDWC3.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDRockchipDWC3.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDRockchipDWC3.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
