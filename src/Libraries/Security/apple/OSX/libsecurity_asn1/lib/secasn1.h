/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * Support for encoding/decoding of ASN.1 using BER/DER (Basic/Distinguished
 * Encoding Rules).  The routines are found in and used extensively by the
 * security library, but exported for other use.
 *
 * $Id: secasn1.h,v 1.8 2004/05/13 15:29:13 dmitch Exp $
 */

#ifndef _SECASN1_H_
#define _SECASN1_H_

#include <security_asn1/plarenas.h>

#include <security_asn1/seccomon.h>
#include <Security/secasn1t.h>
#include <Security/SecAsn1Templates.h>


/************************************************************************/
SEC_BEGIN_PROTOS

/*
 * XXX These function prototypes need full, explanatory comments.
 */

/*
** Decoding.
*/

extern SEC_ASN1DecoderContext *SEC_ASN1DecoderStart(PRArenaPool *pool,
						    void *dest,
						    const SecAsn1Template *t,
							/*
							 * __APPLE__ addenda:
							 *
						     * Only needed if first element will 
							 * be SEC_ASN1_DYNAMIC 
							 */
							const char *buf, size_t len);

/* XXX char or unsigned char? */
extern SECStatus SEC_ASN1DecoderUpdate(SEC_ASN1DecoderContext *cx,
				       const char *buf,
				       size_t len);

extern SECStatus SEC_ASN1DecoderFinish(SEC_ASN1DecoderContext *cx);

/* Higher level code detected an error, abort the rest of the processing */
extern void SEC_ASN1DecoderAbort(SEC_ASN1DecoderContext *cx, int error);

extern void SEC_ASN1DecoderSetFilterProc(SEC_ASN1DecoderContext *cx,
					 SEC_ASN1WriteProc fn,
					 void *arg, PRBool no_store);

extern void SEC_ASN1DecoderClearFilterProc(SEC_ASN1DecoderContext *cx);

extern void SEC_ASN1DecoderSetNotifyProc(SEC_ASN1DecoderContext *cx,
					 SEC_ASN1NotifyProc fn,
					 void *arg);

extern void SEC_ASN1DecoderClearNotifyProc(SEC_ASN1DecoderContext *cx);

extern SECStatus SEC_ASN1Decode(PRArenaPool *pool, void *dest,
				const SecAsn1Template *t,
				const char *buf, size_t len);

extern SECStatus SEC_ASN1DecodeItem(PRArenaPool *pool, void *dest,
				    const SecAsn1Template *t,
				    const SecAsn1Item *item);

extern SECStatus SEC_QuickDERDecodeItem(PRArenaPool* arena, void* dest,
                     const SecAsn1Template* templateEntry,
                     SecAsn1Item* src);

/*
** Encoding.
*/

extern SEC_ASN1EncoderContext *SEC_ASN1EncoderStart(const void *src,
						    const SecAsn1Template *t,
						    SEC_ASN1WriteProc fn,
						    void *output_arg);

/* XXX char or unsigned char? */
extern SECStatus SEC_ASN1EncoderUpdate(SEC_ASN1EncoderContext *cx,
				       const char *buf,
				       unsigned long len);

extern void SEC_ASN1EncoderFinish(SEC_ASN1EncoderContext *cx);

/* Higher level code detected an error, abort the rest of the processing */
extern void SEC_ASN1EncoderAbort(SEC_ASN1EncoderContext *cx, int error);

extern void SEC_ASN1EncoderSetNotifyProc(SEC_ASN1EncoderContext *cx,
					 SEC_ASN1NotifyProc fn,
					 void *arg);

extern void SEC_ASN1EncoderClearNotifyProc(SEC_ASN1EncoderContext *cx);

extern void SEC_ASN1EncoderSetStreaming(SEC_ASN1EncoderContext *cx);

extern void SEC_ASN1EncoderClearStreaming(SEC_ASN1EncoderContext *cx);

extern void sec_ASN1EncoderSetDER(SEC_ASN1EncoderContext *cx);

extern void sec_ASN1EncoderClearDER(SEC_ASN1EncoderContext *cx);

extern void SEC_ASN1EncoderSetTakeFromBuf(SEC_ASN1EncoderContext *cx);

extern void SEC_ASN1EncoderClearTakeFromBuf(SEC_ASN1EncoderContext *cx);

extern SECStatus SEC_ASN1Encode(const void *src, 
				const SecAsn1Template *t,
				SEC_ASN1WriteProc output_proc,
				void *output_arg);

extern SecAsn1Item * SEC_ASN1EncodeItem(PRArenaPool *pool, SecAsn1Item *dest,
				    const void *src, const SecAsn1Template *t);

extern SecAsn1Item * SEC_ASN1EncodeInteger(PRArenaPool *pool,
				       SecAsn1Item *dest, long value);

extern SecAsn1Item * SEC_ASN1EncodeUnsignedInteger(PRArenaPool *pool,
					       SecAsn1Item *dest,
					       unsigned long value);

extern SECStatus SEC_ASN1DecodeInteger(SecAsn1Item *src,
				       unsigned long *value);

/*
** Utilities.
*/

/*
 * We have a length that needs to be encoded; how many bytes will the
 * encoding take?
 */
extern int SEC_ASN1LengthLength (unsigned long len);

/* encode the length and return the number of bytes we encoded. Buffer
 * must be pre allocated  */
extern int SEC_ASN1EncodeLength(unsigned char *buf,unsigned long value);

/*
 * Find the appropriate subtemplate for the given template.
 * This may involve calling a "chooser" function, or it may just
 * be right there.  In either case, it is expected to *have* a
 * subtemplate; this is asserted in debug builds (in non-debug
 * builds, NULL will be returned).
 *
 * "thing" is a pointer to the structure being encoded/decoded
 * "encoding", when true, means that we are in the process of encoding
 *	(as opposed to in the process of decoding)
 */
extern const SecAsn1Template *
SEC_ASN1GetSubtemplate (
	const SecAsn1Template *inTemplate, 
	void *thing,
	PRBool encoding,
	const char *buf, size_t len);	/* __APPLE__ addenda: for decode only */

extern SecAsn1Item *sec_asn1e_allocate_item (
	PRArenaPool *poolp, 
	SecAsn1Item *dest, 
	unsigned long len);

/*
 * These two are exported for use by SecNssEncodeItem()
 */
extern void sec_asn1e_encode_item_count (
	void *arg, 
	const char *buf, 
	size_t len,
	int depth, 
	SEC_ASN1EncodingPart data_kind);

extern void sec_asn1e_encode_item_store (
	void *arg, 
	const char *buf, 
	size_t len,
	int depth, 
	SEC_ASN1EncodingPart data_kind);


SEC_END_PROTOS
#endif /* _SECASN1_H_ */
