# RavynHDAudio: Intel HD Audio controller + codec driver, playback-only.
# The device half is an IOAudioDevice; RavynHDAudioEngine is the IOAudioEngine
# that IOAudioFamily drives. The legacy /dev/dsp0 path is still present for
# fbdoom/pcmplay until there is a userland audio API.
add_kext_bundle(RavynHDAudio.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER com.ravynos.driver.RavynHDAudio BUNDLE_VERSION 1.0.0)
set_property(TARGET RavynHDAudio.kext PROPERTY OUTPUT_NAME RavynHDAudio)

target_sources(RavynHDAudio.kext PRIVATE
    RavynHDAudio.cpp
    RavynHDAudioEngine.cpp
)

target_include_directories(RavynHDAudio.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOPCIFamily/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOAudioFamily)
target_link_libraries(RavynHDAudio.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(RavynHDAudio.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32
    -Wno-unused-variable -Wno-unused-function -Wno-cast-qual)
set_property(TARGET RavynHDAudio.kext PROPERTY CXX_STANDARD 14)
install(TARGETS RavynHDAudio.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
