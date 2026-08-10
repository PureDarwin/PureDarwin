/*
 * Copyright (c) 2003-2004,2008,2010-2012 Apple Inc. All Rights Reserved.
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
 * SecAsn1Templates.c - Common ASN1 templates for use with libsecurity_asn1.
 */

#include "secasn1t.h"
#include "seccomon.h"
#include <stddef.h>

/*
 * Generic templates for individual/simple items and pointers to
 * and sets of same.
 *
 * If you need to add a new one, please note the following:
 *	 - For each new basic type you should add *four* templates:
 *	one plain, one PointerTo, one SequenceOf and one SetOf.
 *	 - If the new type can be constructed (meaning, it is a
 *	*string* type according to BER/DER rules), then you should
 *	or-in SEC_ASN1_MAY_STREAM to the type in the basic template.
 *	See the definition of the OctetString template for an example.
 *	 - It may not be obvious, but these are in *alphabetical*
 *	order based on the SEC_ASN1_XXX name; so put new ones in
 *	the appropriate place.
 */

const SecAsn1Template kSecAsn1AnyTemplate[] = {
    { SEC_ASN1_ANY | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToAnyTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1AnyTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfAnyTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1AnyTemplate }
};

const SecAsn1Template kSecAsn1SetOfAnyTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1AnyTemplate }
};

const SecAsn1Template kSecAsn1BitStringTemplate[] = {
    { SEC_ASN1_BIT_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToBitStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1BitStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfBitStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1BitStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfBitStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1BitStringTemplate }
};

const SecAsn1Template kSecAsn1BMPStringTemplate[] = {
    { SEC_ASN1_BMP_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToBMPStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1BMPStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfBMPStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1BMPStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfBMPStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1BMPStringTemplate }
};

const SecAsn1Template kSecAsn1BooleanTemplate[] = {
    { SEC_ASN1_BOOLEAN, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToBooleanTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1BooleanTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfBooleanTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1BooleanTemplate }
};

const SecAsn1Template kSecAsn1SetOfBooleanTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1BooleanTemplate }
};

const SecAsn1Template kSecAsn1EnumeratedTemplate[] = {
    { SEC_ASN1_ENUMERATED, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToEnumeratedTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1EnumeratedTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfEnumeratedTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1EnumeratedTemplate }
};

const SecAsn1Template kSecAsn1SetOfEnumeratedTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1EnumeratedTemplate }
};

const SecAsn1Template kSecAsn1GeneralizedTimeTemplate[] = {
    { SEC_ASN1_GENERALIZED_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item)}
};

const SecAsn1Template kSecAsn1PointerToGeneralizedTimeTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1GeneralizedTimeTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfGeneralizedTimeTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1GeneralizedTimeTemplate }
};

const SecAsn1Template kSecAsn1SetOfGeneralizedTimeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1GeneralizedTimeTemplate }
};

const SecAsn1Template kSecAsn1IA5StringTemplate[] = {
    { SEC_ASN1_IA5_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToIA5StringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1IA5StringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfIA5StringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1IA5StringTemplate }
};

const SecAsn1Template kSecAsn1SetOfIA5StringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1IA5StringTemplate }
};

const SecAsn1Template kSecAsn1IntegerTemplate[] = {
    { SEC_ASN1_INTEGER | SEC_ASN1_SIGNED_INT, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1UnsignedIntegerTemplate[] = {
    { SEC_ASN1_INTEGER, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToIntegerTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1IntegerTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfIntegerTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1IntegerTemplate }
};

const SecAsn1Template kSecAsn1SetOfIntegerTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1IntegerTemplate }
};

const SecAsn1Template kSecAsn1NullTemplate[] = {
    { SEC_ASN1_NULL, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToNullTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1NullTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfNullTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1NullTemplate }
};

const SecAsn1Template kSecAsn1SetOfNullTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1NullTemplate }
};

const SecAsn1Template kSecAsn1ObjectIDTemplate[] = {
    { SEC_ASN1_OBJECT_ID, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToObjectIDTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1ObjectIDTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfObjectIDTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1ObjectIDTemplate }
};

const SecAsn1Template kSecAsn1SetOfObjectIDTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1ObjectIDTemplate }
};

const SecAsn1Template kSecAsn1OctetStringTemplate[] = {
    { SEC_ASN1_OCTET_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToOctetStringTemplate[] = {
    { SEC_ASN1_POINTER | SEC_ASN1_MAY_STREAM, 0, kSecAsn1OctetStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfOctetStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1OctetStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfOctetStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1OctetStringTemplate }
};

const SecAsn1Template kSecAsn1PrintableStringTemplate[] = {
    { SEC_ASN1_PRINTABLE_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item)}
};

const SecAsn1Template kSecAsn1PointerToPrintableStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1PrintableStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfPrintableStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1PrintableStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfPrintableStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1PrintableStringTemplate }
};

#ifdef	__APPLE__
const SecAsn1Template kSecAsn1TeletexStringTemplate[] = {
    { SEC_ASN1_TELETEX_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item)}
};

const SecAsn1Template kSecAsn1PointerToTeletexStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1TeletexStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfTeletexStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1TeletexStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfTeletexStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1TeletexStringTemplate }
};
#endif	/* __APPLE__ */

const SecAsn1Template kSecAsn1T61StringTemplate[] = {
    { SEC_ASN1_T61_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToT61StringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1T61StringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfT61StringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1T61StringTemplate }
};

const SecAsn1Template kSecAsn1SetOfT61StringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1T61StringTemplate }
};

const SecAsn1Template kSecAsn1UniversalStringTemplate[] = {
    { SEC_ASN1_UNIVERSAL_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item)}
};

const SecAsn1Template kSecAsn1PointerToUniversalStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1UniversalStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfUniversalStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1UniversalStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfUniversalStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1UniversalStringTemplate }
};

const SecAsn1Template kSecAsn1UTCTimeTemplate[] = {
    { SEC_ASN1_UTC_TIME | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToUTCTimeTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1UTCTimeTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfUTCTimeTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1UTCTimeTemplate }
};

const SecAsn1Template kSecAsn1SetOfUTCTimeTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1UTCTimeTemplate }
};

const SecAsn1Template kSecAsn1UTF8StringTemplate[] = {
    { SEC_ASN1_UTF8_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item)}
};

const SecAsn1Template kSecAsn1PointerToUTF8StringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1UTF8StringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfUTF8StringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1UTF8StringTemplate }
};

const SecAsn1Template kSecAsn1SetOfUTF8StringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1UTF8StringTemplate }
};

const SecAsn1Template kSecAsn1VisibleStringTemplate[] = {
    { SEC_ASN1_VISIBLE_STRING | SEC_ASN1_MAY_STREAM, 0, NULL, sizeof(SecAsn1Item) }
};

const SecAsn1Template kSecAsn1PointerToVisibleStringTemplate[] = {
    { SEC_ASN1_POINTER, 0, kSecAsn1VisibleStringTemplate }
};

const SecAsn1Template kSecAsn1SequenceOfVisibleStringTemplate[] = {
    { SEC_ASN1_SEQUENCE_OF, 0, kSecAsn1VisibleStringTemplate }
};

const SecAsn1Template kSecAsn1SetOfVisibleStringTemplate[] = {
    { SEC_ASN1_SET_OF, 0, kSecAsn1VisibleStringTemplate }
};


/*
 * Template for skipping a subitem.
 *
 * Note that it only makes sense to use this for decoding (when you want
 * to decode something where you are only interested in one or two of
 * the fields); you cannot encode a SKIP!
 */
const SecAsn1Template kSecAsn1SkipTemplate[] = {
    { SEC_ASN1_SKIP }
};

#ifndef __APPLE__

/* These functions simply return the address of the above-declared templates.
** This is necessary for Windows DLLs.  Sigh.
*/
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_AnyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BMPStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BooleanTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_BitStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_IA5StringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_GeneralizedTimeTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_IntegerTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_NullTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_ObjectIDTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_OctetStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_PointerToAnyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_PointerToOctetStringTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_SetOfAnyTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_UTCTimeTemplate)
SEC_ASN1_CHOOSER_IMPLEMENT(SEC_UTF8StringTemplate)

#endif  /* __APPLE__ */
