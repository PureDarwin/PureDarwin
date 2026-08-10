/*
 * asn1Types.h - ASN.1 identifier octet constants.
 */
#ifndef _ASN1_TYPES_H_
#define _ASN1_TYPES_H_

#include <libDER/libDER_config.h>

__BEGIN_DECLS

/* Identifier octet: bits 8-7 class, bit 6 constructed, bits 5-1 number. */
#define ASN1_CLASS_MASK        0xc0
#define ASN1_UNIVERSAL         0x00
#define ASN1_APPLICATION       0x40
#define ASN1_CONTEXT_SPECIFIC  0x80
#define ASN1_PRIVATE           0xc0

#define ASN1_CONSTRUCTED       0x20
#define ASN1_PRIMITIVE         0x00

#define ASN1_TAG_MASK          0x1f

/* Universal tag numbers (X.680 clause 8). */
#define ASN1_BOOLEAN           0x01
#define ASN1_INTEGER           0x02
#define ASN1_BIT_STRING        0x03
#define ASN1_OCTET_STRING      0x04
#define ASN1_NULL              0x05
#define ASN1_OBJECT_ID         0x06
#define ASN1_OBJECT_DESCRIPTOR 0x07
#define ASN1_EXTERNAL          0x08
#define ASN1_REAL              0x09
#define ASN1_ENUMERATED        0x0a
#define ASN1_UTF8_STRING       0x0c
#define ASN1_SEQUENCE          0x10
#define ASN1_SET               0x11
#define ASN1_NUMERIC_STRING    0x12
#define ASN1_PRINTABLE_STRING  0x13
#define ASN1_T61_STRING        0x14
#define ASN1_TELETEX_STRING    ASN1_T61_STRING
#define ASN1_VIDEOTEX_STRING   0x15
#define ASN1_IA5_STRING        0x16
#define ASN1_UTC_TIME          0x17
#define ASN1_GENERALIZED_TIME  0x18
#define ASN1_GRAPHIC_STRING    0x19
#define ASN1_VISIBLE_STRING    0x1a
#define ASN1_ISO646_STRING     ASN1_VISIBLE_STRING
#define ASN1_GENERAL_STRING    0x1b
#define ASN1_UNIVERSAL_STRING  0x1c
#define ASN1_BMP_STRING        0x1e

/* SEQUENCE and SET always appear constructed. */
#define ASN1_CONSTR_SEQUENCE   (ASN1_CONSTRUCTED | ASN1_SEQUENCE)
#define ASN1_CONSTR_SET        (ASN1_CONSTRUCTED | ASN1_SET)

/*
 * Under DER_MULTIBYTE_TAGS a DERTag is wider than a byte and carries its
 * class/constructed bits in the top byte, so it cannot be written straight into
 * an identifier octet. These are the same tags in the single-octet encoding,
 * for callers assembling a header by hand.
 */
#define ONE_BYTE_ASN1_CONSTR_SEQUENCE  0x30
#define ONE_BYTE_ASN1_CONSTR_SET       0x31

__END_DECLS

#endif /* _ASN1_TYPES_H_ */
