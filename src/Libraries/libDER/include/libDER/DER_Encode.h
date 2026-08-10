/*
 * DER_Encode.h - DER encoding.
 */
#ifndef _DER_ENCODE_H_
#define _DER_ENCODE_H_

#include <libDER/DER_Decode.h>

__BEGIN_DECLS

/* Worst-case header size: one identifier octet plus a long-form length of up to
 * sizeof(DERSize) octets, plus the leading length-of-length octet. */
#define DER_MAX_ENCODED_SIZE (2 + sizeof(DERSize))

/* Octets needed to encode `length` as a DER length. */
DERSize DERLengthOfLength(DERSize length);

/* Encode `length` at `where`, updating *inOutLen with the octets written. */
DERReturn DEREncodeLength(DERSize length, DERByte *where, DERSize *inOutLen);

/* Octets needed for a complete TLV carrying `itemLen` content octets. */
DERSize DERLengthOfItem(DERTag tag, DERSize itemLen);

/* Encode one TLV. On entry *inOutLen is the capacity of `der`; on return it is
 * the number of octets written. */
DERReturn DEREncodeItem(DERTag tag, DERSize length, const DERByte *src,
                        DERByte *der, DERSize *inOutLen);

/* Octets needed for the SEQUENCE described by `specs`. The length is the return
 * value rather than an out-parameter: callers size a buffer straight from it,
 * e.g. `out.length = DERLengthOfEncodedSequence(...); out.data = malloc(...)`. */
DERSize DERLengthOfEncodedSequence(DERTag topTag, const void *src,
                                   DERShort numSpecs,
                                   const DERItemSpec *specs);

/* Encode a struct as a SEQUENCE per `specs`. On entry *inOutLen is the
 * capacity of `derOut`; on return the octets written. */
DERReturn DEREncodeSequence(DERTag topTag, const void *src, DERShort numSpecs,
                            const DERItemSpec *specs, DERByte *derOut,
                            DERSize *inOutLen);

__END_DECLS

#endif /* _DER_ENCODE_H_ */
