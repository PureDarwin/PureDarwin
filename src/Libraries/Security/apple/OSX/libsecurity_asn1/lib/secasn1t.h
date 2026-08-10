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
 * Private (SPI) types libsecurity_asn1.h.
 */

#ifndef _SECASN1T_H_
#define _SECASN1T_H_

#include <CoreFoundation/CFBase.h>		/* Boolean */
#include <sys/types.h>
#include <Security/SecAsn1Types.h>		/* public types */


/* default size used for allocation of encoding/decoding stuff */
#define SEC_ASN1_DEFAULT_ARENA_SIZE	(2048)

/*
 * Tempalte flags we don't export in the public API in SecAsn1Types.h
 */
#define SEC_ASN1_MAY_STREAM	0x40000	/* field or one of its sub-fields may
									 * stream in and so should encode as
									 * indefinite-length when streaming
									 * has been indicated; only for
									 * encoding */
#define SEC_ASN1_NO_STREAM  0X200000 /* This entry will not stream
									  * even if the sub-template says
									  * streaming is possible.  Helps
									  * to solve ambiguities with potential
									  * streaming entries that are 
									  * optional */

/* Maximum depth of nested SEQUENCEs and SETs */
#define SEC_ASN1D_MAX_DEPTH 32

#define SEC_ASN1_GET(x)        x
#define SEC_ASN1_SUB(x)        x
#define SEC_ASN1_XTRN          0
#define SEC_ASN1_MKSUB(x) 

#define SEC_ASN1_CHOOSER_DECLARE(x) \
extern const SecAsn1Template * NSS_Get_##x (void *arg, Boolean enc);

#define SEC_ASN1_CHOOSER_IMPLEMENT(x) \
const SecAsn1Template * NSS_Get_##x(void * arg, Boolean enc) \
{ return x; }

/*
** Opaque object used by the decoder to store state.
*/
typedef struct sec_DecoderContext_struct SEC_ASN1DecoderContext;

/*
** Opaque object used by the encoder to store state.
*/
typedef struct sec_EncoderContext_struct SEC_ASN1EncoderContext;

/*
 * This is used to describe to a filter function the bytes that are
 * being passed to it.  This is only useful when the filter is an "outer"
 * one, meaning it expects to get *all* of the bytes not just the
 * contents octets.
 */
typedef enum {
    SEC_ASN1_Identifier = 0,
    SEC_ASN1_Length = 1,
    SEC_ASN1_Contents = 2,
    SEC_ASN1_EndOfContents = 3
} SEC_ASN1EncodingPart;

/*
 * Type of the function pointer used either for decoding or encoding,
 * when doing anything "funny" (e.g. manipulating the data stream)
 */ 
typedef void (* SEC_ASN1NotifyProc)(void *arg, Boolean before,
				    void *dest, int real_depth);

/*
 * Type of the function pointer used for grabbing encoded bytes.
 * This can be used during either encoding or decoding, as follows...
 *
 * When decoding, this can be used to filter the encoded bytes as they
 * are parsed.  This is what you would do if you wanted to process the data
 * along the way (like to decrypt it, or to perform a hash on it in order
 * to do a signature check later).  See SEC_ASN1DecoderSetFilterProc().
 * When processing only part of the encoded bytes is desired, you "watch"
 * for the field(s) you are interested in with a "notify proc" (see
 * SEC_ASN1DecoderSetNotifyProc()) and for even finer granularity (e.g. to
 * ignore all by the contents bytes) you pay attention to the "data_kind"
 * parameter.
 *
 * When encoding, this is the specification for the output function which
 * will receive the bytes as they are encoded.  The output function can
 * perform any postprocessing necessary (like hashing (some of) the data
 * to create a digest that gets included at the end) as well as shoving
 * the data off wherever it needs to go.  (In order to "tune" any processing,
 * you can set a "notify proc" as described above in the decoding case.)
 *
 * The parameters:
 * - "arg" is an opaque pointer that you provided at the same time you
 *   specified a function of this type
 * - "data" is a buffer of length "len", containing the encoded bytes
 * - "depth" is how deep in a nested encoding we are (it is not usually
 *   valuable, but can be useful sometimes so I included it)
 * - "data_kind" tells you if these bytes are part of the ASN.1 encoded
 *   octets for identifier, length, contents, or end-of-contents
 */ 
typedef void (* SEC_ASN1WriteProc)(void *arg,
				   const char *data, size_t len,
				   int depth, SEC_ASN1EncodingPart data_kind);

#endif /* _SECASN1T_H_ */
