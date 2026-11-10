add_kext_bundle(PDAdreno.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDAdreno BUNDLE_VERSION 1.0.0)
set_property(TARGET PDAdreno.kext PROPERTY OUTPUT_NAME PDAdreno)

target_sources(PDAdreno.kext PRIVATE
    PDAdreno.cpp
    PDAdrenoSmmu.cpp
    PDAdrenoMsm8916.cpp
)

target_include_directories(PDAdreno.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PDAdreno.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDAdreno.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDAdreno.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDAdreno.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
