/*
 * DER_Encode.c - DER encoding.
 */

#include <libDER/DER_Encode.h>

#include <stdbool.h>
#include <stddef.h>

/* Octets needed for the identifier, given a tag carrying class/constructed
 * bits in its top byte and the number in the low bits. */
static DERSize der_length_of_tag(DERTag tag)
{
#if !DER_MULTIBYTE_TAGS
    (void)tag;
    return 1;
#else
    DERTag number = tag & ~((DERTag)0xff << ((DER_TAG_SIZE - 1) * 8));
    DERSize len;

    if (number < ASN1_TAG_MASK) {
        return 1;
    }
    /* High-tag-number form: 7 value bits per continuation octet. */
    len = 1;
    while (number != 0) {
        len++;
        number >>= 7;
    }
    return len;
#endif
}

/* Encode the identifier octets. *inOutLen is capacity on entry, written on exit. */
static DERReturn der_encode_tag(DERTag tag, DERByte *where, DERSize *inOutLen)
{
#if !DER_MULTIBYTE_TAGS
    if (*inOutLen < 1) {
        return DR_BufOverflow;
    }
    where[0] = (DERByte)tag;
    *inOutLen = 1;
    return DR_Success;
#else
    DERByte classBits = (DERByte)(tag >> ((DER_TAG_SIZE - 1) * 8));
    DERTag number = tag & ~((DERTag)0xff << ((DER_TAG_SIZE - 1) * 8));
    DERSize need, i;

    if (number < ASN1_TAG_MASK) {
        if (*inOutLen < 1) {
            return DR_BufOverflow;
        }
        /* For the short form the whole identifier octet is in the low byte. */
        where[0] = (DERByte)(classBits | (DERByte)number);
        if (classBits == 0) {
            where[0] = (DERByte)tag;
        }
        *inOutLen = 1;
        return DR_Success;
    }

    need = der_length_of_tag(tag);
    if (*inOutLen < need) {
        return DR_BufOverflow;
    }
    where[0] = (DERByte)(classBits | ASN1_TAG_MASK);
    /* Base-128, most significant group first, continuation bit on all but the
     * last octet. */
    for (i = 1; i < need; i++) {
        DERSize shift = 7 * (need - 1 - i);
        DERByte b = (DERByte)((number >> shift) & 0x7f);
        if (i != need - 1) {
            b |= 0x80;
        }
        where[i] = b;
    }
    *inOutLen = need;
    return DR_Success;
#endif
}

DERSize DERLengthOfLength(DERSize length)
{
    DERSize len, tmp;

    if (length < 128) {
        return 1;
    }
    len = 1;
    tmp = length;
    while (tmp != 0) {
        len++;
        tmp >>= 8;
    }
    return len;
}

DERReturn DEREncodeLength(DERSize length, DERByte *where, DERSize *inOutLen)
{
    DERSize need, i, numOctets;

    if (where == NULL || inOutLen == NULL) {
        return DR_ParamErr;
    }
    need = DERLengthOfLength(length);
    if (*inOutLen < need) {
        return DR_BufOverflow;
    }

    if (length < 128) {
        where[0] = (DERByte)length;
        *inOutLen = 1;
        return DR_Success;
    }

    numOctets = need - 1;
    where[0] = (DERByte)(0x80 | numOctets);
    for (i = 0; i < numOctets; i++) {
        where[1 + i] = (DERByte)((length >> (8 * (numOctets - 1 - i))) & 0xff);
    }
    *inOutLen = need;
    return DR_Success;
}

DERSize DERLengthOfItem(DERTag tag, DERSize itemLen)
{
    return der_length_of_tag(tag) + DERLengthOfLength(itemLen) + itemLen;
}

DERReturn DEREncodeItem(DERTag tag, DERSize length, const DERByte *src,
                        DERByte *der, DERSize *inOutLen)
{
    DERSize capacity, used = 0, part;
    DERReturn drtn;

    if (der == NULL || inOutLen == NULL || (length != 0 && src == NULL)) {
        return DR_ParamErr;
    }
    capacity = *inOutLen;

    part = capacity;
    drtn = der_encode_tag(tag, der, &part);
    if (drtn != DR_Success) {
        return drtn;
    }
    used = part;

    part = capacity - used;
    drtn = DEREncodeLength(length, der + used, &part);
    if (drtn != DR_Success) {
        return drtn;
    }
    used += part;

    if (capacity - used < length) {
        return DR_BufOverflow;
    }
    if (length != 0) {
        DERMemmove(der + used, src, length);
    }
    *inOutLen = used + length;
    return DR_Success;
}

/*
 * A DER_ENC_SIGNED_INT field holds an unsigned magnitude, but INTEGER is signed,
 * so a leading zero octet is prepended whenever the top bit would otherwise
 * read as negative. An empty value encodes as a single zero octet.
 */
static bool spec_needs_leading_zero(const DERItemSpec *spec, const DERItem *item)
{
    if ((spec->options & DER_ENC_SIGNED_INT) == 0) {
        return false;
    }
    if (item->length == 0) {
        return false;
    }
    return (item->data[0] & 0x80) != 0;
}

static DERSize spec_content_length(const DERItemSpec *spec, const DERItem *item)
{
    if ((spec->options & DER_ENC_SIGNED_INT) != 0 && item->length == 0) {
        return 1;               /* the value zero */
    }
    return item->length + (spec_needs_leading_zero(spec, item) ? 1 : 0);
}

/* Encoded size of one field, or 0 if it is absent and optional. */
static DERSize spec_encoded_length(const DERItemSpec *spec, const DERItem *item)
{
    if (item->data == NULL && item->length == 0 &&
        (spec->options & DER_DEC_OPTIONAL) != 0) {
        return 0;
    }
    if (spec->options & DER_ENC_WRITE_DER) {
        return item->length;    /* already a complete TLV */
    }
    return DERLengthOfItem(spec->tag, spec_content_length(spec, item));
}

DERSize DERLengthOfEncodedSequence(DERTag topTag, const void *src,
                                   DERShort numSpecs,
                                   const DERItemSpec *specs)
{
    DERSize contentLen = 0;
    DERShort i;

    /* No way to report an error, so bad arguments yield a zero length; the
     * caller's subsequent allocation and encode both fail cleanly. */
    if (src == NULL || (numSpecs != 0 && specs == NULL)) {
        return 0;
    }
    for (i = 0; i < numSpecs; i++) {
        const DERItem *item = (const DERItem *)((const DERByte *)src + specs[i].offset);
        contentLen += spec_encoded_length(&specs[i], item);
    }
    return DERLengthOfItem(topTag, contentLen);
}

DERReturn DEREncodeSequence(DERTag topTag, const void *src, DERShort numSpecs,
                            const DERItemSpec *specs, DERByte *derOut,
                            DERSize *inOutLen)
{
    DERSize capacity, contentLen = 0, used = 0, part;
    DERShort i;
    DERReturn drtn;

    if (src == NULL || derOut == NULL || inOutLen == NULL ||
        (numSpecs != 0 && specs == NULL)) {
        return DR_ParamErr;
    }
    capacity = *inOutLen;

    for (i = 0; i < numSpecs; i++) {
        const DERItem *item = (const DERItem *)((const DERByte *)src + specs[i].offset);
        contentLen += spec_encoded_length(&specs[i], item);
    }

    part = capacity;
    drtn = der_encode_tag(topTag, derOut, &part);
    if (drtn != DR_Success) {
        return drtn;
    }
    used = part;

    part = capacity - used;
    drtn = DEREncodeLength(contentLen, derOut + used, &part);
    if (drtn != DR_Success) {
        return drtn;
    }
    used += part;

    for (i = 0; i < numSpecs; i++) {
        const DERItemSpec *spec = &specs[i];
        const DERItem *item = (const DERItem *)((const DERByte *)src + spec->offset);
        DERSize fieldLen = spec_encoded_length(spec, item);

        if (fieldLen == 0) {
            continue;           /* absent optional field */
        }
        if (capacity - used < fieldLen) {
            return DR_BufOverflow;
        }

        if (spec->options & DER_ENC_WRITE_DER) {
            DERMemmove(derOut + used, item->data, item->length);
            used += item->length;
            continue;
        }

        if (spec_needs_leading_zero(spec, item) ||
            ((spec->options & DER_ENC_SIGNED_INT) != 0 && item->length == 0)) {
            /* Build the sign-corrected INTEGER content in place: header, then
             * the 0x00, then the magnitude. */
            DERSize contentLength = spec_content_length(spec, item);

            part = capacity - used;
            drtn = der_encode_tag(spec->tag, derOut + used, &part);
            if (drtn != DR_Success) {
                return drtn;
            }
            used += part;

            part = capacity - used;
            drtn = DEREncodeLength(contentLength, derOut + used, &part);
            if (drtn != DR_Success) {
                return drtn;
            }
            used += part;

            if (capacity - used < contentLength) {
                return DR_BufOverflow;
            }
            derOut[used++] = 0x00;
            if (item->length != 0) {
                DERMemmove(derOut + used, item->data, item->length);
                used += item->length;
            }
            continue;
        }

        part = capacity - used;
        drtn = DEREncodeItem(spec->tag, item->length, item->data,
                             derOut + used, &part);
        if (drtn != DR_Success) {
            return drtn;
        }
        used += part;
    }

    *inOutLen = used;
    return DR_Success;
}
