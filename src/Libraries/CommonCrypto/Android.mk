LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE		:= libcommoncrypto
LOCAL_C_INCLUDES := $(LOCAL_PATH)/android/include \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/include/Private \
    $(LOCAL_PATH)/libcn \
    $(LOCAL_PATH)/lib \
    $(LOCAL_PATH)/CCRegression/util
LOCAL_SRC_FILES := \
    ./lib/CommonDigest.c \
    ./lib/CommonRSACryptor.c \
    ./lib/CommonHMAC.c \
    ./lib/corecryptoSymmetricBridge.c \
    ./lib/CommonCryptorGCM.c \
    ./lib/CommonKeyDerivation.c \
    ./lib/CommonDH.c \
    ./lib/CommonCMAC.c \
    ./lib/ccDispatch.c \
    ./lib/dummy.c \
    ./lib/CommonCryptorDES.c \
    ./lib/CommonKeyDerivationSPI.c \
    ./lib/CommonSymmetricKeywrap.c \
    ./lib/CommonECCryptor.c \
    ./lib/ccGlobals.c \
    ./lib/CommonCryptor.c \
    ./lib/CommonCollabKeyGen.c \
    ./lib/CommonBigNum.c \
    ./lib/timingsafe_bcmp.c \
    ./lib/CommonRandom.c \
    ./libcn/crc8-rohc.c \
    ./libcn/crc8-wcdma.c \
    ./libcn/crc8-icode.c \
    ./libcn/crc32-mpeg-2.c \
    ./libcn/crc16-b.c \
    ./libcn/reverse_poly.c \
    ./libcn/CommonBaseXX.c \
    ./libcn/reverse_crc.c \
    ./libcn/reflect.c \
    ./libcn/crc32-posix.c \
    ./libcn/CommonCRC.c \
    ./libcn/crc16-ccitt-true.c \
    ./libcn/crc16-a.c \
    ./libcn/crc32-xfer.c \
    ./libcn/crc32.c \
    ./libcn/crc16-icode.c \
    ./libcn/crc16-xmodem.c \
    ./libcn/crc8.c \
    ./libcn/crc16-dect-x.c \
    ./libcn/crc16-dect-r.c \
    ./libcn/crc16-ccitt-false.c \
    ./libcn/crc64-ecma-182.c \
    ./libcn/crc32-bzip2.c \
    ./libcn/normal_crc.c \
    ./libcn/crc16.c \
    ./libcn/crc16-usb.c \
    ./libcn/CommonBuffering.c \
    ./libcn/gen_std_crc_table.c \
    ./libcn/crc16-verifone.c \
    ./libcn/crc8-itu.c \
    ./libcn/adler32.c \
    ./libcn/crc32-castagnoli.c
LOCAL_SHARED_LIBRARIES := corecrypto \
    dispatch
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE		:= libcommoncryptotest
LOCAL_C_INCLUDES := $(LOCAL_PATH)/android/include \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/include/Private \
    $(LOCAL_PATH)/libcn \
    $(LOCAL_PATH)/lib \
    $(LOCAL_PATH)/CCRegression/util \
    $(LOCAL_PATH)/CCRegression/test
LOCAL_SRC_FILES := \
    ./CCRegression/CommonCrypto/CommonDigest.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymCTR.c \
    ./CCRegression/CommonCrypto/CommonHMacClone.c \
    ./CCRegression/CommonCrypto/CommonDHtest.c \
    ./CCRegression/CommonCrypto/CommonCryptoNoPad.c \
    ./CCRegression/CommonCrypto/CommonHMac.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymCCM.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymOFB.c \
    ./CCRegression/CommonCrypto/CCCryptorTestFuncs.c \
    ./CCRegression/CommonCrypto/CommonKeyDerivation.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymGCM.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymXTS.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymRegression.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymCBC.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymOffset.c \
    ./CCRegression/CommonCrypto/CryptorPadFailure.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymZeroLength.c \
    ./CCRegression/CommonCrypto/CommonCryptorWithData.c \
    ./CCRegression/CommonCrypto/CommonANSIKDF.c \
    ./CCRegression/CommonCrypto/CommonCMac.c \
    ./CCRegression/CommonCrypto/CommonCryptoReset.c \
    ./CCRegression/CommonCrypto/CommonNISTKDF.c \
    ./CCRegression/CommonCrypto/CommonCryptoBlowfish.c \
    ./CCRegression/CommonCrypto/CommonCPP.cpp \
    ./CCRegression/CommonCrypto/CommonCollabKeyGen.c \
    ./CCRegression/CommonCrypto/CommonBigNum.c \
    ./CCRegression/CommonCrypto/CommonRandom.c \
    ./CCRegression/CommonCrypto/CommonEC.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymRC2.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymmetricWrap.c \
    ./CCRegression/CommonCrypto/CommonHKDF.c \
    ./CCRegression/CommonCrypto/CommonCryptoOutputLength.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymECB.c \
    ./CCRegression/CommonCrypto/CommonCryptoSymCFB.c \
    ./CCRegression/CommonCrypto/CommonBigDigest.c \
    ./CCRegression/CommonCrypto/CommonCryptoCTSPadding.c \
    ./CCRegression/CommonCrypto/CommonRSA.c \
    ./CCRegression/test/testenv.c \
    ./CCRegression/test/testmore.c \
    ./CCRegression/test/testlist.c \
    ./CCRegression/util/testutil.c \
    ./CCRegression/util/testbyteBuffer.c \
    ./CCRegression/CommonNumerics/CommonCRC.c \
    ./CCRegression/CommonNumerics/CommonBaseEncoding.c \
    ./lib/timingsafe_bcmp.c
LOCAL_SHARED_LIBRARIES := commoncrypto \
    corecrypto \
    dispatch
include $(BUILD_SHARED_LIBRARY)

LOCAL_PATH := /SWE/release/Software/Harissa/Updates/BuiltHarissa/Roots/corecrypto
include $(CLEAR_VARS)
LOCAL_MODULE := corecrypto
LOCAL_SRC_FILES := prebuilts/$(TARGET_ARCH_ABI)/libcorecrypto.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

LOCAL_PATH := /SWE/release/Software/Harissa/Updates/BuiltHarissa/Roots/swift_corelibs_libdispatch
include $(CLEAR_VARS)
LOCAL_MODULE := dispatch
LOCAL_SRC_FILES := lib/$(TARGET_ARCH_ABI)/libdispatch.so
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_SHARED_LIBRARY)

