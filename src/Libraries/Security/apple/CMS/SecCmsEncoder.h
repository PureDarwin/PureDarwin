/*
 *  Copyright (c) 2004-2018 Apple Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*!
    @header SecCmsEncoder.h

    @availability 10.4 and later
    @abstract CMS message encoding
    @discussion The functions here implement functions for encoding
                Cryptographic Message Syntax (CMS) objects as described
                in rfc3369.
                A SecCmsEncoder object is used to encode CMS messages into BER.
 */

#ifndef _SECURITY_SECCMSENCODER_H_
#define _SECURITY_SECCMSENCODER_H_  1

#include <Security/SecCmsBase.h>
#include <CoreFoundation/CFData.h>


__BEGIN_DECLS

/*! @functiongroup Streaming interface */

#if TARGET_OS_OSX
/*!
     @function
     @abstract Set up encoding of a CMS message.
     @param outputfn callback function for delivery of BER-encoded output will
     not be called if NULL.
     @param outputarg first argument passed to outputfn when it is called.
     @param dest If non-NULL, pointer to a CSSM_DATA that will hold the
     BER-encoded output.
     @param destpoolp Pool to allocate BER-encoded output in.
     @param pwfn callback function for getting token password for enveloped
     data content with a password recipient.
     @param pwfn_arg first argument passed to pwfn when it is called.
     @param encrypt_key_cb callback function for getting bulk key for encryptedData content.
     @param encrypt_key_cb_arg first argument passed to encrypt_key_cb when it is
     called.
     @param detached_digestalgs digest algorithms in detached_digests
     @param detached_digests digests from detached content (one for every element
     in detached_digestalgs).
     @result On success a pointer to a SecCmsMessage containing the decoded message
     is returned. On failure returns NULL. Call PR_GetError() to find out what
     went wrong in this case.
     @availability 10.4 through 10.7
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern OSStatus
SecCmsEncoderCreate(SecCmsMessageRef cmsg,
                    SecCmsContentCallback outputfn, void *outputarg,
                    CSSM_DATA_PTR dest, SecArenaPoolRef destpoolp,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback encrypt_key_cb, void *encrypt_key_cb_arg,
                    SECAlgorithmID **detached_digestalgs, CSSM_DATA_PTR *detached_digests,
                    SecCmsEncoderRef *outEncoder)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set up encoding of a CMS message.
	@param cmsg The SecCmsMessageRef to be encoded.
    @param outputfn callback function for delivery of BER-encoded output will
        not be called if NULL.
    @param outputarg first argument passed to outputfn when it is called.
    @param outBer If non-NULL, a CFMutableDataRef to which the
        BER-encoded output will be appended.
    @param pwfn callback function for getting token password for enveloped
           data content with a password recipient.
    @param pwfn_arg first argument passed to pwfn when it is called.
    @param encrypt_key_cb callback function for getting bulk key for encryptedData content.
    @param encrypt_key_cb_arg first argument passed to encrypt_key_cb when it is
        called.
    @result On success a pointer to a SecCmsMessage containing the decoded message
        is returned. On failure returns NULL. Call PR_GetError() to find out what
        went wrong in this case.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsEncoderCreate(SecCmsMessageRef cmsg,
                    SecCmsContentCallback outputfn, void *outputarg,
                    CFMutableDataRef outBer,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback encrypt_key_cb, void *encrypt_key_cb_arg,
                    SecCmsEncoderRef *outEncoder)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX
    
/*!
    @function
    @abstract Take content data delivery from the user
    @param encoder encoder context
    @param data content data
    @param len length of content data
    @result On success 0 is returned. On failure returns non zero. Call 
        PR_GetError() to find out what went wrong in this case.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsEncoderUpdate(SecCmsEncoderRef encoder, const void *data, CFIndex len);

/*!
    @function
    @abstract Abort a (presumably failed) encoding process.
    @param encoder Pointer to a SecCmsEncoderContext created with SecCmsEncoderCreate().
    @availability 10.4 and later
 */
extern void
SecCmsEncoderDestroy(SecCmsEncoderRef encoder);

/*!
    @function
    @abstract Signal the end of data.
    @discussion Walks down the chain of encoders and the finishes them from the
        innermost out.
    @param encoder Pointer to a SecCmsEncoder created with SecCmsEncoderCreate().
    @result On success 0 is returned. On failure returns non zero. Call 
        PR_GetError() to find out what went wrong in this case.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsEncoderFinish(SecCmsEncoderRef encoder);

/*! @functiongroup One shot interface */
#if TARGET_OS_OSX
/*!
     @function
     @abstract BER Encode a CMS message.
     @discussion BER Encode a CMS message, with input being the plaintext message and outBer being the output, stored in arena's pool.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern OSStatus
SecCmsMessageEncode(SecCmsMessageRef cmsg, const CSSM_DATA *input, SecArenaPoolRef arena,
                    CSSM_DATA_PTR outBer)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract BER Encode a CMS message.
	@param cmsg The SecCmsMessageRef to be encoded.
	@param input The inner content of the message.
    @param outBer A CFMutableDataRef to which the
        BER-encoded output will be appended.
    @discussion BER Encode a CMS message, with input being the plaintext message and outBer being the output, stored in arena's pool.
 */
extern OSStatus
SecCmsMessageEncode(SecCmsMessageRef cmsg, const SecAsn1Item *input,
                    CFMutableDataRef outBer)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

__END_DECLS

#endif /* _SECURITY_SECCMSENCODER_H_ */
