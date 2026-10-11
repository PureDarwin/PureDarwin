add_kext_bundle(PDRadeonFramebuffer.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDRadeonFramebuffer BUNDLE_VERSION 1.0.0)
set_property(TARGET PDRadeonFramebuffer.kext PROPERTY OUTPUT_NAME PDRadeonFramebuffer)

target_sources(PDRadeonFramebuffer.kext PRIVATE
    PDRadeonFramebuffer.cpp
    PDRadeonCard.cpp
    PDRadeonModes.cpp
    PDRadeonAccel.cpp
    PDRadeonGpuInit.cpp
    PDRadeonRenderClient.cpp
    RadeonAtom.cpp)

set_source_files_properties(PDRadeonAccel.cpp PROPERTIES
    COMPILE_DEFINITIONS "PDRADEON_FW_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/firmware\""
    OBJECT_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/firmware/RV730_pfp.bin;${CMAKE_CURRENT_SOURCE_DIR}/firmware/RV730_me.bin")
install(FILES firmware/LICENSE.radeon
    DESTINATION System/Library/Extensions/PDRadeonFramebuffer.kext/Contents/Resources COMPONENT BaseSystem)

target_include_directories(PDRadeonFramebuffer.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include)
target_link_libraries(PDRadeonFramebuffer.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(PDRadeonFramebuffer.kext PRIVATE __PRIVATE_SPI__)
target_compile_options(PDRadeonFramebuffer.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter
    -Wno-cast-qual -Wno-c99-designator -Wno-reorder-init-list
    -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-invalid-offsetof)
add_dependencies(PDRadeonFramebuffer.kext IOGOPFramebuffer.kext)
install(TARGETS PDRadeonFramebuffer.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
