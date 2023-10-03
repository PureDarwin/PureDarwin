add_darwin_static_library(corecrypto_static)
target_sources(corecrypto_static PRIVATE
    $<TARGET_OBJECTS:corecrypto_shared>
    ccstub.c
)
