add_kext_bundle(PDBcm2835SD.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kext.PDBcm2835SD BUNDLE_VERSION 1.0.0)
set_property(TARGET PDBcm2835SD.kext PROPERTY OUTPUT_NAME PDBcm2835SD)

target_sources(PDBcm2835SD.kext PRIVATE
    PDBcm2835SD.cpp
    PDBcm2835SDDisk.cpp
    bcm2835_emmc_bringup.c
)

set_source_files_properties(bcm2835_emmc_bringup.c PROPERTIES
    COMPILE_OPTIONS "-std=c11;-ffreestanding")

target_include_directories(PDBcm2835SD.kext PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../IOStorageFamily/include)
target_link_libraries(PDBcm2835SD.kext PRIVATE xnu_kernel_private_headers)
target_compile_options(PDBcm2835SD.kext PRIVATE
    -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-shorten-64-to-32)
set_property(TARGET PDBcm2835SD.kext PROPERTY CXX_STANDARD 14)

install(TARGETS PDBcm2835SD.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
