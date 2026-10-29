add_library(host_commoncrypto_static STATIC)
target_include_directories(host_commoncrypto_static PUBLIC include)
target_include_directories(host_commoncrypto_static PRIVATE include/Private include/Private/CommonCrypto libcn lib)
# See host_corecrypto_static.cmake for why: keeps this native host build
# tool target on genuine Darwin SDK headers only, no glibc leakage.
if(DEFINED ENV{NIX_NATIVE_DARWIN_HEADER_FLAGS})
    separate_arguments(_nix_darwin_hdr_flags UNIX_COMMAND "$ENV{NIX_NATIVE_DARWIN_HEADER_FLAGS}")
    target_compile_options(host_commoncrypto_static PRIVATE -nostdlibinc ${_nix_darwin_hdr_flags})
    target_include_directories(host_commoncrypto_static PRIVATE $ENV{NIX_NATIVE_DARWIN_HEADER_DIRS})
endif()

add_library(host_commoncrypto_headers INTERFACE)
target_include_directories(host_commoncrypto_headers INTERFACE include)

add_library(host_commoncrypto_private_headers INTERFACE)
target_include_directories(host_commoncrypto_private_headers INTERFACE include/Private)

target_link_libraries(host_commoncrypto_static PRIVATE
    host_corecrypto_static
    host_commoncrypto_headers
    host_commoncrypto_private_headers
)

target_sources(host_commoncrypto_static PRIVATE
    libcn/pd_cc_digest_bridge.c
    libcn/adler32.c
    libcn/crc8.c
    libcn/crc8-icode.c
    libcn/crc8-itu.c
    libcn/crc8-rohc.c
    libcn/crc8-wcdma.c
    libcn/crc16-a.c
    libcn/crc16-b.c
    libcn/crc16-ccitt-false.c
    libcn/crc16-ccitt-true.c
    libcn/crc16-dect-r.c
    libcn/crc16-dect-x.c
    libcn/crc16-icode.c
    libcn/crc16-usb.c
    libcn/crc16-verifone.c
    libcn/crc16-xmodem.c
    libcn/crc16.c
    libcn/crc32-bzip2.c
    libcn/crc32-castagnoli.c
    libcn/crc32-mpeg-2.c
    libcn/crc32-posix.c
    libcn/crc32-xfer.c
    libcn/crc32.c
    libcn/crc64-ecma-182.c
    lib/ccGlobals.c
    lib/ccDispatch.c
    lib/CommonCryptorChaCha20Poly1305.c
    lib/CommonKeyDerivationSPI.c
    lib/CommonCMAC.c
    lib/CommonCryptor.c
    lib/CommonDigest.c
    lib/CommonCryptorGCM.c
    lib/CommonHMAC.c
    lib/CommonKeyDerivation.c
    lib/CommonRandom.c
    lib/CommonSymmetricKeywrap.c
    lib/corecryptoSymmetricBridge.c
    lib/CommonCryptorDES.c
    libcn/CommonCRC.c
    libcn/CommonBaseXX.c
    libcn/CommonBuffering.c
    libcn/gen_std_crc_table.c
    libcn/normal_crc.c
    libcn/reverse_crc.c
    libcn/reverse_poly.c
)

# the asymmetric half (RSA, EC, DH, BigNum) needs corecrypto APIs the
# external corecrypto does not have yet, and libSystem exports none of it, so build it only
# against the old in-tree corecrypto
if(NOT PD_CORECRYPTO_DIR)
    target_sources(host_commoncrypto_static PRIVATE
        lib/CommonBigNum.c
        lib/CommonDH.c
        lib/CommonECCryptor.c
        lib/CommonRSACryptor.c
        lib/CommonCollabKeyGen.c
    )
endif()
