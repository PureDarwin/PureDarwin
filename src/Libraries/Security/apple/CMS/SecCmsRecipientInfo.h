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
    @header SecCmsRecipientInfo.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSRECIPIENTINFO_H_
#define _SECURITY_SECCMSRECIPIENTINFO_H_  1

#include <Security/SecCmsBase.h>

__BEGIN_DECLS

#if TARGET_OS_OSX
/*!
    @function
    @abstract Create a recipientinfo
    @discussion We currently do not create KeyAgreement recipientinfos with multiple recipientEncryptedKeys
    the certificate is supposed to have been verified by the caller
 */
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreate(SecCmsMessageRef cmsg, SecCertificateRef cert)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);

#else // !TARGET_OS_OSX

/*!
    @function
    @abstract Create a recipientinfo
    @discussion We currently do not create KeyAgreement recipientinfos with multiple recipientEncryptedKeys
                the certificate is supposed to have been verified by the caller
 */
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreate(SecCmsEnvelopedDataRef envd, SecCertificateRef cert)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX


#if TARGET_OS_OSX
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyID(SecCmsMessageRef cmsg,
                                       CSSM_DATA_PTR subjKeyID,
                                       SecPublicKeyRef pubKey)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#else // !TARGET_OS_OSX
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyID(SecCmsEnvelopedDataRef envd,
                                       const SecAsn1Item *subjKeyID,
                                       SecPublicKeyRef pubKey)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX


#if TARGET_OS_OSX
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(SecCmsMessageRef cmsg,
                                               SecCertificateRef cert)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#else // !TARGET_OS_OSX
extern SecCmsRecipientInfoRef
SecCmsRecipientInfoCreateWithSubjKeyIDFromCert(SecCmsEnvelopedDataRef envd, 
                                               SecCertificateRef cert)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX


#if TARGET_OS_OSX
extern void
SecCmsRecipientInfoDestroy(SecCmsRecipientInfoRef ri);
#endif

__END_DECLS

#endif /* _SECURITY_SECCMSRECIPIENTINFO_H_ */
