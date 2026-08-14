/*
 * Copyright (c) 2006-2018 Apple Inc. All Rights Reserved.
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

/* 
 * CMSPrivate.h - access to low-level CMS objects used by CMSDecoder and CMSEncoder.
 */
#ifndef	_CMS_PRIVATE_H_
#define _CMS_PRIVATE_H_

#include <Security/CMSEncoder.h>	
#include <Security/CMSDecoder.h>	
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsMessage.h>
#include <AvailabilityMacros.h>

#ifdef __cplusplus
extern "C" {
#endif

/***
 *** Private CMSEncoder routines
 ***/
 
/*
 * Obtain the SecCmsMessageRef associated with a CMSEncoderRef. Intended 
 * to be called after (optionally) setting the encoder's various attributes 
 * via CMSEncoderAddSigners(), CMSEncoderAddRecipients(), etc. and before 
 * the first call to CMSEncoderUpdateContent(). The returned SecCmsMessageRef 
 * will be initialized per the previously specified attributes; the caller 
 * can manipulate the SecCmsMessageRef prior to proceeding with 
 * CMSEncoderUpdateContent() calls. 
 */
OSStatus CMSEncoderGetCmsMessage(
	CMSEncoderRef		cmsEncoder,
	SecCmsMessageRef	*cmsMessage);		/* RETURNED */
	
/* 
 * Optionally specify a SecCmsEncoderRef to use with a CMSEncoderRef.
 * If this is called, it must be called before the first call to 
 * CMSEncoderUpdateContent(). The CMSEncoderRef takes ownership of the
 * incoming SecCmsEncoderRef.
 */
OSStatus CMSEncoderSetEncoder(
	CMSEncoderRef		cmsEncoder,
	SecCmsEncoderRef	encoder);
	
/* 
 * Obtain the SecCmsEncoderRef associated with a CMSEncoderRef. 
 * Returns a NULL SecCmsEncoderRef if neither CMSEncoderSetEncoder nor
 * CMSEncoderUpdateContent() has been called. 
 * The CMSEncoderRef retains ownership of the SecCmsEncoderRef.
 */
OSStatus CMSEncoderGetEncoder(
	CMSEncoderRef		cmsEncoder,
	SecCmsEncoderRef	*encoder);			/* RETURNED */

/*
 * Set the signing time for a CMSEncoder.
 * This is only used if the kCMSAttrSigningTime attribute is included.
 */
OSStatus CMSEncoderSetSigningTime(
	CMSEncoderRef		cmsEncoder,
	CFAbsoluteTime		time);

/*
 * Set the hash agility attribute for a CMSEncoder.
 * This is only used if the kCMSAttrAppleCodesigningHashAgility attribute
 * is included.
 */
OSStatus CMSEncoderSetAppleCodesigningHashAgility(
        CMSEncoderRef   cmsEncoder,
        CFDataRef       hashAgilityAttrValue);

/*
 * Set the hash agility attribute for a CMSEncoder.
 * This is only used if the kCMSAttrAppleCodesigningHashAgilityV2 attribute
 * is included. V2 encodes the hash agility values using DER.
 * The dictionary should have CFNumberRef keys, corresponding to SECOidTags
 * (from SecCmsBase.h) for digest algorithms, and CFDataRef values,
 * corresponding to the digest value for that digest algorithm.
 */
OSStatus CMSEncoderSetAppleCodesigningHashAgilityV2(
    CMSEncoderRef       cmsEncoder,
    CFDictionaryRef     hashAgilityV2AttrValues);

/*
 * Set the expiration time for a CMSEncoder.
 * This is only used if the kCMSAttrAppleExpirationTime attribute is included.
 */
OSStatus CMSEncoderSetAppleExpirationTime(
                                          CMSEncoderRef        cmsEncoder,
                                          CFAbsoluteTime        time);


void
CmsMessageSetTSAContext(CMSEncoderRef cmsEncoder, CFTypeRef tsaContext)
    API_AVAILABLE(macos(10.8)) API_UNAVAILABLE(ios, tvos, watchos, bridgeos, macCatalyst);

/***
 *** Private CMSDecoder routines
 ***/
 
/*
 * Obtain the SecCmsMessageRef associated with a CMSDecoderRef. Intended 
 * to be called after decoding the message (i.e., after 
 * CMSDecoderFinalizeMessage() to gain finer access to the contents of the
 * SecCmsMessageRef than is otherwise available via the CMSDecoder interface. 
 * Returns a NULL SecCmsMessageRef if CMSDecoderFinalizeMessage() has not been
 * called. 
 *
 * The CMSDecoder retains ownership of the returned SecCmsMessageRef.
 */
OSStatus CMSDecoderGetCmsMessage(
	CMSDecoderRef		cmsDecoder,
	SecCmsMessageRef	*cmsMessage);		/* RETURNED */
	

/* 
 * Optionally specify a SecCmsDecoderRef to use with a CMSDecoderRef.
 * If this is called, it must be called before the first call to 
 * CMSDecoderUpdateMessage(). The CMSDecoderRef takes ownership of the
 * incoming SecCmsDecoderRef.
 */
OSStatus CMSDecoderSetDecoder(
	CMSDecoderRef		cmsDecoder,
	SecCmsDecoderRef	decoder);
	
/* 
 * Obtain the SecCmsDecoderRef associated with a CMSDecoderRef. 
 * Returns a NULL SecCmsDecoderRef if neither CMSDecoderSetDecoder() nor
 * CMSDecoderUpdateMessage() has been called. 
 * The CMSDecoderRef retains ownership of the SecCmsDecoderRef.
 */
OSStatus CMSDecoderGetDecoder(
	CMSDecoderRef		cmsDecoder,
	SecCmsDecoderRef	*decoder);			/* RETURNED */

/*
 * Obtain the Hash Agility attribute value of signer 'signerIndex'
 * of a CMS message, if present.
 *
 * Returns errSecParam if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one.
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called.
 */
OSStatus CMSDecoderCopySignerAppleCodesigningHashAgility(
    CMSDecoderRef		cmsDecoder,
    size_t				signerIndex,            /* usually 0 */
    CFDataRef  CF_RETURNS_RETAINED *hashAgilityAttrValue)		/* RETURNED */
    API_AVAILABLE(macos(10.12.4), ios(11.0));

/*
 * Obtain the Hash Agility v2 attribute value of signer 'signerIndex'
 * of a CMS message, if present. V2 encodes the hash agility values using DER.
 *
 * Returns errSecParam if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one.
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called.
 */
OSStatus CMSDecoderCopySignerAppleCodesigningHashAgilityV2(
    CMSDecoderRef                           cmsDecoder,
    size_t                                  signerIndex,            /* usually 0 */
    CFDictionaryRef CF_RETURNS_RETAINED *   hashAgilityAttrValues)   /* RETURNED */
    API_AVAILABLE(macos(10.13.4), ios(11.3));

/*
 * Obtain the expiration time of signer 'signerIndex' of a CMS message, if
 * present. This is part of the signed attributes of the message.
 *
 * Returns errSecParam if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one.
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called.
 */
OSStatus CMSDecoderCopySignerAppleExpirationTime(
    CMSDecoderRef      cmsDecoder,
    size_t             signerIndex,
    CFAbsoluteTime     *expirationTime)            /* RETURNED */
    API_AVAILABLE(macos(10.14), ios(12.0));

#ifdef __cplusplus
}
#endif

#endif	/* _CMS_PRIVATE_H_ */

