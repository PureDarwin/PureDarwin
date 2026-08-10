/*
 * Copyright (c) 2003-2006,2008-2013 Apple Inc. All Rights Reserved.
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
 * SecAsn1Coder.h: ANS1 encode/decode object, ANSI C version.
 */
 
#include "SecAsn1Coder.h"
#include "plarenas.h"
#include "prerror.h"
#include "seccomon.h"
#include "secasn1.h"
#include <Security/SecBase.h>

/* 
 * Default chunk size for new arena pool.
 * FIXME: analyze & measure different defaults here. I'm pretty sure
 * that only performance - not correct behavior - is affected by
 * an arena pool's chunk size.
 */
#define CHUNKSIZE_DEF		1024		

/*
 * Caller's SecAsn1CoderRef points to one of these.
 */
typedef struct SecAsn1Coder {
	PLArenaPool		*mPool;
} SecAsn1Coder_t;

/*
 * Create/destroy SecAsn1Coder object. 
 */
OSStatus SecAsn1CoderCreate(
	SecAsn1CoderRef  *coder)
{
	if(coder == NULL) {
		return errSecParam;
	}
	SecAsn1CoderRef _coder = (SecAsn1CoderRef)malloc(sizeof(SecAsn1Coder_t));
	_coder->mPool = PORT_NewArena(CHUNKSIZE_DEF);
	if(_coder->mPool == NULL) {
        free(_coder);
		return errSecAllocate;
	}
	*coder = _coder;
	return errSecSuccess;
}
	
OSStatus SecAsn1CoderRelease(
	SecAsn1CoderRef  coder)
{
	if(coder == NULL) {
		return errSecParam;
	}
	if(coder->mPool != NULL) {
		/*
		 * Note: we're asking for a memory zero here, but 
		 * PORT_FreeArena doesn't do that (yet).
		 */
		PORT_FreeArena(coder->mPool, PR_TRUE);
		coder->mPool = NULL;
	}
	free(coder);
	return errSecSuccess;
}
	
/*
 * DER decode an untyped item per the specified template array. 
 * The result is allocated in this SecAsn1Coder's memory pool and 
 * is freed when this object is released.
 *
 * The dest pointer is a template-specific struct allocated by the caller 
 * and must be zeroed by the caller. 
 */
OSStatus SecAsn1Decode(
	SecAsn1CoderRef			coder,
	const void				*src,		// DER-encoded source
	size_t					len,
	const SecAsn1Template 	*templ,	
	void					*dest)
{
	if((coder == NULL) || (src == NULL) || (templ == NULL) || (dest == NULL)) {
		return errSecParam;
	}
	SECStatus prtn = SEC_ASN1Decode(coder->mPool, dest, templ, (const char *)src, len);
	if(prtn) {
		return errSecDecode;
	}
	else {
		return errSecSuccess;
	}
}
		
/* 
 * Convenience routine, decode from a SecAsn1Item.
 */
OSStatus SecAsn1DecodeData(
	SecAsn1CoderRef			coder,
	const SecAsn1Item			*src,
	const SecAsn1Template 	*templ,	
	void					*dest)
{
	return SecAsn1Decode(coder, src->Data, src->Length, templ, dest);
}

/*
 * DER encode. The encoded data (in dest.Data) is allocated in this 
 * SecAsn1Coder's memory pool and is freed when this object is released.
 *
 * The src pointer is a template-specific struct.
 */
OSStatus SecAsn1EncodeItem(
	SecAsn1CoderRef			coder,
	const void				*src,
	const SecAsn1Template 	*templ,	
	SecAsn1Item				*dest)
{
	if((coder == NULL) || (src == NULL) || (templ == NULL) || (dest == NULL)) {
		return errSecParam;
	}
	dest->Data = NULL;
	dest->Length = 0;
	
	SecAsn1Item *rtnItem = SEC_ASN1EncodeItem(coder->mPool, dest, src, templ);
	if(rtnItem == NULL) {
		/* FIXME what to return here? */
		return errSecParam;
	}
	else {
		return errSecSuccess;
	}
}
		
/*
 * Some alloc-related methods which come in handy when using
 * this object. All memory is allocated using this object's 
 * memory pool. Caller never has to free it. Used for
 * temp allocs of memory which only needs a scope which is the
 * same as this object. 
 *
 * These return a errSecAllocate in the highly unlikely event of 
 * a malloc failure.
 */
void *SecAsn1Malloc(
	SecAsn1CoderRef			coder,
	size_t					len)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // After introducing nullability annotations, coder is supposed to be nonnull, suppress the warning
	if(coder == NULL) {
		return NULL;
	}
#pragma clang diagnostic pop
	return PORT_ArenaAlloc(coder->mPool, len);
}
	
/* malloc item.Data, set item.Length */
OSStatus SecAsn1AllocItem(
	SecAsn1CoderRef			coder,
	SecAsn1Item				*item,
	size_t					len)
{
	if((coder == NULL) || (item == NULL)) {
		return errSecParam;
	}
	item->Data = (uint8_t *)PORT_ArenaAlloc(coder->mPool, len);
	if(item->Data == NULL) {
		return errSecAllocate;
	}
	item->Length = len;
	return errSecSuccess;
}
	
/* malloc and copy, various forms */
OSStatus SecAsn1AllocCopy(
	SecAsn1CoderRef			coder,
	const void				*src,
	size_t					len,
	SecAsn1Item				*dest)
{
	if(src == NULL) {
		return errSecParam;
	}
	OSStatus ortn = SecAsn1AllocItem(coder, dest, len);
	if(ortn) {
		return ortn;
	}
	memmove(dest->Data, src, len);
	return errSecSuccess;
}
	
OSStatus SecAsn1AllocCopyItem(
	SecAsn1CoderRef			coder,
	const SecAsn1Item			*src,
	SecAsn1Item				*dest)
{
	return SecAsn1AllocCopy(coder, src->Data, src->Length, dest);
}
		
bool SecAsn1OidCompare(const SecAsn1Oid *oid1, const SecAsn1Oid *oid2) {
	if (!oid1 || !oid2)
		return oid1 == oid2;
	if (oid1->Length != oid2->Length)
		return false;
	return !memcmp(oid1->Data, oid2->Data, oid1->Length);
}
