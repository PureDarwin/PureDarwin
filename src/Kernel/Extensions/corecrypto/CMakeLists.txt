add_kext_bundle(corecrypto.kext INFO_PLIST kext/Info.plist KERNEL_PRIVATE
    BUNDLE_IDENTIFIER org.puredarwin.kec.corecrypto BUNDLE_VERSION 1.0d1
    MAIN_FUNCTION cc_kext_start ANTIMAIN_FUNCTION cc_kext_stop
)
set_property(TARGET corecrypto.kext PROPERTY OUTPUT_NAME corecrypto)

target_sources(corecrypto.kext PRIVATE
    cc_clear.c
    cc_cmp_safe.c
    cc_debug.c
    cc_digest.c

    algorithms/aes_cbc.c
    algorithms/aes_ecb.c
    algorithms/cckprng.c
    algorithms/md5_impl.cc
    algorithms/md5.c
    algorithms/pdcrypto_digest_final.c
    algorithms/pdcrypto_digest_init.c
    algorithms/pdcrypto_digest_update.c
    algorithms/pdcrypto_dummy.c
    algorithms/sha1_impl.cc
    algorithms/sha256.cc
    algorithms/sha512.cc
    algorithms/yarrow/comp.c
    algorithms/yarrow/prng.c
    algorithms/yarrow/sha1mod.c
    algorithms/yarrow/smf.c

    kext/cc_functions.c
    kext/cc_kext_main.c
)

target_include_directories(corecrypto.kext PRIVATE include)
target_link_libraries(corecrypto.kext PRIVATE xnu_kernel_private_headers)
set_property(TARGET corecrypto.kext PROPERTY CXX_STANDARD 11)
install(TARGETS corecrypto.kext DESTINATION System/Library/Extensions COMPONENT BaseSystem)
