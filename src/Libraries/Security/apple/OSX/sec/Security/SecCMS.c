/*
 * Copyright (c) 2008-2010,2012-2014 Apple Inc. All Rights Reserved.
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
 *
 */

/*
 *  signed_data.c
 *  Security
 *
 *
 */
#include <AssertMacros.h>

#include <TargetConditionals.h>
#if TARGET_OS_OSX
#define ENABLE_CMS 0
#else
#define ENABLE_CMS 1
#endif

#if ENABLE_CMS
#include <Security/SecBase.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsRecipientInfo.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/cmstpriv.h>

#include <CoreFoundation/CFData.h>

#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecCertificatePriv.h>

#include <Security/SecCMS.h>
#include <Security/SecTrustPriv.h>


#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>
#include <Security/SecAsn1Item.h>
#include <security_smime/secoid.h>
#include <security_smime/cmslocal.h>


CFTypeRef kSecCMSSignDigest = CFSTR("kSecCMSSignDigest");
CFTypeRef kSecCMSSignDetached = CFSTR("kSecCMSSignDetached");
CFTypeRef kSecCMSCertChainMode = CFSTR("kSecCMSCertChainMode");
CFTypeRef kSecCMSCertChainModeNone = CFSTR("0");
CFTypeRef kSecCMSAdditionalCerts = CFSTR("kSecCMSAdditionalCerts");
CFTypeRef kSecCMSSignedAttributes = CFSTR("kSecCMSSignedAttributes");
CFTypeRef kSecCMSSignDate = CFSTR("kSecCMSSignDate");
CFTypeRef kSecCMSAllCerts = CFSTR("kSecCMSAllCerts");
CFTypeRef kSecCMSHashAgility = CFSTR("kSecCMSHashAgility");
CFTypeRef kSecCMSHashAgilityV2 = CFSTR("kSecCMSHashAgilityV2");
CFTypeRef kSecCMSExpirationDate = CFSTR("kSecCMSExpirationDate");

CFTypeRef kSecCMSBulkEncryptionAlgorithm = CFSTR("kSecCMSBulkEncryptionAlgorithm");
CFTypeRef kSecCMSEncryptionAlgorithmDESCBC = CFSTR("kSecCMSEncryptionAlgorithmDESCBC");
CFTypeRef kSecCMSEncryptionAlgorithmAESCBC = CFSTR("kSecCMSEncryptionAlgorithmAESCBC");

CFTypeRef kSecCMSSignHashAlgorithm = CFSTR("kSecCMSSignHashAlgorithm");
CFTypeRef kSecCMSHashingAlgorithmMD5 = CFSTR("kSecCMSHashingAlgorithmMD5");
CFTypeRef kSecCMSHashingAlgorithmSHA1 = CFSTR("kSecCMSHashingAlgorithmSHA1");
CFTypeRef kSecCMSHashingAlgorithmSHA256 = CFSTR("kSecCMSHashingAlgorithmSHA256");
CFTypeRef kSecCMSHashingAlgorithmSHA384 = CFSTR("kSecCMSHashingAlgorithmSHA384");
CFTypeRef kSecCMSHashingAlgorithmSHA512 = CFSTR("kSecCMSHashingAlgorithmSHA512");

OSStatus SecCMSCreateEnvelopedData(CFTypeRef recipient_or_cfarray_thereof, 
    CFDictionaryRef params, CFDataRef data, CFMutableDataRef enveloped_data)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsEnvelopedDataRef envd = NULL;
    SECOidTag algorithmTag = SEC_OID_DES_EDE3_CBC;
    int keySize = 192;
    OSStatus status = errSecParam;
    
    if (params) {
        CFStringRef algorithm_name = CFDictionaryGetValue(params, kSecCMSBulkEncryptionAlgorithm);
        if (algorithm_name) {
            if (CFEqual(kSecCMSEncryptionAlgorithmDESCBC, algorithm_name)) {
                algorithmTag = SEC_OID_DES_CBC;
                keySize = 64;
            } else if (CFEqual(kSecCMSEncryptionAlgorithmAESCBC, algorithm_name)) {
                algorithmTag = SEC_OID_AES_128_CBC;
                keySize = 128;
            }
        }
    }
    
    require(cmsg = SecCmsMessageCreate(), out);
	require(envd = SecCmsEnvelopedDataCreate(cmsg, algorithmTag, keySize), out);
    require(cinfo = SecCmsMessageGetContentInfo(cmsg), out);
    require_noerr(SecCmsContentInfoSetContentEnvelopedData(cinfo, envd), out);
    require(cinfo = SecCmsEnvelopedDataGetContentInfo(envd), out);
    require_noerr(SecCmsContentInfoSetContentData(cinfo, NULL, false), out);
    // == wrapper of: require(SECSuccess == SecCmsContentInfoSetContent(cinfo, SEC_OID_PKCS7_DATA, NULL), out);
    
    if (CFGetTypeID(recipient_or_cfarray_thereof) == CFArrayGetTypeID()) {
        CFIndex dex, numCerts = CFArrayGetCount(recipient_or_cfarray_thereof);
        for(dex=0; dex<numCerts; dex++) {
            SecCertificateRef recip = 
                (SecCertificateRef)CFArrayGetValueAtIndex(recipient_or_cfarray_thereof, dex);
            SecCmsRecipientInfoRef rinfo;
            require(rinfo = SecCmsRecipientInfoCreate(envd, recip), out);
        }
    } else if (CFGetTypeID(recipient_or_cfarray_thereof) == SecCertificateGetTypeID()) {
            require(SecCmsRecipientInfoCreate(envd, (SecCertificateRef)recipient_or_cfarray_thereof), out);
    } else
        goto out;
    
    SecAsn1Item input = {};
    if (data) {
        input.Length = CFDataGetLength(data);
        input.Data = (uint8_t*)CFDataGetBytePtr(data);
    }
    require_noerr(SecCmsMessageEncode(cmsg, (data && input.Length) ? &input : NULL, enveloped_data), out);
    
    status = errSecSuccess;
out:
    if (cmsg) SecCmsMessageDestroy(cmsg);
    return status;
}

OSStatus SecCMSDecryptEnvelopedData(CFDataRef message, 
    CFMutableDataRef data, SecCertificateRef *recipient)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsEnvelopedDataRef envd = NULL;
    SecCertificateRef used_recipient = NULL;
    OSStatus status = errSecParam;
    
    SecAsn1Item encoded_message = { CFDataGetLength(message), (uint8_t*)CFDataGetBytePtr(message) };
    require_noerr_action_quiet(SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg), 
        out, status = errSecDecode);
    require_quiet(cinfo = SecCmsMessageContentLevel(cmsg, 0), out);
    require_quiet(SecCmsContentInfoGetContentTypeTag(cinfo) == SEC_OID_PKCS7_ENVELOPED_DATA, out);
    require_quiet(envd = (SecCmsEnvelopedDataRef)SecCmsContentInfoGetContent(cinfo), out);
    SecCmsRecipientInfoRef *rinfo = envd->recipientInfos;
    while (!used_recipient && *rinfo) {
        used_recipient = (*rinfo)->cert;
        rinfo++;
    }
    require_quiet(2 == SecCmsMessageContentLevelCount(cmsg), out);
    require_quiet(cinfo = SecCmsMessageContentLevel(cmsg, 1), out);
    require_quiet(SecCmsContentInfoGetContentTypeTag(cinfo) == SEC_OID_PKCS7_DATA, out);
    const SecAsn1Item *content = SecCmsMessageGetContent(cmsg);
    if (content)
        CFDataAppendBytes(data, content->Data, content->Length);
    if (recipient) {
        CFRetainSafe(used_recipient);
        *recipient = used_recipient;
    }
    status = errSecSuccess;
out:
    if (cmsg) SecCmsMessageDestroy(cmsg);
    return status;
}

static SecCmsAttribute *
make_attr(PLArenaPool *poolp, SecAsn1Item *type, SecAsn1Item *value, bool encoded)
{
    SecAsn1Item * copiedvalue;
    SecCmsAttribute *attr = (SecCmsAttribute *)PORT_ArenaZAlloc(poolp, sizeof(SecCmsAttribute));
    if (attr == NULL)
        goto loser;

    if (SECITEM_CopyItem(poolp, &(attr->type), type) != SECSuccess)
        goto loser;

    if (value != NULL) {
        if ((copiedvalue = SECITEM_AllocItem(poolp, NULL, value->Length)) == NULL)
            goto loser;

        if (SECITEM_CopyItem(poolp, copiedvalue, value) != SECSuccess)
            goto loser;

        if (SecCmsArrayAdd(poolp, (void ***)&(attr->values), (void *)copiedvalue) != SECSuccess)
            goto loser;
    }

    attr->encoded = encoded;
    
loser:
    return attr;
}

static void
signerinfo_add_auth_attr(SecCmsSignerInfoRef signerinfo, /*SECOidTag oidtag*/
    SecAsn1Item *oid, SecAsn1Item *value, bool encoded)
{
    PLArenaPool *poolp  = signerinfo->signedData->contentInfo.cmsg->poolp;
    PORT_Assert (poolp != NULL);
    void *mark = PORT_ArenaMark (poolp);
    
    SecCmsAttribute *attr = make_attr(poolp, oid, value, encoded);
    if (!attr || SecCmsAttributeArrayAddAttr(poolp, &(signerinfo->authAttr), attr) != SECSuccess)
            goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return;
}

static void sign_all_attributes(const void *key, const void *value, void *context)
{
    SecAsn1Item oid = { CFDataGetLength(key), (uint8_t*)CFDataGetBytePtr(key) },
            oid_value = { CFDataGetLength(value), (uint8_t*)CFDataGetBytePtr(value) };
    
    signerinfo_add_auth_attr((SecCmsSignerInfoRef)context, &oid, &oid_value, true);
}

#if 0
static void enveloped_data_add_unprotected_attr(SecCmsEnvelopedDataRef envd,
    SecAsn1Item *oid, SecAsn1Item *value, bool encoded)
{
    PLArenaPool *poolp  = envd->contentInfo.cmsg->poolp;
    PORT_Assert (poolp != NULL);
    void *mark = PORT_ArenaMark (poolp);
    SecCmsAttribute *attr = make_attr(poolp, oid, value, encoded);

    if (!attr || SecCmsAttributeArrayAddAttr(
        poolp, 
        &(envd->unprotectedAttr), attr) != SECSuccess)
            goto loser;

    PORT_ArenaUnmark (poolp, mark);
    return;

loser:
    PORT_Assert (mark != NULL);
    PORT_ArenaRelease (poolp, mark);
    return;

}
#endif

static OSStatus SecCMSSignDataOrDigestAndAttributes(SecIdentityRef identity, 
    CFDataRef data, bool detached, bool data_is_digest, SECOidTag sign_algorithm,
    CFMutableDataRef signed_data, CFDictionaryRef signed_attributes, SecCmsCertChainMode chainMode, CFArrayRef additional_certs)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;
    SecCmsSignerInfoRef signerinfo;
    OSStatus status = errSecParam;
    
    require(!data_is_digest || detached /* if digest, must be detached */, out);

    require(cmsg = SecCmsMessageCreate(), out);
    require(sigd = SecCmsSignedDataCreate(cmsg), out);
    require(cinfo = SecCmsMessageGetContentInfo(cmsg), out);
    require_noerr(SecCmsContentInfoSetContentSignedData(cinfo, sigd), out);
    require(cinfo = SecCmsSignedDataGetContentInfo(sigd), out);
    require_noerr(SecCmsContentInfoSetContentData(cinfo, NULL, detached), out);
    require(signerinfo = SecCmsSignerInfoCreate(sigd, identity, sign_algorithm), out);
    if (additional_certs)
        require_noerr(SecCmsSignedDataAddCertList(sigd, additional_certs), out);
    require_noerr(SecCmsSignerInfoIncludeCerts(signerinfo, chainMode, certUsageAnyCA), out);
    require_noerr(SecCmsSignerInfoAddSigningTime(signerinfo, CFAbsoluteTimeGetCurrent()), out);

    if (signed_attributes)
        CFDictionaryApplyFunction(signed_attributes, sign_all_attributes, signerinfo);
    
    SecAsn1Item input = {};
    if (data) {
        input.Length = CFDataGetLength(data);
        input.Data = (uint8_t*)CFDataGetBytePtr(data);
    }
    if (data_is_digest) {
        require_noerr(SecCmsSignedDataSetDigestValue(sigd, sign_algorithm, &input), out);
        require_noerr(SecCmsMessageEncode(cmsg, NULL, signed_data), out);
    }
    else
        require_noerr(SecCmsMessageEncode(cmsg, (data && input.Length) ? &input : NULL, signed_data), out);
    
    status = errSecSuccess;
out:
    if (cmsg) SecCmsMessageDestroy(cmsg);
    return status;
}

OSStatus SecCMSSignDataAndAttributes(SecIdentityRef identity, CFDataRef data, bool detached, 
    CFMutableDataRef signed_data, CFDictionaryRef signed_attributes)
{
    return SecCMSSignDataOrDigestAndAttributes(identity, data, detached, false, SEC_OID_SHA1,
        signed_data, signed_attributes, SecCmsCMCertChain, NULL);
}

OSStatus SecCMSSignDigestAndAttributes(SecIdentityRef identity, CFDataRef digest, 
    CFMutableDataRef signed_data, CFDictionaryRef signed_attributes)
{
    return SecCMSSignDataOrDigestAndAttributes(identity, digest, true, true, SEC_OID_SHA1,
        signed_data, signed_attributes, SecCmsCMCertChain, NULL);
}


OSStatus SecCMSCreateSignedData(SecIdentityRef identity, CFDataRef data,
    CFDictionaryRef parameters, CFDictionaryRef signed_attributes,
    CFMutableDataRef signed_data)
{
    bool is_digest = false, is_detached = false;
    CFStringRef algorithm_name = NULL;
    SecCmsCertChainMode chain_mode = SecCmsCMCertChain;
    CFArrayRef additional_certs = NULL;

    if (parameters) {
        is_digest = CFDictionaryGetValueIfPresent(parameters, 
                        kSecCMSSignDigest, NULL);
        is_detached = CFDictionaryGetValueIfPresent(parameters, 
                        kSecCMSSignDetached, NULL);
        algorithm_name = CFDictionaryGetValue(parameters,
                        kSecCMSSignHashAlgorithm);

        CFTypeRef chain_mode_param = CFDictionaryGetValue(parameters, kSecCMSCertChainMode);
        if (chain_mode_param && (CFGetTypeID(chain_mode_param) == CFStringGetTypeID()))
            chain_mode = CFStringGetIntValue(chain_mode_param);

        CFTypeRef additional_certs_param = CFDictionaryGetValue(parameters, kSecCMSAdditionalCerts);
        if (additional_certs_param && (CFGetTypeID(additional_certs_param) == CFArrayGetTypeID()))
            additional_certs = (CFArrayRef)additional_certs_param;
    }
    
    SECOidTag algorithm = SEC_OID_SHA1;
    if (algorithm_name) {
        if (CFEqual(kSecCMSHashingAlgorithmSHA1, algorithm_name)) {
            algorithm = SEC_OID_SHA1;
        } else if (CFEqual(kSecCMSHashingAlgorithmSHA256, algorithm_name)) {
            algorithm = SEC_OID_SHA256;
        } else if (CFEqual(kSecCMSHashingAlgorithmSHA384, algorithm_name)) {
            algorithm = SEC_OID_SHA384;
        } else if (CFEqual(kSecCMSHashingAlgorithmSHA512, algorithm_name)) {
            algorithm = SEC_OID_SHA512;
        } else {
            return errSecParam;
        }
    }
    
    return SecCMSSignDataOrDigestAndAttributes(identity, data, 
        is_detached, is_digest, algorithm,
        signed_data, signed_attributes, chain_mode, additional_certs);
}


static CFMutableArrayRef copy_signed_attribute_values(SecCmsAttribute *attr)
{
    CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    SecAsn1Item **item = attr->values;
    if (item) while (*item) {
        CFDataRef asn1data = CFDataCreate(kCFAllocatorDefault, (*item)->Data, (*item)->Length);
        if (asn1data) {
            CFArrayAppendValue(array, asn1data);
            CFRelease(asn1data);
        }
        item++;
    }
    return array;
}

static OSStatus SecCMSVerifySignedData_internal(CFDataRef message, CFDataRef detached_contents,
    CFTypeRef policy, SecTrustRef *trustref, CFArrayRef additional_certs,
    CFDataRef *attached_contents, CFDictionaryRef *signed_attributes)
{
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;
    OSStatus status = errSecParam;

    require(message, out);
    SecAsn1Item encoded_message = { CFDataGetLength(message), (uint8_t*)CFDataGetBytePtr(message) };
    require_noerr_action_quiet(SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg), 
        out, status = errSecDecode);
    /* expected to be a signed data message at the top level */
    require_quiet(cinfo = SecCmsMessageContentLevel(cmsg, 0), out);
    require_quiet(SecCmsContentInfoGetContentTypeTag(cinfo) == SEC_OID_PKCS7_SIGNED_DATA, out);
    require_quiet(sigd = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(cinfo), out);

    if (detached_contents)
    {
        require_quiet(!SecCmsSignedDataHasDigests(sigd), out);
        SECAlgorithmID **digestalgs = SecCmsSignedDataGetDigestAlgs(sigd);
        SecCmsDigestContextRef digcx = SecCmsDigestContextStartMultiple(digestalgs);
        SecCmsDigestContextUpdate(digcx, CFDataGetBytePtr(detached_contents), CFDataGetLength(detached_contents));
        SecCmsSignedDataSetDigestContext(sigd, digcx);
        SecCmsDigestContextDestroy(digcx);
    }

    if (additional_certs)
        require_noerr_quiet(SecCmsSignedDataAddCertList(sigd, additional_certs), out);

    if (policy) { /* if no policy is given skip verification */
        /* find out about signers */
        int nsigners = SecCmsSignedDataSignerInfoCount(sigd);
        require_quiet(nsigners == 1, out);
        require_noerr_action_quiet(SecCmsSignedDataVerifySignerInfo(sigd, 0, NULL, policy, trustref), 
            out, status = errSecAuthFailed);
    }
    
#if 0
    if (nsigners > 1)
        trustrefs = CFArrayCreateMutable(kCFAllocatorDefault, nsigners, &kCFTypeArrayCallBacks);
        
    int j;
    for (j = 0; j < nsigners; j++)
    {
        SecTrustRef trustRef;
        require_noerr_action_quiet(SecCmsSignedDataVerifySignerInfo(sigd, j, NULL, policy, &trustRef),
            out, status = errSecAuthFailed);
        if ((j == 0) && (nsigners == 1))
            *trustref_or_cfarray_thereof = trustRef;
        else {
            CFArrayAppendValue(trustrefs, trustRef);
            CFRelease(trustRef);
        }
    }
    *trustref_or_cfarray_thereof = trustrefs;
    trustrefs = NULL;
#endif

    status = errSecSuccess;

    if (attached_contents) {
        const SecAsn1Item *content = SecCmsMessageGetContent(cmsg);
        if (content)
            *attached_contents = CFDataCreate(kCFAllocatorDefault, content->Data, content->Length);
        else
            *attached_contents = NULL;
    }
    
    if (signed_attributes) {
        CFMutableDictionaryRef attrs = CFDictionaryCreateMutable(kCFAllocatorDefault, 
            0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_quiet(attrs, out);
        SecCmsAttribute **signed_attrs = sigd->signerInfos[0]->authAttr;
        if (signed_attrs) while (*signed_attrs) {
            CFDataRef type = CFDataCreate(kCFAllocatorDefault, (*signed_attrs)->type.Data, (*signed_attrs)->type.Length);
            if (type) {
                CFMutableArrayRef attr = copy_signed_attribute_values(*signed_attrs);
                if (attr) {
                    CFMutableArrayRef existing_attrs = (CFMutableArrayRef)CFDictionaryGetValue(attrs, type);
                    if (existing_attrs) {
                        CFIndex count = CFArrayGetCount(attr);
                        if (count)
                            CFArrayAppendArray(existing_attrs, attr, CFRangeMake(0, count));
                    } else
                        CFDictionarySetValue(attrs, type, attr);
                    CFRelease(attr);
                }
                CFRelease(type);
            }
            signed_attrs++;
        }
        CFMutableArrayRef certs = NULL;
        
        SecAsn1Item **cert_datas = SecCmsSignedDataGetCertificateList(sigd);
        certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        SecAsn1Item *cert_data;
        if (cert_datas) while ((cert_data = *cert_datas) != NULL) {
            SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, cert_data->Data, cert_data->Length);
            if (cert) {
                CFArrayAppendValue(certs, cert);
                CFRelease(cert);
            }
            cert_datas++;
        }
        
        CFDictionaryAddValue(attrs, kSecCMSAllCerts, certs);

        /* Add "cooked" values separately */
        CFAbsoluteTime signing_time;
        if (errSecSuccess == SecCmsSignerInfoGetSigningTime(sigd->signerInfos[0], &signing_time)) {
                CFDateRef signing_date = CFDateCreate(kCFAllocatorDefault, signing_time);
            if (signing_date){
                CFDictionarySetValue(attrs, kSecCMSSignDate, signing_date);
                CFReleaseSafe(signing_date);
            }
        }

        CFDataRef hash_agility_value = NULL;
        if (errSecSuccess == SecCmsSignerInfoGetAppleCodesigningHashAgility(sigd->signerInfos[0], &hash_agility_value)) {
            if (hash_agility_value) {
                CFDictionarySetValue(attrs, kSecCMSHashAgility, hash_agility_value);
            }
        }

        CFDictionaryRef hash_agility_values = NULL;
        if (errSecSuccess == SecCmsSignerInfoGetAppleCodesigningHashAgilityV2(sigd->signerInfos[0], &hash_agility_values)) {
            if (hash_agility_values) {
                CFDictionarySetValue(attrs, kSecCMSHashAgilityV2, hash_agility_values);
            }
        }

        CFAbsoluteTime expiration_time;
        if (errSecSuccess == SecCmsSignerInfoGetAppleExpirationTime(sigd->signerInfos[0], &expiration_time)) {
            CFDateRef expiration_date = CFDateCreate(NULL, expiration_time);
            if (expiration_date) {
                CFDictionarySetValue(attrs, kSecCMSExpirationDate, expiration_date);
                CFReleaseSafe(expiration_date);
            }
        }

        *signed_attributes = attrs;
        CFReleaseSafe(certs);
    }
    
    
out:
    if (cmsg) SecCmsMessageDestroy(cmsg);
    return status;
}

OSStatus SecCMSVerifyCopyDataAndAttributes(CFDataRef message, CFDataRef detached_contents,
    CFTypeRef policy, SecTrustRef *trustref,
    CFDataRef *attached_contents, CFDictionaryRef *signed_attributes)
{
    OSStatus status = SecCMSVerifySignedData_internal(message, detached_contents, policy, trustref, NULL, attached_contents, signed_attributes);
    
    return status;
}

OSStatus SecCMSVerifySignedData(CFDataRef message, CFDataRef detached_contents,
    CFTypeRef policy, SecTrustRef *trustref, CFArrayRef additional_certificates,
    CFDataRef *attached_contents, CFDictionaryRef *message_attributes)
{
    CFDictionaryRef signed_attributes = NULL;
    OSStatus status = SecCMSVerifySignedData_internal(message, detached_contents, policy, trustref, additional_certificates, attached_contents, &signed_attributes);
    if (!status && signed_attributes && message_attributes) {
        *message_attributes = CFDictionaryCreate(kCFAllocatorDefault, &kSecCMSSignedAttributes, (const void **)&signed_attributes, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    CFReleaseSafe(signed_attributes);
   
    return status;
}

OSStatus SecCMSVerify(CFDataRef message, CFDataRef detached_contents,
    CFTypeRef policy, SecTrustRef *trustref,
    CFDataRef *attached_contents) {
        return SecCMSVerifySignedData_internal(message, detached_contents, policy, trustref, NULL, attached_contents, NULL);
}

CFArrayRef SecCMSCertificatesOnlyMessageCopyCertificates(CFDataRef message) {
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;
    CFMutableArrayRef certs = NULL;

    if (!message) {
        return NULL;
    }

    SecAsn1Item encoded_message = { CFDataGetLength(message), (uint8_t*)CFDataGetBytePtr(message) };
    require_noerr_quiet(SecCmsMessageDecode(&encoded_message, NULL, NULL, NULL, NULL, NULL, NULL, &cmsg), out);
    /* expected to be a signed data message at the top level */
    require(cinfo = SecCmsMessageContentLevel(cmsg, 0), out);
    require(SecCmsContentInfoGetContentTypeTag(cinfo) == SEC_OID_PKCS7_SIGNED_DATA, out);
    require(sigd = (SecCmsSignedDataRef)SecCmsContentInfoGetContent(cinfo), out);

    /* find out about signers */
    int nsigners = SecCmsSignedDataSignerInfoCount(sigd);
    require(nsigners == 0, out);

    SecAsn1Item **cert_datas = SecCmsSignedDataGetCertificateList(sigd);
    certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    SecAsn1Item *cert_data;
    if (cert_datas) while ((cert_data = *cert_datas) != NULL) {
        SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, cert_data->Data, cert_data->Length);
        if (cert) {
            CFArrayAppendValue(certs, cert);
            CFRelease(cert); 
        }
        cert_datas++;
    }

out:
    if (cmsg) { SecCmsMessageDestroy(cmsg); }
    if (certs && CFArrayGetCount(certs) < 1) {
        CFReleaseNull(certs);
    }

    return certs;
}


extern const SecAsn1Template SecCmsMessageTemplate[];

CFDataRef SecCMSCreateCertificatesOnlyMessage(CFTypeRef cert_or_array_thereof) {
    OSStatus status = errSecParam;
    SecCmsMessageRef cmsg = NULL;
    SecCmsContentInfoRef cinfo;
    SecCmsSignedDataRef sigd = NULL;
    CFMutableDataRef cert_only_signed_data = NULL;
    CFArrayRef cert_array = NULL;
    CFIndex cert_array_count = 0;
    SecCertificateRef cert = NULL;
    
    require(cert_or_array_thereof, out);

    require(cmsg = SecCmsMessageCreate(), out);
    require(sigd = SecCmsSignedDataCreate(cmsg), out);
    require_noerr(SecCmsContentInfoSetContentData(&(sigd->contentInfo), NULL, PR_TRUE), out);
    require(cinfo = SecCmsMessageGetContentInfo(cmsg), out);
    require_noerr(SecCmsContentInfoSetContentSignedData(cinfo, sigd), out);
    long version = SEC_CMS_SIGNED_DATA_VERSION_BASIC;
    require(SEC_ASN1EncodeInteger(cmsg->poolp, &(sigd->version), version), out);
    
    if (CFGetTypeID(cert_or_array_thereof) == SecCertificateGetTypeID()) {
        cert_array = CFArrayCreate(kCFAllocatorDefault, &cert_or_array_thereof, 1, &kCFTypeArrayCallBacks);
    } else if (CFGetTypeID(cert_or_array_thereof) == CFArrayGetTypeID()) {
        cert_array = CFArrayCreateCopy(kCFAllocatorDefault, (CFArrayRef)cert_or_array_thereof);
    }

    require(cert_array, out);
    cert_array_count = CFArrayGetCount(cert_array);
    require(cert_array_count > 0, out);

    sigd->rawCerts = (SecAsn1Item * *)PORT_ArenaAlloc(cmsg->poolp, (cert_array_count + 1) * sizeof(SecAsn1Item *));
    require(sigd->rawCerts, out);
    CFIndex ix;
    for (ix = 0; ix < cert_array_count; ix++) {
        cert = (SecCertificateRef)CFArrayGetValueAtIndex(cert_array, ix);
        require(cert, out);
        
        sigd->rawCerts[ix] = PORT_ArenaZAlloc(cmsg->poolp, sizeof(SecAsn1Item));
        SecAsn1Item cert_data = { SecCertificateGetLength(cert),
                                (uint8_t *)SecCertificateGetBytePtr(cert) };
        *(sigd->rawCerts[ix]) = cert_data;
    }
    sigd->rawCerts[ix] = NULL;

	/* this is a SET OF, so we need to sort them guys - we have the DER already, though */
    if (cert_array_count > 1)
        SecCmsArraySort((void **)sigd->rawCerts, SecCmsUtilDERCompare, NULL, NULL);
            
    cert_only_signed_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
        SecAsn1Item cert_only_signed_data_item = {};
        require_quiet(SEC_ASN1EncodeItem(cmsg->poolp, &cert_only_signed_data_item, 
            cmsg, SecCmsMessageTemplate), out);
    CFDataAppendBytes(cert_only_signed_data, cert_only_signed_data_item.Data, 
        cert_only_signed_data_item.Length);
    
    status = errSecSuccess;
out:
    CFReleaseSafe(cert_array);
    if (status) CFReleaseSafe(cert_only_signed_data);
    if (cmsg) SecCmsMessageDestroy(cmsg);
    return cert_only_signed_data;
}

CFDataRef SecCMSCreateCertificatesOnlyMessageIAP(SecCertificateRef cert)
{
    static const uint8_t header[] = {
        0x30, 0x82, 0x03, 0x6d, 0x06, 0x09, 0x2a, 0x86,
        0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
        0x82, 0x03, 0x5e, 0x30, 0x82, 0x03, 0x5a, 0x02,
        0x01, 0x01, 0x31, 0x00, 0x30, 0x0b, 0x06, 0x09,
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
        0x01, 0xa0, 0x82, 0x03, 0x40
    };

    static const uint8_t trailer[] = {
        0xa1, 0x00, 0x31, 0x00
    };

    CFMutableDataRef message = NULL;
    CFDataRef certdata;
    const uint8_t *certbytes;
    CFIndex certlen;
    uint8_t *messagebytes;
    uint16_t messagelen;

    certdata = SecCertificateCopyData(cert);
    require(certdata, out);

    certbytes = CFDataGetBytePtr(certdata);
    certlen = CFDataGetLength(certdata);
    require(certlen > UINT8_MAX, out);
    require(certlen < UINT16_MAX, out);

    message = CFDataCreateMutable(kCFAllocatorDefault, 0);
    require(message, out);

    CFDataAppendBytes(message, header, sizeof(header));
    CFDataAppendBytes(message, certbytes, certlen);
    CFDataAppendBytes(message, trailer, sizeof(trailer));

    messagebytes = CFDataGetMutableBytePtr(message);
    messagelen = CFDataGetLength(message);

    messagelen -= 4;
    messagebytes[2] = messagelen >> 8;
    messagebytes[3] = messagelen & 0xFF;

    messagelen -= 15;
    messagebytes[17] = messagelen >> 8;
    messagebytes[18] = messagelen & 0xFF;

    messagelen -= 4;
    messagebytes[21] = messagelen >> 8;
    messagebytes[22] = messagelen & 0xFF;

    messagelen -= 26;
    messagebytes[43] = messagelen >> 8;
    messagebytes[44] = messagelen & 0xFF;

out:
    CFReleaseSafe(certdata);
    return message;
}

#endif /* ENABLE_CMS */

