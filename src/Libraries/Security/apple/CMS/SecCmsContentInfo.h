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
    @header SecCmsContentInfo.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for creating and
                accessing ContentInfo objects that are part of Cryptographic
                Message Syntax (CMS) objects as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSCONTENTINFO_H_
#define _SECURITY_SECCMSCONTENTINFO_H_  1

#include <Security/SecCmsBase.h>
#include <CoreFoundation/CFData.h>


__BEGIN_DECLS

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/*! @functiongroup ContentInfo accessors */
/*!
    @function
    @abstract Get content's contentInfo (if it exists).
    @param cinfo A ContentInfo object of which we want to get the child contentInfo.
    @result The child ContentInfo object, or NULL if there is none.
    @discussion This function requires a ContentInfo object which is usually created by decoding and SecCmsMessage using a SecCmsDecoder.
    @availability 10.4 and later
 */
extern SecCmsContentInfoRef
SecCmsContentInfoGetChildContentInfo(SecCmsContentInfoRef cinfo);

/*!
    @function
    @abstract Get pointer to inner content
    @discussion needs to be casted...
 */
extern void *
SecCmsContentInfoGetContent(SecCmsContentInfoRef cinfo);

#if TARGET_OS_OSX
/*!
     @function
     @abstract Get pointer to innermost content
     @discussion This is typically only called by SecCmsMessageGetContent().
 */
extern CSSM_DATA_PTR
SecCmsContentInfoGetInnerContent(SecCmsContentInfoRef cinfo)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Get pointer to innermost content
    @discussion This is typically only called by SecCmsMessageGetContent().
 */
extern const SecAsn1Item *
SecCmsContentInfoGetInnerContent(SecCmsContentInfoRef cinfo)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

/*!
    @function
    @abstract Find out and return the inner content type.
 */
extern SECOidTag
SecCmsContentInfoGetContentTypeTag(SecCmsContentInfoRef cinfo);


#if TARGET_OS_OSX
/*!
     @function
     @abstract Find out and return the inner content type.
     @discussion Caches pointer to lookup result for future reference.
 */
extern CSSM_OID *
SecCmsContentInfoGetContentTypeOID(SecCmsContentInfoRef cinfo)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Find out and return the inner content type.
    @discussion Caches pointer to lookup result for future reference.
 */
extern SecAsn1Oid *
SecCmsContentInfoGetContentTypeOID(SecCmsContentInfoRef cinfo)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

/*!
    @function
    @abstract Find out and return the content encryption algorithm tag.
 */
extern SECOidTag
SecCmsContentInfoGetContentEncAlgTag(SecCmsContentInfoRef cinfo);

/*!
    @function
    @abstract Find out and return the content encryption algorithm.
    @discussion Caches pointer to lookup result for future reference.
 */
extern SECAlgorithmID *
SecCmsContentInfoGetContentEncAlg(SecCmsContentInfoRef cinfo)
    API_AVAILABLE(macos(10.4), ios(2.0));

/*! @functiongroup Message construction */

#if TARGET_OS_OSX
/*!
     @function
     @abstract Set a ContentInfos content to a Data
     @param cmsg A Message object to which the cinfo object belongs.
     @param cinfo A ContentInfo object of which we want set the content.
     @param data A pointer to a CSSM_DATA object or NULL if data will be provided during SecCmsEncoderUpdate calls.
     @param detached True if the content is to be deattched from the CMS message rather than included within it.
     @result A result code. See "SecCmsBase.h" for possible results.
     @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object.  If the call succeeds the passed in data will be owned by the reciever.  The data->Data must have been allocated using the cmsg's SecArenaPool if it is present.
     @availability 10.4 through 10.7
 */
extern OSStatus
SecCmsContentInfoSetContentData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, CSSM_DATA_PTR data, Boolean detached)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set a ContentInfos content to a Data
    @param cinfo A ContentInfo object of which we want set the content.
    @param data A CFDataRef or NULL if data will be provided during SecCmsEncoderUpdate calls.
    @param detached True if the content is to be deattched from the CMS message rather than included within it.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentData(SecCmsContentInfoRef cinfo, CFDataRef data, Boolean detached)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
/*!
     @function
     @abstract Set a ContentInfos content to a SignedData.
     @param cmsg A Message object to which the cinfo object belongs.
     @param cinfo A ContentInfo object of which we want set the content.
     @param sigd A SignedData object to set as the content of the cinfo object.
     @result A result code. See "SecCmsBase.h" for possible results.
     @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a SignedData which can be made by calling SecCmsSignedDataCreate().  If the call succeeds the passed in SignedData object will be owned by the reciever.  The Message object of the SignedData object must be the same as cmsg.
     @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentSignedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsSignedDataRef sigd)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set a ContentInfos content to a SignedData.
    @param cinfo A ContentInfo object of which we want set the content.
    @param sigd A SignedData object to set as the content of the cinfo object.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a SignedData which can be made by calling SecCmsSignedDataCreate().  If the call succeeds the passed in SignedData object will be owned by the reciever.  The Message object of the SignedData object must be the same as cmsg.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentSignedData(SecCmsContentInfoRef cinfo, SecCmsSignedDataRef sigd)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // TARGET_OS_OSX

#if TARGET_OS_OSX
/*!
     @function
     @abstract Set a ContentInfos content to a EnvelopedData.
     @param cmsg A Message object to which the cinfo object belongs.
     @param cinfo A ContentInfo object of which we want set the content.
     @param envd A EnvelopedData object to set as the content of the cinfo object.
     @result A result code. See "SecCmsBase.h" for possible results.
     @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a EnvelopedData which can be made by calling SecCmsEnvelopedDataCreate().  If the call succeeds the passed in EnvelopedData object will be owned by the reciever.  The Message object of the EnvelopedData object must be the same as cmsg.
     @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentEnvelopedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsEnvelopedDataRef envd)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set a ContentInfos content to a EnvelopedData.
    @param cinfo A ContentInfo object of which we want set the content.
    @param envd A EnvelopedData object to set as the content of the cinfo object.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a EnvelopedData which can be made by calling SecCmsEnvelopedDataCreate().  If the call succeeds the passed in EnvelopedData object will be owned by the reciever.  The Message object of the EnvelopedData object must be the same as cmsg.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentEnvelopedData(SecCmsContentInfoRef cinfo, SecCmsEnvelopedDataRef envd)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
/*!
     @function
     @abstract Set a ContentInfos content to a DigestedData.
     @param cmsg A Message object to which the cinfo object belongs.
     @param cinfo A ContentInfo object of which we want set the content.
     @param digd A DigestedData object to set as the content of the cinfo object.
     @result A result code. See "SecCmsBase.h" for possible results.
     @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a DigestedData which can be made by calling SecCmsDigestedDataCreate().  If the call succeeds the passed in DigestedData object will be owned by the reciever.  The Message object of the DigestedData object must be the same as cmsg.
     @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentDigestedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsDigestedDataRef digd)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set a ContentInfos content to a DigestedData.
    @param cinfo A ContentInfo object of which we want set the content.
    @param digd A DigestedData object to set as the content of the cinfo object.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a DigestedData which can be made by calling SecCmsDigestedDataCreate().  If the call succeeds the passed in DigestedData object will be owned by the reciever.  The Message object of the DigestedData object must be the same as cmsg.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentDigestedData(SecCmsContentInfoRef cinfo, SecCmsDigestedDataRef digd)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
/*!
 @function
 @abstract Set a ContentInfos content to a EncryptedData.
 @param cmsg A Message object to which the cinfo object belongs.
 @param cinfo A ContentInfo object of which we want set the content.
 @param encd A EncryptedData object to set as the content of the cinfo object.
 @result A result code. See "SecCmsBase.h" for possible results.
 @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a EncryptedData which can be made by calling SecCmsEncryptedDataCreate().  If the call succeeds the passed in EncryptedData object will be owned by the reciever.  The Message object of the EncryptedData object must be the same as cmsg.
 @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentEncryptedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsEncryptedDataRef encd)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Set a ContentInfos content to a EncryptedData.
    @param cinfo A ContentInfo object of which we want set the content.
    @param encd A EncryptedData object to set as the content of the cinfo object.
    @result A result code. See "SecCmsBase.h" for possible results.
    @discussion This function requires a ContentInfo object which can be made by creating a SecCmsMessage object and a EncryptedData which can be made by calling SecCmsEncryptedDataCreate().  If the call succeeds the passed in EncryptedData object will be owned by the reciever.  The Message object of the EncryptedData object must be the same as cmsg.
    @availability 10.4 and later
 */
extern OSStatus
SecCmsContentInfoSetContentEncryptedData(SecCmsContentInfoRef cinfo, SecCmsEncryptedDataRef encd)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
OSStatus
SecCmsContentInfoSetContentOther(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, CSSM_DATA_PTR data, Boolean detached, const CSSM_OID *eContentType)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
OSStatus
SecCmsContentInfoSetContentOther(SecCmsContentInfoRef cinfo, SecAsn1Item *data, Boolean detached, const SecAsn1Oid *eContentType)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
extern OSStatus
SecCmsContentInfoSetContentEncAlg(SecArenaPoolRef pool, SecCmsContentInfoRef cinfo,
                                  SECOidTag bulkalgtag, CSSM_DATA_PTR parameters, int keysize)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
extern OSStatus
SecCmsContentInfoSetContentEncAlg(SecCmsContentInfoRef cinfo,
				    SECOidTag bulkalgtag, const SecAsn1Item *parameters, int keysize)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

#if TARGET_OS_OSX
extern OSStatus
SecCmsContentInfoSetContentEncAlgID(SecArenaPoolRef pool, SecCmsContentInfoRef cinfo,
                                    SECAlgorithmID *algid, int keysize)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
extern OSStatus
SecCmsContentInfoSetContentEncAlgID(SecCmsContentInfoRef cinfo,
				    SECAlgorithmID *algid, int keysize)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !!TARGET_OS_OSX

/*!
    @function
 */
extern void
SecCmsContentInfoSetBulkKey(SecCmsContentInfoRef cinfo, SecSymmetricKeyRef bulkkey)
    API_AVAILABLE(macos(10.4),ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);

/*!
    @function
 */
extern SecSymmetricKeyRef
SecCmsContentInfoGetBulkKey(SecCmsContentInfoRef cinfo)
    API_AVAILABLE(macos(10.4),ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);

/*!
    @function
 */
extern int
SecCmsContentInfoGetBulkKeySize(SecCmsContentInfoRef cinfo);

#pragma clang diagnostic pop

__END_DECLS

#endif /* _SECURITY_SECCMSCONTENTINFO_H_ */
