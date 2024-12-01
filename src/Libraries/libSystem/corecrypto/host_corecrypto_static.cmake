add_library(host_corecrypto_static STATIC)
target_include_directories(host_corecrypto_static PUBLIC include)
target_include_directories(host_corecrypto_static PRIVATE private)

add_library(host_corecrypto_headers INTERFACE)
target_include_directories(host_corecrypto_headers INTERFACE include)

add_library(host_corecrypto_private_headers INTERFACE)
target_include_directories(host_corecrypto_private_headers INTERFACE include)

target_sources(host_corecrypto_static PRIVATE
    src/cc.c
    src/ccdigest.c
    src/ccmd2.c
    src/ccmd4.c
    src/ccmd5.c
    src/ccder.c
    src/ccec.c
    src/ccdh.c
    src/ccdh_gp.c
    src/ccaes.c
    src/ccsha2xx.c
    src/ccsha3xx.c
    src/cczp.c
    src/ccsha1.c
    src/ccrsa.c
    src/ccrng.c
    src/ccrng_system.c
    src/ccrc4.c
    src/ccn.c
    src/ccmode.c
    src/ccdes.c
    src/ccrsa_priv.c
    src/cccast.c
    src/ccrc2.c
    src/ccblowfish.c
    src/ccnistkdf.c
    src/ccz.c
    src/cccmac.c
    src/ccripemd.c
    src/cchkdf.c
    src/cchmac.c
    src/ccpad.c
    src/ccpbkdf2.c
    src/ccrc4.c
    src/ccansikdf.c
    src/ccecies.c
    src/ccrng_pbkdf2.c
    src/ccec_projective_point.c
    src/ccec_points.c
    src/ccn_extra.c
    src/cczp_extra.c
    src/ccgcm.c
    src/cch2c.c
    src/ccsrp.c
    src/ccwrap_priv.c
    src/cc_priv.c
    src/ccec25519.c
    src/cccbc.c
    src/ccccm.c
    src/cccfb.c
    src/cccfb8.c
    src/ccchacha20poly1305.c
    src/ccctr.c
    src/ccofb.c
    src/ccxts.c
    src/ccckg.c
    cc_user_stub.c
)
