add_kext_bundle(IOVirtIOBlock.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.IOVirtIOBlock BUNDLE_VERSION 1.0.0)
set_property(TARGET IOVirtIOBlock.kext PROPERTY OUTPUT_NAME IOVirtIOBlock)

target_sources(IOVirtIOBlock.kext PRIVATE
    IOVirtIOBlock.cpp
    IOVirtIOBlockDisk.cpp
)

target_include_directories(IOVirtIOBlock.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOVirtIOFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../../xnu/EXTERNAL_HEADERS)
target_link_libraries(IOVirtIOBlock.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(IOVirtIOBlock.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(IOVirtIOBlock.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET IOVirtIOBlock.kext PROPERTY CXX_STANDARD 14)
add_dependencies(IOVirtIOBlock.kext IOVirtIOFamily.kext)
install(TARGETS IOVirtIOBlock.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
