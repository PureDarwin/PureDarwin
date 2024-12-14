/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef	_CC_CryptorSPI_H_
#define _CC_CryptorSPI_H_

#include <sys/types.h>
#include <stdint.h>

#include <string.h>
#include <limits.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#include <availability.h>
#else
#include <os/availability.h>
#endif

#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonCryptoErrorSPI.h>
#include <CommonCrypto/CommonCryptor.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
    int timingsafe_bcmp(const void *b1, const void *b2, size_t n);

#endif
/*
	This is an SPI header.  It includes some work in progress implementation notes that
	will be removed when this is promoted to an API set.
*/

/*
 	Private Ciphers
 */

/* Lion SPI name for no padding.  Defining for compatibility.  Is now
   ccNoPadding in CommonCryptor.h
 */

enum {
    ccDefaultPadding = ccNoPadding,
};

/*
 	Private Modes
 */
enum {
	kCCModeXTS		= 8,
	kCCModeGCM		= 11,
	kCCModeCCM		= 12,
};

/*
 	Private Paddings
 */
enum {
    ccCBCCTS3			= 12,
};

/*
    Private Cryptor direction (op)
 */
enum {
    kCCBoth		= 3,
};




/*
	Supports a mode call of
	int mode_setup(int cipher, const unsigned char *IV, const unsigned char *key, int keylen,
		const unsigned char *tweak, int tweaklen, int num_rounds, int options, mode_context *ctx);
*/

/* User supplied space for the CryptorRef */

CCCryptorStatus CCCryptorCreateFromDataWithMode(
	CCOperation 	op,				/* kCCEncrypt, kCCEncrypt, kCCBoth (default for BlockMode) */
	CCMode			mode,
	CCAlgorithm		alg,
	CCPadding		padding,
	const void 		*iv,			/* optional initialization vector */
	const void 		*key,			/* raw key material */
	size_t 			keyLength,
	const void 		*tweak,			/* raw tweak material */
	size_t 			tweakLength,
	int				numRounds,
	CCModeOptions 	options,
	const void		*data,			/* caller-supplied memory */
	size_t			dataLength,		/* length of data in bytes */
	CCCryptorRef	*cryptorRef,	/* RETURNED */
	size_t			*dataUsed)		/* optional, RETURNED */
API_AVAILABLE(macos(10.7), ios(5.0));


/*
	Assuming we can use existing CCCryptorCreateFromData for all modes serviced by these:
	int mode_encrypt(const unsigned char *pt, unsigned char *ct, unsigned long len, mode_context *ctx);
	int mode_decrypt(const unsigned char *ct, unsigned char *pt, unsigned long len, mode_context *ctx);
*/

/*
	Block mode encrypt and decrypt interfaces for IV tweaked blocks (XTS and CBC)

	int mode_encrypt_tweaked(const unsigned char *pt, unsigned long len, unsigned char *ct, const unsigned char *tweak, mode_context *ctx);
	int mode_decrypt_tweaked(const unsigned char *ct, unsigned long len, unsigned char *pt, const unsigned char *tweak, mode_context *ctx);
*/

CCCryptorStatus CCCryptorEncryptDataBlock(
	CCCryptorRef cryptorRef,
	const void *iv,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
API_AVAILABLE(macos(10.7), ios(5.0));


CCCryptorStatus CCCryptorDecryptDataBlock(
	CCCryptorRef cryptorRef,
	const void *iv,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
API_AVAILABLE(macos(10.7), ios(5.0));

/*!
    @function   CCCryptorReset_binary_compatibility
    @abstract   Do not call this function. Reinitializes an existing CCCryptorRef with a (possibly)
                new initialization vector. The CCCryptorRef's key is
                unchanged. Preserves compatibility for SKDs prior to
                macOS 10.13, iOS 11, watchOS 4 and tvOS 11. It is used
                internally in CommonCrypto. See CCCryptorReset for more information.

    @result     The only possible error is kCCParamError.

*/
CCCryptorStatus CCCryptorReset_binary_compatibility(CCCryptorRef cryptorRef, const void *iv)
    API_DEPRECATED_WITH_REPLACEMENT("CCCryptorReset", macos(10.4, 10.13), ios(2.0, 11.0));

/*
	Assuming we can use the existing CCCryptorRelease() interface for
	int mode_done(mode_context *ctx);
*/

/*
	Not surfacing these other than with CCCryptorReset()

	int mode_setiv(const unsigned char *IV, unsigned long len, mode_context *ctx);
	int mode_getiv(const unsigned char *IV, unsigned long *len, mode_context *ctx);
*/

/*
    DES key utilities
*/

CCCryptorStatus CCDesIsWeakKey(
                               void *key,
                               size_t Length)
API_AVAILABLE(macos(10.7), ios(5.0));

void CCDesSetOddParity(void *key, size_t Length)
API_AVAILABLE(macos(10.7), ios(5.0));

uint32_t CCDesCBCCksum(void *input, void *output,
                       size_t length, void *key, size_t keylen,
                       void *ivec)
API_AVAILABLE(macos(10.7), ios(5.0));


/*
 * returns a cipher blocksize length iv in the provided iv buffer.
 */

CCCryptorStatus
CCCryptorGetIV(CCCryptorRef cryptorRef, void *iv)
API_AVAILABLE(macos(10.7), ios(5.0));

/*
    GCM Support Interfaces

	Use CCCryptorCreateWithMode() with the kCCModeGCM selector to initialize
    a CryptoRef.  Only kCCAlgorithmAES128 can be used with GCM and these
    functions.  IV Setting etc will be ignored from CCCryptorCreateWithMode().
    Use the CCCryptorGCMAddIV() routine below for IV setup.
*/

/*
	Deprecated. Use CCCryptorGCMSetIV() instead.
    This adds the initial vector octets from iv of length ivLen to the GCM
	CCCryptorRef. You can call this function as many times as required to
	process the entire IV.
*/

CCCryptorStatus
CCCryptorGCMAddIV(CCCryptorRef cryptorRef,
                	const void *iv, size_t ivLen)
API_DEPRECATED_WITH_REPLACEMENT("CCCryptorGCMSetIV", macos(10.8, 10.13), ios(5.0, 11.0));


/*
   This adds the initial vector octets from iv of length ivLen to the GCM
   CCCryptorRef. The input iv cannot be NULL and ivLen must be between 12
   to 16 bytes inclusive. CCRandomGenerateBytes() can be used to generate random IVs
*/

CCCryptorStatus
CCCryptorGCMSetIV(CCCryptorRef cryptorRef,
                    const void *iv, size_t ivLen)
API_AVAILABLE(macos(10.13), ios(11.0));
/*
	Additional Authentication Data
	After the entire IV has been processed, the additional authentication
    data can be processed. Unlike the IV, a packet/session does not require
    additional authentication data (AAD) for security. The AAD is meant to
    be used as side–channel data you want to be authenticated with the packet.
    Note: once you begin adding AAD to the GCM CCCryptorRef you cannot return
    to adding IV data until the state has been reset.
*/

CCCryptorStatus
CCCryptorGCMAddAAD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
API_AVAILABLE(macos(10.8), ios(6.0));

// Maintain the old symbol with incorrect camel-case for now.
CCCryptorStatus
CCCryptorGCMaddAAD(CCCryptorRef cryptorRef,
                	const void 		*aData,
                    size_t aDataLen)
API_AVAILABLE(macos(10.8), ios(6.0));

// This is for old iOS5 clients
CCCryptorStatus
CCCryptorGCMAddADD(CCCryptorRef cryptorRef,
                   const void 		*aData,
                   size_t aDataLen)
API_AVAILABLE(macos(10.8), ios(5.0));


CCCryptorStatus CCCryptorGCMEncrypt(
	CCCryptorRef cryptorRef,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
API_AVAILABLE(macos(10.8), ios(5.0));


CCCryptorStatus CCCryptorGCMDecrypt(
	CCCryptorRef cryptorRef,
	const void *dataIn,
	size_t dataInLength,
	void *dataOut)
API_AVAILABLE(macos(10.8), ios(5.0));

/*
    Finalizes the GCM state and stores the tag in tagOut of length
    tagLength octets.

    The tag must be verified by comparing the computed and expected values
    using timingsafe_bcmp. Other comparison functions (e.g. memcmp)
    must not be used as they may be vulnerable to practical timing attacks,
    leading to tag forgery.
*/
CCCryptorStatus CCCryptorGCMFinal(
    CCCryptorRef cryptorRef,
    void   *tagOut,
    size_t *tagLength)
API_DEPRECATED_WITH_REPLACEMENT("CCCryptorGCMFinalize", macos(10.8, 10.13), ios(5.0, 11.0));

/*
     Finalizes the GCM state.

     On encryption, the computed tag is stored in tag.

     On decryption, the provided tag is securely compared to the expected tag
     and an error returned if the tags do not match. The tag buffer content
     is not modified on decryption.
*/
CCCryptorStatus CCCryptorGCMFinalize(
    CCCryptorRef cryptorRef,
    void   *tag,
    size_t tagLength)
API_AVAILABLE(macos(10.13), ios(11.0));

/*
	This will reset the GCM CCCryptorRef to the state that CCCryptorCreateWithMode()
    left it. The user would then call CCCryptorGCMAddIV(), CCCryptorGCMaddAAD(), etc.
*/

CCCryptorStatus CCCryptorGCMReset(
	CCCryptorRef cryptorRef)
API_AVAILABLE(macos(10.8), ios(5.0));

/*
    Deprecated. Use CCCryptorGCMOneshotEncrypt() or CCCryptorGCMOneshotDecrypt() instead.

	This will initialize the GCM state with the given key, IV and AAD value
    then proceed to encrypt or decrypt the message text and store the final
    message tag. The definition of the variables is the same as it is for all
    the manual functions. If you are processing many packets under the same
    key you shouldn’t use this function as it invokes the pre–computation
    with each call.

    The tag must be verified by comparing the computed and expected values
    using timingsafe_bcmp. Other comparison functions (e.g. memcmp)
    must not be used as they may be vulnerable to practical timing attacks,
    leading to tag forgery.
*/

CCCryptorStatus CCCryptorGCM(
	CCOperation 	op,				/* kCCEncrypt, kCCDecrypt */
	CCAlgorithm		alg,
	const void 		*key,			/* raw key material */
	size_t 			keyLength,
	const void 		*iv,
	size_t 			ivLen,
	const void 		*aData,
	size_t 			aDataLen,
	const void 		*dataIn,
	size_t 			dataInLength,
  	void 			*dataOut,
	void 		    *tagOut,
	size_t 			*tagLength)
API_DEPRECATED_WITH_REPLACEMENT("CCCryptorGCMOneshotEncrypt or CCCryptorGCMOneshotDecrypt", macos(10.8, 10.13), ios(6.0, 11.0));

/*!
 @function   CCCryptorGCMOneshotEncrypt
 @abstract   Encrypts using AES-GCM and outputs encrypted data and an authentication tag
 @param      alg            It can only be kCCAlgorithmAES
 @param      key            AES key size (kCCKeySizeAES128, kCCKeySizeAES192 or kCCKeySizeAES256)
 @param      keyLength      Length of the key in bytes

 @param      iv             Initialization vector, must be at least 12 bytes
 @param      ivLength       Length of the IV in bytes

 @param      aData          Additional data to authenticate. It can be NULL, if there is no additional data to be authenticated.
 @param      aDataLength    Length of the additional data in bytes. It can be zero.

 @param      dataIn         Input plaintext
 @param      dataInLength   Length of the input plaintext data in bytes

 @param      cipherOut      Output ciphertext
 @param      tagLength      Length of the output authentication tag in bytes. (Recommended: 16 bytes. Minimum: 8 bytes.)
 @param      tagOut         the output authentication tag

 @result     kCCSuccess if successful.

 @discussion It is a one-shot AESGCM encryption and in-place encryption is supported.

 @warning The key-IV pair must be unique per encryption. The IV must be nonzero in length.

 In stateful protocols, if each packet exposes a guaranteed-unique value, it is recommended to format this as a 12-byte value for use as the IV.

 In stateless protocols, it is recommended to choose a 12-byte value using a cryptographically-secure pseudorandom number generator (e.g. @p ccrng).

 The length of the authentication tag should always be 16 bytes. It should be lower only to support legacy applications.
*/
CCCryptorStatus CCCryptorGCMOneshotEncrypt(CCAlgorithm alg, const void  *key,    size_t keyLength, /* raw key material */
                                           const void  *iv,     size_t ivLength,
                                           const void  *aData,  size_t aDataLength,
                                           const void  *dataIn, size_t dataInLength,
                                           void 	    *cipherOut,
                                           void        *tagOut, size_t tagLength) __attribute__((__warn_unused_result__))
API_AVAILABLE(macos(10.13), ios(11.0));

/*!
 @function   CCCryptorGCMOneshotDecrypt
 @abstract   Decrypts using AES-GCM, compares the computed tag of the decrypted message to the input tag and returns error is authentication fails.

 @discussion CCCryptorGCMOneshotDecrypt() works similar to CCCryptorGCMOneshotEncrypt(). CCCryptorGCMOneshotDecrypt() does not return the tag of the decrypted message. It compares the computed tag with the input tag and outputs an error if authentication of the decrypted message fails
 */
CCCryptorStatus CCCryptorGCMOneshotDecrypt(CCAlgorithm alg, const void  *key,    size_t keyLength,
                                           const void  *iv,     size_t ivLen,
                                           const void  *aData,  size_t aDataLen,
                                           const void  *dataIn, size_t dataInLength,
                                           void 	   *dataOut,
                                           const void  *tagIn,  size_t tagLength) __attribute__((__warn_unused_result__))
API_AVAILABLE(macos(10.13), ios(11.0));

/*
GCM interface can then be easily bolt on the rest of standard CCCryptor interface; typically following sequence can be used:

CCCryptorCreateWithMode(mode = kCCModeGCM)
0..Nx: CCCryptorAddParameter(kCCParameterIV, iv)
0..Nx: CCCryptorAddParameter(kCCParameterAuthData, data)
0..Nx: CCCryptorUpdate(inData, outData)
0..1: CCCryptorFinal(outData)
0..1: CCCryptorGetParameter(kCCParameterAuthTag, tag)
CCCryptorRelease()

*/

 /*!
  @function CCCryptorChaCha20Poly1305OneshotEncrypt
  @abstract Encrypts using ChaCha20Poly1305 and outputs encrypted data and an authentication tag.

  @param key ChaCha20Poly1305 key.
  @param keyLength Length of the key in bytes. This MUST be 32 bytes.
  @param iv Initialization vector.
  @param ivLength Length of the IV in bytes. This MUST be 12 bytes.
  @param aData Additional data to authenticate. It can be NULL, if there is no additional data to be authenticated.
  @param aDataLength Length of the additional data in bytes. It can be zero.
  @param dataIn Input plaintext.
  @param dataInLength Length of the input plaintext data in bytes.
  @param cipherOut Output ciphertext.
  @param tagOut The output authentication tag.
  @param tagLength Length of the output authentication tag in bytes. This MUST be 16 bytes.

  @result kCCSuccess if successful.

  @discussion It is a one-shot ChaCha20Poly1305 authenticated encryption.

  @warning The key-IV pair must be unique per encryption.
*/
CCCryptorStatus CCCryptorChaCha20Poly1305OneshotEncrypt(const void *key, size_t keyLength,
                                                        const void *iv, size_t ivLength,
                                                        const void *aData, size_t aDataLength,
                                                        const void *dataIn, size_t dataInLength,
                                                        void *cipherOut, void *tagOut, size_t tagLength) __attribute__((__warn_unused_result__))
API_AVAILABLE(macos(10.16), ios(14.0));

/*!
 @function CCCryptorChaCha20Poly1305OneshotDecrypt
 @abstract Decrypts using ChaCha20Poly1305, compares the computed tag of the decrypted message to the input tag and returns an error if authentication fails.

 @param key ChaCha20Poly1305 key.
 @param keyLength Length of the key in bytes. This MUST be 32 bytes.
 @param iv Initialization vector.
 @param ivLength Length of the IV in bytes. This MUST be 12 bytes.
 @param aData Additional data to authenticate. It can be NULL, if there is no additional data to be authenticated.
 @param aDataLength Length of the additional data in bytes. It can be zero.
 @param cipherIn Input ciphertext.
 @param cipherInLength Length of the output authentication tag in bytes. This MUST be 16 bytes.
 @param dataOut Input plaintext.
 @param tagIn The authentication tag.
 @param tagLength The authentication tag length.

 @result kCCSuccess if successful.

 @discussion CCCryptorChaCha20Poly1305OneshotDecrypt() does not return the tag of the decrypted message. It compares the computed tag with
 the input tag and outputs an error if authentication of the decrypted message fails.
 */
CCCryptorStatus CCCryptorChaCha20Poly1305OneshotDecrypt(const void *key, size_t keyLength,
                                                        const void *iv, size_t ivLength,
                                                        const void *aData, size_t aDataLength,
                                                        const void *cipherIn, size_t cipherInLength,
                                                        void *dataOut, const void *tagIn, size_t tagLength) __attribute__((__warn_unused_result__))
API_AVAILABLE(macos(10.16), ios(14.0));

/*!
 @function CCCryptorChaCha20
 @abstract Compute the ChaCha20 stream cipher function.

 @param key CCCryptorChaCha20 key.
 @param keyLength Length of the key in bytes. This MUST be 32 bytes.
 @param nonce Nonce.
 @param nonceLength Length of the nonce. This MUST be 12 bytes.
 @param counter ChaCha20 counter value.
 @param dataIn Length of the input data in bytes.
 @param dataInLength Input data.
 @param dataOut Output data.

 @result kCCSuccess if successful.
*/
CCCryptorStatus CCCryptorChaCha20(const void *key, size_t keyLength,
                                  const void *nonce, size_t nonceLength,
                                  uint32_t counter,
                                  const void *dataIn, size_t dataInLength, void *dataOut) __attribute__((__warn_unused_result__))
API_AVAILABLE(macos(10.16), ios(14.0));

enum {
    /*
        Initialization vector - cryptor input parameter, typically
        needs to have the same length as block size, but in some cases
        (GCM) it can be arbitrarily long and even might be called
        multiple times.
    */
    kCCParameterIV,

    /*
        Authentication data - cryptor input parameter, input for
        authenticating encryption modes like GCM.  If supported, can
        be called multiple times before encryption starts.
    */
    kCCParameterAuthData,

    /*
        Mac Size - cryptor input parameter, input for
        authenticating encryption modes like CCM. Specifies the size of
        the AuthTag the algorithm is expected to produce.
    */
    kCCMacSize,

    /*
        Data Size - cryptor input parameter, input for
        authenticating encryption modes like CCM. Specifies the amount of
        data the algorithm is expected to process.
    */
    kCCDataSize,

    /*
        Authentication tag - cryptor output parameter, output from
        authenticating encryption modes like GCM.  If supported,
        should be retrieved after the encryption finishes.
    */
    kCCParameterAuthTag,
};
typedef uint32_t CCParameter;

/*
    Sets or adds some other cryptor input parameter.  According to the
    cryptor type and state, parameter can be either accepted or
    refused with kCCUnimplemented (when given parameter is not
    supported for this type of cryptor at all) or kCCParamError (bad
    data length or format) or kCCCallSequenceError (bad sequence of
    calls when using GCM or CCM).
*/

CCCryptorStatus CCCryptorAddParameter(
    CCCryptorRef cryptorRef,
    CCParameter parameter,
    const void *data,
    size_t dataSize);


/*
    Gets value of output cryptor parameter.  According to the cryptor
    type state, the request can be either accepted or refused with
    kCCUnimplemented (when given parameteris not supported for this
    type of cryptor) or kCCBufferTooSmall (in this case, *dataSize
    argument is set to the requested size of data).
*/

CCCryptorStatus CCCryptorGetParameter(
    CCCryptorRef cryptorRef,
    CCParameter parameter,
    void *data,
    size_t *dataSize);


#ifdef __cplusplus
}
#endif

#endif /* _CC_CryptorSPI_H_ */
