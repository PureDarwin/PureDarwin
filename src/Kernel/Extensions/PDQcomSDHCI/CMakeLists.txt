add_kext_bundle(PDQcomSDHCI.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDQcomSDHCI BUNDLE_VERSION 1.0.0)
set_property(TARGET PDQcomSDHCI.kext PROPERTY OUTPUT_NAME PDQcomSDHCI)

target_sources(PDQcomSDHCI.kext PRIVATE
    PDQcomSDHCI.cpp
    PDQcomSDHCIDisk.cpp
)

target_include_directories(PDQcomSDHCI.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDQcomSDHCI.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDQcomSDHCI.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDQcomSDHCI.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDQcomSDHCI.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
