add_kext_bundle(IOIntelGen9Framebuffer.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.IOIntelGen9Framebuffer BUNDLE_VERSION 1.0.0)
set_property(TARGET IOIntelGen9Framebuffer.kext PROPERTY OUTPUT_NAME IOIntelGen9Framebuffer)

# lil sources (mirrors lil/meson.build: generic + ivy_bridge + kaby_lake + vbt)
set(LIL_SOURCES
    lil/src/avi.cpp lil/src/edid.cpp lil/src/gmbus.cpp lil/src/intel.cpp
    lil/src/lvds.cpp lil/src/pch.cpp
    lil/src/ivy_bridge/crtc.cpp lil/src/ivy_bridge/gtt.cpp
    lil/src/ivy_bridge/interrupt.cpp lil/src/ivy_bridge/ivb.cpp
    lil/src/ivy_bridge/plane.cpp
    lil/src/kaby_lake/brightness.cpp lil/src/kaby_lake/cdclk.cpp
    lil/src/kaby_lake/crtc.cpp lil/src/kaby_lake/ddi-translations.cpp
    lil/src/kaby_lake/ddi.cpp lil/src/kaby_lake/dp-aux.cpp lil/src/kaby_lake/dp.cpp
    lil/src/kaby_lake/edp.cpp lil/src/kaby_lake/encoder.cpp lil/src/kaby_lake/gtt.cpp
    lil/src/kaby_lake/hdmi.cpp lil/src/kaby_lake/hpd.cpp lil/src/kaby_lake/kbl.cpp
    lil/src/kaby_lake/link-training.cpp lil/src/kaby_lake/pci.cpp
    lil/src/kaby_lake/pcode.cpp lil/src/kaby_lake/pipe.cpp lil/src/kaby_lake/plane.cpp
    lil/src/kaby_lake/pll.cpp lil/src/kaby_lake/setup.cpp
    lil/src/kaby_lake/transcoder.cpp lil/src/kaby_lake/watermarks.cpp
    lil/src/gemini_lake/glk.cpp lil/src/gemini_lake/hdmi.cpp
    lil/src/gemini_lake/phy.cpp lil/src/gemini_lake/pll.cpp
    lil/src/gemini_lake/dp.cpp
    lil/src/vbt/parsing.cpp lil/src/vbt/vbt.cpp)

target_sources(IOIntelGen9Framebuffer.kext PRIVATE
    IOIntelGen9Framebuffer.cpp
    lil_imports.cpp
    ${LIL_SOURCES})

target_include_directories(IOIntelGen9Framebuffer.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/lil
    ${CMAKE_CURRENT_SOURCE_DIR}/lil/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOGraphicsFamily
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOHIDFamily/IOHIDSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include)
target_link_libraries(IOIntelGen9Framebuffer.kext PRIVATE xnu_kernel_private_headers)
target_compile_definitions(IOIntelGen9Framebuffer.kext PRIVATE __PRIVATE_SPI__ __STDC_HOSTED__=0)

# lil is modern C++ (concepts/CTAD/requires): build the whole kext at gnu++2b.
# Freestanding kext: no exceptions/RTTI. lil's warnings are noisy - quiet them.
target_compile_options(IOIntelGen9Framebuffer.kext PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++2b>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter
    -Wno-cast-qual -Wno-c99-designator -Wno-reorder-init-list
    -Wno-gnu-anonymous-struct -Wno-nested-anon-types)
add_dependencies(IOIntelGen9Framebuffer.kext IOGOPFramebuffer.kext)
install(TARGETS IOIntelGen9Framebuffer.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
