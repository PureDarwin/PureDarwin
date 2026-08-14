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
    @header SecCmsDecoder.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSDECODER_H_
#define _SECURITY_SECCMSDECODER_H_  1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

/*! @functiongroup Streaming interface */

#if TARGET_OS_OSX
/*!
     @function
     @abstract Set up decoding of a BER-encoded CMS message.
     @param arena An ArenaPool object to use for the resulting message, or NULL if new ArenaPool
     should be created.
     @param cb callback function for delivery of inner content inner
     content will be stored in the message if cb is NULL.
     @param cb_arg first argument passed to cb when it is called.
     @param pwfn callback function for getting token password for
     enveloped data content with a password recipient.
     @param pwfn_arg first argument passed to pwfn when it is called.
     @param decrypt_key_cb callback function for getting bulk key
     for encryptedData content.
     @param decrypt_key_cb_arg first argument passed to decrypt_key_cb
     when it is called.
     @param outDecoder On success will contain a pointer to a newly created SecCmsDecoder.
     @result A result code. See "SecCmsBase.h" for possible results.
     @discussion Create a SecCmsDecoder().  If this function returns noErr, the caller must dispose of the returned outDecoder by calling SecCmsDecoderDestroy() or SecCmsDecoderFinish().
     @availability 10.4 through 10.7
 */
extern OSStatus
SecCmsDecoderCreate(SecArenaPoolRef arena,
                    SecCmsContentCallback cb, void *cb_arg,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback decrypt_key_cb, void
                    *decrypt_key_cb_arg,
                    SecCmsDecoderRef *outDecoder)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set up decoding of a BER-encoded CMS message.
    @param cb callback function for delivery of inner content inner
	content will be stored in the message if cb is NULL.
    @param cb_arg first argument passed to cb when it is called.
    @param pwfn callback function for getting token password for
	enveloped data content with a password recipient.
    @param pwfn_arg first argument passed to pwfn when it is called.
    @param decrypt_key_cb callback function for getting bulk key
	for encryptedData content.
    @param decrypt_key_cb_arg first argument passed to decrypt_key_cb
	when it is called.
    @param outDecoder On success will contain a pointer to a newly created SecCmsDecoder.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion Create a SecCmsDecoder().  If this function returns errSecSuccess, the caller must dispose of the returned outDecoder by calling SecCmsDecoderDestroy() or SecCmsDecoderFinish().
    @availability 10.4 and later
 */
extern OSStatus
SecCmsDecoderCreate(SecCmsContentCallback cb, void *cb_arg,
                   PK11PasswordFunc pwfn, void *pwfn_arg,
                   SecCmsGetDecryptKeyCallback decrypt_key_cb, void
                   *decrypt_key_cb_arg,
                   SecCmsDecoderRef *outDecoder)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

/*!
    @function
    @abstract Feed BER-encoded data to decoder.
    @param decoder Pointer to a SecCmsDecoderContext created with SecCmsDecoderCreate().
    @param buf Pointer to bytes to be decoded.
    @param len number of bytes to decode.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion If a call to this function fails the caller should call SecCmsDecoderDestroy().
    @availability 10.4 and later
 */
extern OSStatus
SecCmsDecoderUpdate(SecCmsDecoderRef decoder, const void *buf, CFIndex len);

/*!
    @function
    @abstract Abort a (presumably failed) decoding process.
    @param decoder Pointer to a SecCmsDecoderContext created with SecCmsDecoderCreate().
    @availability 10.4 and later
 */
extern void
SecCmsDecoderDestroy(SecCmsDecoderRef decoder);

/*!
    @function
    @abstract Mark the end of inner content and finish decoding.
    @param decoder Pointer to a SecCmsDecoderContext created with SecCmsDecoderCreate().
    @param outMessage On success a pointer to a SecCmsMessage containing the decoded message.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion decoder is no longer valid after this function is called.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsDecoderFinish(SecCmsDecoderRef decoder, SecCmsMessageRef *outMessage);

/*! @functiongroup One shot interface */
#if TARGET_OS_OSX
/*!
    @function
    @abstract Decode a CMS message from BER encoded data.
    @discussion This function basically does the same as calling
    SecCmsDecoderStart(), SecCmsDecoderUpdate() and SecCmsDecoderFinish().
    @param encodedMessage Pointer to a CSSM_DATA containing the BER encoded cms
    message to decode.
    @param cb callback function for delivery of inner content inner
    content will be stored in the message if cb is NULL.
    @param cb_arg first argument passed to cb when it is called.
    @param pwfn callback function for getting token password for enveloped
    data content with a password recipient.
    @param pwfn_arg first argument passed to pwfn when it is called.
    @param decrypt_key_cb callback function for getting bulk key for encryptedData content.
    @param decrypt_key_cb_arg first argument passed to decrypt_key_cb when it is called.
    @param outMessage On success a pointer to a SecCmsMessage containing the decoded message.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion decoder is no longer valid after this function is called.
    @availability 10.4 through 10.7
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern OSStatus
SecCmsMessageDecode(const CSSM_DATA *encodedMessage,
                    SecCmsContentCallback cb, void *cb_arg,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
                    SecCmsMessageRef *outMessage)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Decode a CMS message from BER encoded data.
    @discussion This function basically does the same as calling
                SecCmsDecoderStart(), SecCmsDecoderUpdate() and SecCmsDecoderFinish().
    @param encodedMessage Pointer to a SecAsn1Item containing the BER encoded cms
           message to decode.
    @param cb callback function for delivery of inner content inner
           content will be stored in the message if cb is NULL.
    @param cb_arg first argument passed to cb when it is called.
    @param pwfn callback function for getting token password for enveloped
           data content with a password recipient.
    @param pwfn_arg first argument passed to pwfn when it is called.
    @param decrypt_key_cb callback function for getting bulk key for encryptedData content.
    @param decrypt_key_cb_arg first argument passed to decrypt_key_cb when it is called.
    @param outMessage On success a pointer to a SecCmsMessage containing the decoded message.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion decoder is no longer valid after this function is called.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsMessageDecode(const SecAsn1Item *encodedMessage,
                    SecCmsContentCallback cb, void *cb_arg,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
                    SecCmsMessageRef *outMessage)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX


__END_DECLS

#endif /* _SECURITY_SECCMSDECODER_H_ */
