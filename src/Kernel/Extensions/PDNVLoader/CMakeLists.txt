# PDNVLoader: NVIDIA's closed resource manager object, linked into the kernel unmodified by pd_elf_rel
add_kext_bundle(PDNVLoader.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.driver.PDNVLoader BUNDLE_VERSION 1.0.0)
set_property(TARGET PDNVLoader.kext PROPERTY OUTPUT_NAME PDNVLoader)

target_sources(PDNVLoader.kext PRIVATE
    PDNVLoader.cpp
    pd_elf_rel.c
    pdnv_imports.c
    pdnv_os.c
    pdnv_pci.c
    pdnv_dev.c
    pdnv_pcidev.cpp
    pdnv_fmt.c
    pdnv_reg.c
    pdnv_trap.c
)

# the port compiles against NVIDIA's MIT interface headers as shipped, with the defines its Kbuild passes
set_source_files_properties(pdnv_os.c pdnv_pci.c pdnv_dev.c pdnv_reg.c PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/nvidia/inc"
    COMPILE_DEFINITIONS "NVRM;NV_KERNEL_INTERFACE_LAYER;NV_VERSION_STRING=\"580.196.07\"")

target_link_libraries(PDNVLoader.kext PRIVATE xnu_kernel_private_headers IOPCIFamilyHeaders)
set_property(TARGET PDNVLoader.kext PROPERTY CXX_STANDARD 14)
install(TARGETS PDNVLoader.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
