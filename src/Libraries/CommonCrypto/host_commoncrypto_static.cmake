add_library(host_commoncrypto_static STATIC)
target_include_directories(host_commoncrypto_static PUBLIC include)
target_include_directories(host_commoncrypto_static PRIVATE include/Private libcn lib)

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
    lib/CommonBigNum.c
    lib/CommonCMAC.c
    lib/CommonCollabKeyGen.c
    lib/CommonCryptor.c
    lib/CommonDH.c
    lib/CommonDigest.c
    lib/CommonECCryptor.c
    lib/CommonCryptorGCM.c
    lib/CommonHMAC.c
    lib/CommonKeyDerivation.c
    lib/CommonRandom.c
    lib/CommonRSACryptor.c
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
