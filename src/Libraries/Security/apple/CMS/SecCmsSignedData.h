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
    @header SecCmsSignedData.h

    @availability 10.4 and later
    @abstract Interfaces of the CMS implementation.
    @discussion The functions here implement functions for encoding
                and decoding Cryptographic Message Syntax (CMS) objects
                as described in rfc3369.
 */

#ifndef _SECURITY_SECCMSSIGNEDDATA_H_
#define _SECURITY_SECCMSSIGNEDDATA_H_  1

#include <Security/SecCmsBase.h>
#include <Security/SecTrust.h>

__BEGIN_DECLS

/*!
    @function
    @abstract Create a new SecCmsSignedData object.
    @param cmsg Pointer to a SecCmsMessage in which this SecCmsSignedData
        should be created.
 */
extern SecCmsSignedDataRef
SecCmsSignedDataCreate(SecCmsMessageRef cmsg);

/*!
    @function
 */
extern void
SecCmsSignedDataDestroy(SecCmsSignedDataRef sigd);

/*!
    @function
    @abstract Retrieve the SignedData's signer list.
 */
extern SecCmsSignerInfoRef *
SecCmsSignedDataGetSignerInfos(SecCmsSignedDataRef sigd);

/*!
    @function
 */
extern int
SecCmsSignedDataSignerInfoCount(SecCmsSignedDataRef sigd);

/*!
    @function
 */
extern SecCmsSignerInfoRef
SecCmsSignedDataGetSignerInfo(SecCmsSignedDataRef sigd, int i);

/*!
    @function
    @abstract Retrieve the SignedData's digest algorithm list.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern SECAlgorithmID **
SecCmsSignedDataGetDigestAlgs(SecCmsSignedDataRef sigd);
#pragma clang diagnostic pop

/*!
    @function
    @abstract Return pointer to this signedData's contentinfo.
 */
extern SecCmsContentInfoRef
SecCmsSignedDataGetContentInfo(SecCmsSignedDataRef sigd);

/*!
    @function
    @discussion XXX Should be obsoleted.
 */
extern OSStatus
SecCmsSignedDataImportCerts(SecCmsSignedDataRef sigd, SecKeychainRef keychain,
				SECCertUsage certusage, Boolean keepcerts);

/*!
    @function
    @abstract See if we have digests in place.
 */
extern Boolean
SecCmsSignedDataHasDigests(SecCmsSignedDataRef sigd);

/*!
    @function
    @abstract Check the signatures.
    @discussion The digests were either calculated during decoding (and are stored in the
                signedData itself) or set after decoding using SecCmsSignedDataSetDigests.

                The verification checks if the signing cert is valid and has a trusted chain
                for the purpose specified by "policies".

                If trustRef is NULL the cert chain is verified and the VerificationStatus is set accordingly.
                Otherwise a SecTrust object is returned for the caller to evaluate using SecTrustEvaluate().
 */
extern OSStatus
SecCmsSignedDataVerifySignerInfo(SecCmsSignedDataRef sigd, int i, SecKeychainRef keychainOrArray,
				 CFTypeRef policies, SecTrustRef *trustRef);

/*!
    @function
    @abstract Verify the certs in a certs-only message.
*/
extern OSStatus
SecCmsSignedDataVerifyCertsOnly(SecCmsSignedDataRef sigd, 
                                  SecKeychainRef keychainOrArray, 
                                  CFTypeRef policies);

/*!
    @function
 */
extern OSStatus
SecCmsSignedDataAddCertList(SecCmsSignedDataRef sigd, CFArrayRef certlist);

/*!
    @function
    @abstract Add cert and its entire chain to the set of certs.
 */
extern OSStatus
SecCmsSignedDataAddCertChain(SecCmsSignedDataRef sigd, SecCertificateRef cert);

/*!
    @function
 */
extern OSStatus
SecCmsSignedDataAddCertificate(SecCmsSignedDataRef sigd, SecCertificateRef cert);

/*!
    @function
 */
extern Boolean
SecCmsSignedDataContainsCertsOrCrls(SecCmsSignedDataRef sigd);


#if TARGET_OS_OSX
/*!
     @function
     @abstract Retrieve the SignedData's certificate list.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern CSSM_DATA_PTR *
SecCmsSignedDataGetCertificateList(SecCmsSignedDataRef sigd)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#else // !TARGET_OS_OSX
/*!
    @function
    @abstract Retrieve the SignedData's certificate list.
 */
extern SecAsn1Item * *
SecCmsSignedDataGetCertificateList(SecCmsSignedDataRef sigd)
    API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macCatalyst);
#endif // !TARGET_OS_OSX

/*!
    @function
    @abstract Create a certs-only SignedData.
    @param cert Base certificate that will be included
    @param include_chain If true, include the complete cert chain for cert.
    @discussion More certs and chains can be added via AddCertificate and AddCertChain.
    @result An error results in a return value of NULL and an error set.
 */
extern SecCmsSignedDataRef
SecCmsSignedDataCreateCertsOnly(SecCmsMessageRef cmsg, SecCertificateRef cert, Boolean include_chain);

#if TARGET_OS_IPHONE
/*!
	@function
    @abstract Finalize the digests in digestContext and apply them to sigd.
    @param sigd A SecCmsSignedDataRef for which the digests have been calculated
    @param digestContext A digestContext created with SecCmsDigestContextStartMultiple.
	@result The digest will have been applied to sigd.  After this call completes sigd is ready to accept
	SecCmsSignedDataVerifySignerInfo() calls.  The caller should still destroy digestContext with a SecCmsDigestContextDestroy() call.

 */
extern OSStatus SecCmsSignedDataSetDigestContext(SecCmsSignedDataRef sigd,
												 SecCmsDigestContextRef digestContext)
     API_AVAILABLE(ios(2.0), tvos(2.0), watchos(1.0)) API_UNAVAILABLE(macos, macCatalyst);
#endif

#if TARGET_OS_OSX
extern OSStatus
SecCmsSignedDataAddSignerInfo(SecCmsSignedDataRef sigd,
                              SecCmsSignerInfoRef signerinfo);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
extern OSStatus
SecCmsSignedDataSetDigests(SecCmsSignedDataRef sigd,
                           SECAlgorithmID **digestalgs,
                           CSSM_DATA_PTR *digests)
    API_AVAILABLE(macos(10.4)) API_UNAVAILABLE(macCatalyst);
#pragma clang diagnostic pop
#endif

__END_DECLS

#endif /* _SECURITY_SECCMSSIGNEDDATA_H_ */
