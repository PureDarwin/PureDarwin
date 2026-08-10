/*
 * Copyright (c) 2007-2010,2012-2014 Apple Inc. All Rights Reserved.
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

#include <libDER/oids.h>
#include <security_asn1/nssUtils.h>
#include <security_asn1/SecAsn1Templates.h>
#include <security_asn1/pkcs12Templates.h>

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>

#include <CoreFoundation/CoreFoundation.h>

#include <AssertMacros.h>
#include <Security/SecInternal.h>
#include <utilities/debugging.h>

#include "p12pbegen.h"
#include "p12import.h"
#include <Security/SecImportExport.h>

#ifdef NDEBUG
#define p12DecodeLog(args...)
#else
#define p12DecodeLog(args...)         secdebug("pkcs12", "%s\n", args)
#endif

int decode_item(pkcs12_context * context, const SecAsn1Item *item, 
    const SecAsn1Template *tmpl, void *dest);
inline int decode_item(pkcs12_context * context, const SecAsn1Item *item, 
    const SecAsn1Template *tmpl, void *dest)
{
    return SecAsn1Decode(context->coder, (const char *)item->Data, item->Length, tmpl, dest);
}

void alloc_item(pkcs12_context * context, SecAsn1Item *item, size_t len);
inline void alloc_item(pkcs12_context * context, SecAsn1Item *item, size_t len)
{
    SecAsn1AllocItem(context->coder, item, len);
}

/*
 * OIDS for P12 map to the following attributes.
 */
typedef struct {
	CCAlgorithm		alg;
	uint32_t		keySizeInBits; // XXX/cs make keysize in bytes
	uint32_t		blockSizeInBytes;	// for IV, optional, make iv size in bytes
	CCOptions		options;	// padding and mode.
} PKCSOidInfo;

/* PKCS12 algorithms OID_ISO_MEMBER, OID_US, OID_RSA, OID_PKCS, OID_PKCS_12 */
static const uint8_t PKCS12_pbep[] = { 42, 134, 72, 134, 247, 13, 1, 12, 1 };
static const DERItem OID_PKCS12_pbep = { (uint8_t*)PKCS12_pbep, sizeof(PKCS12_pbep)  };
static const PKCSOidInfo pkcsOidInfos[] = {
	{   /*CSSMOID_PKCS12_pbeWithSHAAnd128BitRC4,*/
		kCCAlgorithmRC4, 128, 0/* stream cipher */, 0 },
	{   /*CSSMOID_PKCS12_pbeWithSHAAnd40BitRC4,*/
		kCCAlgorithmRC4, 40, 0/* stream cipher */, 0 },
	{   /*CSSMOID_PKCS12_pbeWithSHAAnd3Key3DESCBC,*/
		kCCAlgorithm3DES, 64 * 3, 8, kCCOptionPKCS7Padding },
	{   /*CSSMOID_PKCS12_pbeWithSHAAnd2Key3DESCBC,*/
		-1 /*CSSM_ALGID_3DES_2KEY unsupported*/, 64 * 2, 8, kCCOptionPKCS7Padding },
    {   /*CSSMOID_PKCS12_pbeWithSHAAnd128BitRC2CBC,*/
        kCCAlgorithmRC2, 128, 8, kCCOptionPKCS7Padding },
    {   /*CSSMOID_PKCS12_pbewithSHAAnd40BitRC2CBC,*/
        kCCAlgorithmRC2, 40, 8, kCCOptionPKCS7Padding }
};

#define NUM_PKCS_OID_INFOS (sizeof(pkcsOidInfos) / sizeof(pkcsOidInfos[1]))

static int pkcsOidToParams(const SecAsn1Item *oid, CCAlgorithm *alg,
	uint32_t *keySizeInBits, uint32_t *blockSizeInBytes, CCOptions *options)
{
    DERItem prefix = { oid->Data, oid->Length };
    prefix.length -= 1;
    if (DEROidCompare(&OID_PKCS12_pbep, &prefix)) {
        uint8_t postfix = oid->Data[oid->Length-1];
        if (postfix > NUM_PKCS_OID_INFOS || postfix == 4)
            return -1;
        *alg 			  = pkcsOidInfos[postfix-1].alg;
        *keySizeInBits 	  = pkcsOidInfos[postfix-1].keySizeInBits;
        *blockSizeInBytes = pkcsOidInfos[postfix-1].blockSizeInBytes;
        *options		  = pkcsOidInfos[postfix-1].options;
        return 0;
    }
	return -1;
}

static int p12DataToInt(const SecAsn1Item *cdata, uint32_t *u)
{
    /* default/not present */
    if((cdata->Length == 0) || (cdata->Data == NULL)) { 
        *u = 0;
        return 0;
    }
    size_t len = cdata->Length;
    if(len > sizeof(uint32_t)) {
            return -1;
    }
    
    uint32_t rtn = 0;
    uint8_t *cp = cdata->Data;
    size_t  i;
    for(i = 0; i < len; i++) {
            rtn = (rtn << 8) | *cp++;
    }
    *u = rtn;
    return 0;
}

/*
 * Parse an SecAsn1AlgId specific to P12.
 * Decode the alg params as a NSS_P12_PBE_Params and parse and 
 * return the result if the pbeParams is non-NULL.
 */
static int algIdParse(pkcs12_context * context, 
	const SecAsn1AlgId *algId, NSS_P12_PBE_Params *pbeParams/*optional*/)
{
	p12DecodeLog("algIdParse");
	const SecAsn1Item *param = &algId->parameters;
	require(pbeParams, out);
    require(param && param->Length, out);
	memset(pbeParams, 0, sizeof(*pbeParams));
	require_noerr(decode_item(context, param, NSS_P12_PBE_ParamsTemplate, pbeParams), out);
    
    return 0;
out:
    return -1;
}

static int p12Decrypt(pkcs12_context * context, const SecAsn1AlgId *algId,
	const SecAsn1Item *cipherText, SecAsn1Item *plainText)
{
	NSS_P12_PBE_Params pbep = {};
    // XXX/cs not requiring decoding, but if pbep is uninit this will fail later
	algIdParse(context, algId, &pbep);

	CCAlgorithm		alg = 0;
	uint32_t			keySizeInBits = 0;
	uint32_t			blockSizeInBytes = 0;	// for IV, optional
	CCOptions		options = 0;
	require_noerr_quiet(pkcsOidToParams(&algId->algorithm, &alg, &keySizeInBits, 
        &blockSizeInBytes, &options), out);

	uint32_t iterCount = 0;
	require_noerr(p12DataToInt(&pbep.iterations, &iterCount), out);

	/* P12 style key derivation */
	SecAsn1Item key = {0, NULL};
	if(keySizeInBits)
        alloc_item(context, &key, (keySizeInBits+7)/8);
    require_noerr(p12_pbe_gen(context->passphrase, pbep.salt.Data, pbep.salt.Length, 
        iterCount, PBE_ID_Key, key.Data, key.Length), out);
        
	/* P12 style IV derivation, optional */
	SecAsn1Item iv = {0, NULL};
	if(blockSizeInBytes) {
		alloc_item(context, &iv, blockSizeInBytes);
        require_noerr(p12_pbe_gen(context->passphrase, pbep.salt.Data, pbep.salt.Length, 
            iterCount, PBE_ID_IV, iv.Data, iv.Length), out);
    }

	SecAsn1Item ourPtext = {0, NULL};
    alloc_item(context, &ourPtext, cipherText->Length);
    require_noerr(CCCrypt(kCCDecrypt, alg, options/*kCCOptionPKCS7Padding*/, 
        key.Data, key.Length, iv.Data, cipherText->Data, cipherText->Length, 
        ourPtext.Data, ourPtext.Length, &ourPtext.Length), out);
    *plainText = ourPtext;

    return 0;
out:
    return -1;
}

static int emit_item(pkcs12_context * context, NSS_Attribute **attrs, 
    CFStringRef item_key, CFTypeRef item_value)
{
    int result = -1;
	/* parse attrs into friendlyName, localKeyId; ignoring generic attrs */
    CFMutableDictionaryRef attr_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(attr_dict, out);
	unsigned numAttrs = nssArraySize((const void **)attrs);
    unsigned int dex;
	for(dex = 0; dex < numAttrs; dex++) {
		NSS_Attribute *attr = attrs[dex];
		unsigned numValues = nssArraySize((const void**)attr->attrValue);
		DERItem type = { attr->attrType.Data, attr->attrType.Length };
		if(DEROidCompare(&type, &oidFriendlyName)) {
			/* 
			 * BMP string (UniCode). Spec says only one legal value.
			 */
			require(numValues == 1, out);
            SecAsn1Item friendly_name_asn1;
			require_noerr(decode_item(context, attr->attrValue[0],
					kSecAsn1BMPStringTemplate, &friendly_name_asn1), out);
            CFStringRef friendly_name = CFStringCreateWithBytes(kCFAllocatorDefault, 
                friendly_name_asn1.Data, friendly_name_asn1.Length, 
                kCFStringEncodingUnicode, true);
            if (friendly_name) {
                CFDictionarySetValue(attr_dict, kSecImportItemLabel, friendly_name);
                CFRelease(friendly_name);
            }
		}
		else if(DEROidCompare(&type, &oidLocalKeyId)) {
			/* 
			 * Octet string. Spec says only one legal value.
			 */
			require(numValues == 1, out);
            SecAsn1Item local_key_id;
			require_noerr(decode_item(context, attr->attrValue[0],
					kSecAsn1OctetStringTemplate, &local_key_id), out);
            CFDataRef keyid = CFDataCreate(kCFAllocatorDefault, local_key_id.Data, local_key_id.Length);
            if (keyid) {
                CFDictionarySetValue(attr_dict, kSecImportItemKeyID, keyid);
                CFRelease(keyid);
            }
		}
	}

    CFTypeRef key = CFDictionaryGetValue(attr_dict, kSecImportItemKeyID);
    if (!key)
        key = CFDictionaryGetValue(attr_dict, kSecImportItemLabel);
    if (!key)
        key = item_value;

    CFMutableDictionaryRef item = (CFMutableDictionaryRef)CFDictionaryGetValue(context->items, key);
    if (item) {
        CFDictionarySetValue(item, item_key, item_value);
    } else {
        CFDictionarySetValue(attr_dict, item_key, item_value);
        CFDictionarySetValue(context->items, key, attr_dict);
    }
    result = 0;
out:
    CFReleaseSafe(attr_dict);
    return result;
}


/*
 * ShroudedKeyBag parser w/decrypt
 */
static int shroudedKeyBagParse(pkcs12_context * context, const NSS_P12_SafeBag *safeBag)
{
    CFDataRef algoidData = NULL;
    CFDataRef keyData = NULL;

	p12DecodeLog("Found shrouded key bag");

	const NSS_P12_ShroudedKeyBag *keyBag = safeBag->bagValue.shroudedKeyBag;
    SecAsn1Item ptext = {0, NULL};
    require_noerr_quiet(p12Decrypt(context, &keyBag->algorithm, 
        &keyBag->encryptedData, &ptext), out);

    /* Decode PKCS#8 formatted private key */
    NSS_PrivateKeyInfo pki;
    memset(&pki, 0, sizeof(pki));
	require_noerr(decode_item(context, &ptext, kSecAsn1PrivateKeyInfoTemplate,
			&pki), out);

    DERItem algorithm = { pki.algorithm.algorithm.Data, pki.algorithm.algorithm.Length };
    algoidData = NULL;
    if (DEROidCompare(&oidEcPubKey, &algorithm)) {
        algoidData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, oidEcPubKey.data, oidEcPubKey.length, kCFAllocatorNull);
    } else if (DEROidCompare(&oidRsa, &algorithm)) {
        algoidData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, oidRsa.data, oidRsa.length, kCFAllocatorNull);
    } else {
        goto out;
    }
    require_noerr(emit_item(context, safeBag->bagAttrs, CFSTR("algid"), algoidData), out);
    CFReleaseNull(algoidData);

    keyData = CFDataCreate(kCFAllocatorDefault, pki.privateKey.Data, pki.privateKey.Length);
    require_noerr(emit_item(context, safeBag->bagAttrs, CFSTR("key"), keyData), out);
    CFReleaseNull(keyData);

    return 0;
out:
    CFReleaseSafe(algoidData);
    CFReleaseSafe(keyData);
    return -1;
}


/*
 * CertBag parser
 */
static int certBagParse(pkcs12_context * context, const NSS_P12_SafeBag *safeBag)
{
    CFDataRef certData = NULL;
	p12DecodeLog("found certBag");
	NSS_P12_CertBag *certBag = safeBag->bagValue.certBag;

	switch(certBag->type) {
		case CT_X509:
        {
            /* certType = CSSM_CERT_X_509v3;
			   certEncoding = CSSM_CERT_ENCODING_DER; */
            certData = CFDataCreate(kCFAllocatorDefault, certBag->certValue.Data,
                certBag->certValue.Length);
            require_noerr(emit_item(context, safeBag->bagAttrs, CFSTR("cert"), certData), out);
            CFRelease(certData);
            break;
        }
		case CT_SDSI:
            /* certType = CSSM_CERT_SDSIv1; */
			/* it's base64 encoded - no value for that in this enum */
			break;
		default:
            return -1;
	}
    return 0;
out:
    CFReleaseSafe(certData);
    return -1;
}


/*
 * Parse an encoded NSS_P12_SafeContents. This could be either 
 * present as plaintext in an AuthSafe or decrypted. 
 */
static int safeContentsParse(pkcs12_context * context, const SecAsn1Item *contentsBlob)
{
	p12DecodeLog("safeContentsParse");

	NSS_P12_SafeContents sc;
	memset(&sc, 0, sizeof(sc));
	require_noerr(decode_item(context, contentsBlob, NSS_P12_SafeContentsTemplate,
			&sc), out);

	unsigned numBags = nssArraySize((const void **)sc.bags);
    unsigned int dex;
	for(dex=0; dex<numBags; dex++) {
		NSS_P12_SafeBag *bag = sc.bags[dex];
		assert(bag != NULL);
		
		/* ensure that *something* is there */
		require(bag->bagValue.keyBag != NULL, out);
		
		/*
		 * Break out to individual bag type
		 */
		switch(bag->type) {
			case BT_ShroudedKeyBag:
				require_noerr(shroudedKeyBagParse(context, bag), out);
				break;
			case BT_CertBag:
				require_noerr(certBagParse(context, bag), out);
				break;

			case BT_KeyBag:
				/* keyBagParse(bag); */
                p12DecodeLog("Unhandled BT_KeyBag");
				break;
			case BT_CrlBag:
				/* crlBagParse(bag); */
                p12DecodeLog("Unhandled BT_CrlBag");
				break;
			case BT_SecretBag:
				/* secretBagParse(bag); */
                p12DecodeLog("Unhandled BT_SecretBag");
				break;
			case BT_SafeContentsBag:
				/* safeContentsBagParse(bag); */
                p12DecodeLog("Unhandled BT_SafeContentsBag");
				break;
			default:
                p12DecodeLog("Unknown bag type");
                goto out;
                break;
		}
	}
    return 0;
out:
    return -1;
}

/*
 * Parse a ContentInfo in the context of (i.e., as an element of)
 * an AuthenticatedSafe.
 */
static int authSafeElementParse(pkcs12_context * context, const NSS_P7_DecodedContentInfo *info)
{
	p12DecodeLog("authSafeElementParse");
	switch(info->type) {
		case CT_Data:
			/* unencrypted SafeContents */
			require_noerr(safeContentsParse(context, info->content.data), out);
			break;
			
		case CT_EncryptedData:
		{
			/* 
			 * Decrypt contents to get a SafeContents and
			 * then parse that.
			 */
			SecAsn1Item ptext = {0, NULL};
            NSS_P7_EncryptedData *edata = info->content.encryptData;
            require_noerr_quiet(p12Decrypt(context, &edata->contentInfo.encrAlg, 
                &edata->contentInfo.encrContent, &ptext), out);
			require_noerr(safeContentsParse(context, &ptext), out);
			break;
		}	
		default:
            break;
	}
    return 0;
out:
    return -1;
}

/*
 * Parse an encoded NSS_P12_AuthenticatedSafe
 */
static int authSafeParse(pkcs12_context * context, const SecAsn1Item *authSafeBlob)
{
    p12DecodeLog("authSafeParse");
    NSS_P12_AuthenticatedSafe authSafe;
    memset(&authSafe, 0, sizeof(authSafe));
    require_noerr(decode_item(context, authSafeBlob, 
        NSS_P12_AuthenticatedSafeTemplate, &authSafe), out);

    unsigned numInfos = nssArraySize((const void **)authSafe.info);
    unsigned int dex;
    for (dex=0; dex<numInfos; dex++) {
        NSS_P7_DecodedContentInfo *info = authSafe.info[dex];
        require_noerr_quiet(authSafeElementParse(context, info), out);
    }
    return 0;
out:
    return -1;
}

static int p12VerifyMac(pkcs12_context * context, const NSS_P12_DecodedPFX *pfx)
{
	NSS_P12_MacData *macData = pfx->macData;
	require(macData, out);
	NSS_P7_DigestInfo *digestInfo  = &macData->mac;
    require(digestInfo, out);
	SecAsn1Item *algOid = &digestInfo->digestAlgorithm.algorithm;
    require(algOid, out);

    /* has to be OID_OIW_SHA1 */
    DERItem algOidItem = { algOid->Data, algOid->Length };
    require(algOidItem.length && DEROidCompare(&oidSha1, &algOidItem), out);
	
	uint32_t iterCount = 0;
	require_noerr_quiet(p12DataToInt(&macData->iterations, &iterCount), out);
	if (iterCount == 0) { /* optional, default 1 */
		iterCount = 1;
	}

	/*
	 * In classic fashion, the PKCS12 spec now says:
	 *
	 *      When password integrity mode is used to secure a PFX PDU, 
	 *      an SHA-1 HMAC is computed on the BER-encoding of the contents 
	 *      of the content field of the authSafe field in the PFX PDU.
	 *
	 * So here we go.
	 */
	uint8_t hmac_key[CC_SHA1_DIGEST_LENGTH];
	require_noerr_quiet(p12_pbe_gen(context->passphrase, 
        macData->macSalt.Data, macData->macSalt.Length,
        iterCount, PBE_ID_MAC, hmac_key, sizeof(hmac_key)), out);

	/* prealloc the mac data */
	SecAsn1Item verifyMac;
	alloc_item(context, &verifyMac, CC_SHA1_DIGEST_LENGTH);
	SecAsn1Item *ptext = pfx->authSafe.content.data;
	CCHmac(kCCHmacAlgSHA1, hmac_key, CC_SHA1_DIGEST_LENGTH, 
        ptext->Data, ptext->Length, verifyMac.Data);
	require_quiet(nssCompareSecAsn1Items(&verifyMac, &digestInfo->digest), out);
	
	return 0;
out:
    return -1;
}

p12_error p12decode(pkcs12_context * context, CFDataRef cdpfx)
{
    int err = p12_decodeErr;
	NSS_P12_DecodedPFX pfx;
	memset(&pfx, 0, sizeof(pfx));
    SecAsn1Item raw_blob = { CFDataGetLength(cdpfx), (void*)CFDataGetBytePtr(cdpfx) };

    require_noerr_quiet(decode_item(context, &raw_blob, NSS_P12_DecodedPFXTemplate, &pfx), out);
	NSS_P7_DecodedContentInfo *dci = &pfx.authSafe;
    
    /* only support CT_Data at top level (password based integrity mode) */
	require(dci->type == CT_Data, out);
	require(pfx.macData, out);
    
	require_noerr_action_quiet(p12VerifyMac(context, &pfx), out, err = p12_passwordErr);
    require_noerr_quiet(authSafeParse(context, dci->content.data), out);
    
	return errSecSuccess;
out:
    return err;
}
