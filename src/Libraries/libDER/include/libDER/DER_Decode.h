/*
 * DER_Decode.h - DER decoding.
 */
#ifndef _DER_DECODE_H_
#define _DER_DECODE_H_

#include <libDER/DERItem.h>
#include <libDER/asn1Types.h>

#include <stdbool.h>    /* bool, in the parse helpers below */
#include <stddef.h>     /* offsetof, used by DER_OFFSET */

__BEGIN_DECLS

typedef enum {
    DR_Success = 0,
    DR_EndOfSequence,     /* no more items in a sequence */
    DR_UnexpectedTag,     /* tag did not match the spec */
    DR_DecodeError,       /* malformed encoding */
    DR_Unimplemented,
    DR_IncompleteSeq,     /* sequence ended with required items outstanding */
    DR_ParamErr,
    DR_BufOverflow,
} DERReturn;

/* One decoded TLV: its tag and a view of its content octets. */
typedef struct {
    DERTag  tag;
    DERItem content;
} DERDecodedInfo;

/* Cursor over the content of a constructed item. */
typedef struct {
    DERByte *nextItem;   /* next unread octet */
    DERByte *end;        /* one past the last content octet */
} DERSequence;

/*
 * How one field of a struct is decoded. `offset` is the byte offset of a
 * DERItem within the caller's struct (see DER_OFFSET), `tag` the tag expected
 * there, and `options` the DER_DEC_ and DER_ENC_ flags below.
 */
typedef struct {
    DERSize  offset;
    DERTag   tag;
    DERShort options;
} DERItemSpec;

#define DER_OFFSET(type, field) ((DERSize)offsetof(type, field))

/* Decode options. */
#define DER_DEC_NO_OPTS   0x0000
#define DER_DEC_OPTIONAL  0x0001  /* field may be absent; left zeroed */
#define DER_DEC_SAVE_DER  0x0002  /* keep the whole TLV, not just content */
#define DER_DEC_ASN_ANY   0x0004  /* accept any tag */

/* Encode options (declared here so one spec table can drive both). */
#define DER_ENC_SIGNED_INT   0x0100  /* INTEGER: emit a leading 0x00 if needed */
#define DER_ENC_WRITE_DER    0x0200  /* value already holds a complete TLV */

/* Decode one TLV at the front of `der`. */
DERReturn DERDecodeItem(const DERItem *der, DERDecodedInfo *decoded);

/* Start iterating the content of a constructed TLV. `DERDecodeSeqInit` reads
 * the header from `der` and reports its tag; `DERDecodeSeqContentInit` is given
 * the content directly. */
DERReturn DERDecodeSeqInit(const DERItem *der, DERTag *tag, DERSequence *seq);
DERReturn DERDecodeSeqContentInit(const DERItem *content, DERSequence *seq);

/* Next element of a sequence, or DR_EndOfSequence when exhausted. */
DERReturn DERDecodeSeqNext(DERSequence *seq, DERDecodedInfo *decoded);

/* Decode a whole SEQUENCE/SET into a struct described by `specs`.
 * DERParseSequence expects the TLV header; DERParseSequenceContent is handed
 * the content octets. */
DERReturn DERParseSequence(const DERItem *der, DERShort numSpecs,
                           const DERItemSpec *specs, void *dest,
                           DERSize sizeToZero);
DERReturn DERParseSequenceContent(const DERItem *content, DERShort numSpecs,
                                  const DERItemSpec *specs, void *dest,
                                  DERSize sizeToZero);

/* Primitive helpers. */
DERReturn DERParseInteger(const DERItem *contents, uint32_t *result);
DERReturn DERParseBitString(const DERItem *contents, DERItem *bitStringBytes,
                            DERByte *numUnusedBits);
DERReturn DERParseBooleanWithDefault(const DERItem *contents,
                                     bool defaultValue, bool *result);

/* Compare an encoded OID's content octets against a known OID. */
bool DEROidCompare(const DERItem *oid1, const DERItem *oid2);

__END_DECLS

#endif /* _DER_DECODE_H_ */
