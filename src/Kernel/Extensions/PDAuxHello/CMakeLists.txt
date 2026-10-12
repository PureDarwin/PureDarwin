# an example out-of-tree kext: never in the boot KC, linked into an auxiliary KC on the machine
add_kext_bundle(PDAuxHello.kext INFO_PLIST Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.example.AuxHello BUNDLE_VERSION 1.0.0
    MAIN_FUNCTION pd_aux_hello_start ANTIMAIN_FUNCTION pd_aux_hello_stop)
set_property(TARGET PDAuxHello.kext PROPERTY OUTPUT_NAME PDAuxHello)

target_sources(PDAuxHello.kext PRIVATE PDAuxHello.c)
target_link_libraries(PDAuxHello.kext PRIVATE xnu_kernel_private_headers)

install(TARGETS PDAuxHello.kext DESTINATION Library/Extensions COMPONENT BaseSystem)
