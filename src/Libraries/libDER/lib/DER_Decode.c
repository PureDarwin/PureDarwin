/*
 * DER_Decode.c - DER decoding.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <libDER/DER_Decode.h>

#include <stdbool.h>
#include <stddef.h>

/*
 * Decode an identifier octet sequence.
 *
 * Low tag numbers (0-30) live in the bottom five bits of a single octet. The
 * value 31 escapes to the high-tag-number form, where the number continues in
 * base-128 with the top bit set on every octet but the last. DER additionally
 * requires the minimal encoding, so a high-tag form whose value would have fit
 * in the short form, or one with a leading 0x80 continuation octet, is invalid.
 *
 * The returned tag keeps the class and constructed bits in their original
 * positions in the top byte, which is what the callers' ASN1_* constants and
 * their `tag == ASN1_CONSTR_SEQUENCE` comparisons expect.
 */
static DERReturn der_decode_tag(const DERByte *buf, DERSize len,
                                DERTag *tagOut, DERSize *consumed)
{
    DERByte first;
    DERTag tag;
    DERSize i;

    if (len < 1) {
        return DR_DecodeError;
    }
    first = buf[0];

    if ((first & ASN1_TAG_MASK) != ASN1_TAG_MASK) {
        *tagOut = (DERTag)first;
        *consumed = 1;
        return DR_Success;
    }

#if !DER_MULTIBYTE_TAGS
    return DR_Unimplemented;
#else
    /* High-tag-number form. Accumulate the base-128 number, refusing anything
     * that cannot be represented in DERTag once the class/constructed bits are
     * reserved. */
    tag = 0;
    i = 1;
    for (;;) {
        DERByte b;

        if (i >= len) {
            return DR_DecodeError;
        }
        b = buf[i];
        if (i == 1 && b == 0x80) {
            return DR_DecodeError;      /* non-minimal: leading padding */
        }
        /* Reserve the top byte for class/constructed bits. */
        if (tag > (((DERTag)~0 >> 8) >> 7)) {
            return DR_DecodeError;      /* would overflow DERTag */
        }
        tag = (tag << 7) | (DERTag)(b & 0x7f);
        i++;
        if ((b & 0x80) == 0) {
            break;
        }
    }
    if (tag < ASN1_TAG_MASK) {
        return DR_DecodeError;          /* non-minimal: should be short form */
    }
    /* Put class/constructed back where the callers look for them. */
    *tagOut = ((DERTag)(first & ~ASN1_TAG_MASK) << ((DER_TAG_SIZE - 1) * 8)) | tag;
    *consumed = i;
    return DR_Success;
#endif
}

/*
 * Decode a length octet sequence. DER permits only the definite forms, and
 * requires the shortest one: lengths under 128 must use the single-octet short
 * form, and the long form must not carry leading zero octets.
 */
static DERReturn der_decode_length(const DERByte *buf, DERSize len,
                                   DERSize *lengthOut, DERSize *consumed)
{
    DERByte first;
    DERSize numOctets, i, length;

    if (len < 1) {
        return DR_DecodeError;
    }
    first = buf[0];

    if ((first & 0x80) == 0) {
        *lengthOut = (DERSize)first;
        *consumed = 1;
        return DR_Success;
    }

    numOctets = (DERSize)(first & 0x7f);
    if (numOctets == 0) {
        return DR_DecodeError;          /* indefinite length: not DER */
    }
    if (numOctets > sizeof(DERSize)) {
        return DR_DecodeError;          /* cannot represent */
    }
    if (len < 1 + numOctets) {
        return DR_DecodeError;
    }
    if (buf[1] == 0) {
        return DR_DecodeError;          /* non-minimal: leading zero */
    }

    length = 0;
    for (i = 0; i < numOctets; i++) {
        length = (length << 8) | (DERSize)buf[1 + i];
    }
    if (length < 128) {
        return DR_DecodeError;          /* non-minimal: should be short form */
    }

    *lengthOut = length;
    *consumed = 1 + numOctets;
    return DR_Success;
}

/* Decode one TLV, returning its tag and a view of its content. */
static DERReturn der_decode_one(const DERByte *buf, DERSize len,
                                DERDecodedInfo *decoded, DERSize *tlvLen)
{
    DERSize tagLen = 0, lenLen = 0, contentLen = 0;
    DERReturn drtn;

    if (buf == NULL || decoded == NULL) {
        return DR_ParamErr;
    }

    drtn = der_decode_tag(buf, len, &decoded->tag, &tagLen);
    if (drtn != DR_Success) {
        return drtn;
    }
    drtn = der_decode_length(buf + tagLen, len - tagLen, &contentLen, &lenLen);
    if (drtn != DR_Success) {
        return drtn;
    }
    /* Written to avoid an overflow in tagLen + lenLen + contentLen. */
    if (contentLen > len - tagLen - lenLen) {
        return DR_DecodeError;
    }

    decoded->content.data = (DERByte *)(buf + tagLen + lenLen);
    decoded->content.length = contentLen;
    if (tlvLen != NULL) {
        *tlvLen = tagLen + lenLen + contentLen;
    }
    return DR_Success;
}

DERReturn DERDecodeItem(const DERItem *der, DERDecodedInfo *decoded)
{
    if (der == NULL || der->data == NULL) {
        return DR_ParamErr;
    }
    return der_decode_one(der->data, der->length, decoded, NULL);
}

DERReturn DERDecodeSeqContentInit(const DERItem *content, DERSequence *seq)
{
    if (content == NULL || seq == NULL) {
        return DR_ParamErr;
    }
    if (content->data == NULL && content->length != 0) {
        return DR_ParamErr;
    }
    seq->nextItem = content->data;
    seq->end = content->data + content->length;
    return DR_Success;
}

DERReturn DERDecodeSeqInit(const DERItem *der, DERTag *tag, DERSequence *seq)
{
    DERDecodedInfo decoded;
    DERReturn drtn;

    if (der == NULL || der->data == NULL || seq == NULL) {
        return DR_ParamErr;
    }
    drtn = der_decode_one(der->data, der->length, &decoded, NULL);
    if (drtn != DR_Success) {
        return drtn;
    }
    if (tag != NULL) {
        *tag = decoded.tag;
    }
    return DERDecodeSeqContentInit(&decoded.content, seq);
}

DERReturn DERDecodeSeqNext(DERSequence *seq, DERDecodedInfo *decoded)
{
    DERSize tlvLen = 0;
    DERReturn drtn;

    if (seq == NULL || decoded == NULL) {
        return DR_ParamErr;
    }
    if (seq->nextItem == NULL || seq->nextItem >= seq->end) {
        return DR_EndOfSequence;
    }

    drtn = der_decode_one(seq->nextItem, (DERSize)(seq->end - seq->nextItem),
                          decoded, &tlvLen);
    if (drtn != DR_Success) {
        return drtn;
    }
    seq->nextItem += tlvLen;
    return DR_Success;
}

/*
 * Walk `specs` in order against the elements of a sequence.
 *
 * The specs describe the fields in the order they appear in the encoding. A
 * spec matches if its tag equals the element's, or if DER_DEC_ASN_ANY is set.
 * A non-matching spec is skipped only when DER_DEC_OPTIONAL, in which case its
 * DERItem stays zeroed; otherwise the input is rejected. Trailing optional
 * specs may go unsatisfied, but a missing required one is DR_IncompleteSeq, and
 * elements left over after the last spec are an error.
 */
static DERReturn der_parse_seq_content(const DERItem *content, DERShort numSpecs,
                                       const DERItemSpec *specs, void *dest,
                                       DERSize sizeToZero)
{
    DERSequence seq;
    DERDecodedInfo decoded;
    DERReturn drtn;
    DERShort specIx;
    bool haveItem;
    DERByte *itemStart = NULL;   /* first octet of the pending element's TLV */
    DERSize itemLen = 0;         /* its full encoded length */

    if (content == NULL || dest == NULL || (numSpecs != 0 && specs == NULL)) {
        return DR_ParamErr;
    }
    if (sizeToZero != 0) {
        DERMemset(dest, 0, sizeToZero);
    }

    drtn = DERDecodeSeqContentInit(content, &seq);
    if (drtn != DR_Success) {
        return drtn;
    }

    haveItem = false;
    for (specIx = 0; specIx < numSpecs; specIx++) {
        const DERItemSpec *spec = &specs[specIx];
        DERItem *field;

        if (!haveItem) {
            itemStart = seq.nextItem;
            drtn = DERDecodeSeqNext(&seq, &decoded);
            if (drtn == DR_EndOfSequence) {
                /* Everything still outstanding must be optional. */
                for (; specIx < numSpecs; specIx++) {
                    if ((specs[specIx].options & DER_DEC_OPTIONAL) == 0) {
                        return DR_IncompleteSeq;
                    }
                }
                return DR_Success;
            }
            if (drtn != DR_Success) {
                return drtn;
            }
            /* DERDecodeSeqNext advanced nextItem past this element. */
            itemLen = (DERSize)(seq.nextItem - itemStart);
            haveItem = true;
        }

        if ((spec->options & DER_DEC_ASN_ANY) == 0 && decoded.tag != spec->tag) {
            if (spec->options & DER_DEC_OPTIONAL) {
                continue;               /* absent; keep the item for the next spec */
            }
            return DR_UnexpectedTag;
        }

        field = (DERItem *)((DERByte *)dest + spec->offset);
        if (spec->options & DER_DEC_SAVE_DER) {
            /* Hand back the whole TLV, header included, not just the content. */
            field->data = itemStart;
            field->length = itemLen;
        } else {
            *field = decoded.content;
        }
        haveItem = false;
    }

    if (haveItem || seq.nextItem < seq.end) {
        return DR_UnexpectedTag;        /* unconsumed trailing elements */
    }
    return DR_Success;
}

DERReturn DERParseSequenceContent(const DERItem *content, DERShort numSpecs,
                                  const DERItemSpec *specs, void *dest,
                                  DERSize sizeToZero)
{
    return der_parse_seq_content(content, numSpecs, specs, dest, sizeToZero);
}

DERReturn DERParseSequence(const DERItem *der, DERShort numSpecs,
                           const DERItemSpec *specs, void *dest,
                           DERSize sizeToZero)
{
    DERDecodedInfo decoded;
    DERReturn drtn;

    if (der == NULL || der->data == NULL) {
        return DR_ParamErr;
    }
    drtn = der_decode_one(der->data, der->length, &decoded, NULL);
    if (drtn != DR_Success) {
        return drtn;
    }
    return der_parse_seq_content(&decoded.content, numSpecs, specs, dest,
                                 sizeToZero);
}

/*
 * INTEGER content octets are big-endian two's complement, minimally encoded.
 * Callers want a uint32_t, so reject negatives and anything too wide.
 */
DERReturn DERParseInteger(const DERItem *contents, uint32_t *result)
{
    DERSize i, len;
    const DERByte *p;
    uint32_t value;

    if (contents == NULL || contents->data == NULL || result == NULL) {
        return DR_ParamErr;
    }
    p = contents->data;
    len = contents->length;

    if (len == 0) {
        return DR_DecodeError;
    }
    if (p[0] & 0x80) {
        return DR_DecodeError;          /* negative */
    }
    /* A single leading zero is the sign byte; two is non-minimal. */
    if (len > 1 && p[0] == 0 && (p[1] & 0x80) == 0) {
        return DR_DecodeError;
    }
    if (p[0] == 0) {
        p++;
        len--;
    }
    if (len > sizeof(uint32_t)) {
        return DR_BufOverflow;
    }

    value = 0;
    for (i = 0; i < len; i++) {
        value = (value << 8) | (uint32_t)p[i];
    }
    *result = value;
    return DR_Success;
}

/*
 * BIT STRING content is a count of unused trailing bits followed by the bits
 * themselves. The count must be 0-7, and must be 0 when there are no bits.
 */
DERReturn DERParseBitString(const DERItem *contents, DERItem *bitStringBytes,
                            DERByte *numUnusedBits)
{
    if (contents == NULL || contents->data == NULL || bitStringBytes == NULL) {
        return DR_ParamErr;
    }
    if (contents->length < 1) {
        return DR_DecodeError;
    }
    if (contents->data[0] > 7) {
        return DR_DecodeError;
    }
    if (contents->length == 1 && contents->data[0] != 0) {
        return DR_DecodeError;
    }

    bitStringBytes->data = contents->data + 1;
    bitStringBytes->length = contents->length - 1;
    if (numUnusedBits != NULL) {
        *numUnusedBits = contents->data[0];
    }
    return DR_Success;
}

/*
 * BOOLEAN content is one octet. DER requires TRUE to be 0xFF exactly. An empty
 * item means the field was absent, so the caller's default stands.
 */
DERReturn DERParseBooleanWithDefault(const DERItem *contents,
                                     bool defaultValue, bool *result)
{
    if (result == NULL) {
        return DR_ParamErr;
    }
    if (contents == NULL || contents->data == NULL || contents->length == 0) {
        *result = defaultValue;
        return DR_Success;
    }
    if (contents->length != 1) {
        return DR_DecodeError;
    }
    if (contents->data[0] == 0x00) {
        *result = false;
    } else if (contents->data[0] == 0xff) {
        *result = true;
    } else {
        return DR_DecodeError;
    }
    return DR_Success;
}

bool DEROidCompare(const DERItem *oid1, const DERItem *oid2)
{
    if (oid1 == NULL || oid2 == NULL) {
        return false;
    }
    if (oid1->length != oid2->length) {
        return false;
    }
    if (oid1->length == 0) {
        return true;
    }
    if (oid1->data == NULL || oid2->data == NULL) {
        return false;
    }
    return DERMemcmp(oid1->data, oid2->data, oid1->length) == 0;
}
