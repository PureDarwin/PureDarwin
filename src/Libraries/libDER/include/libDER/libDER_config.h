/*
 * libDER_config.h - base types and build knobs for libDER.
 *
 * Apple ships libDER sources as part of CommonCrypto without licensing
 * terms that permit reuse here, so none of it may be used here;
 * this is written from the public API contract
 * (the type definitions are fixed by ABI compatibility with binaries built against the macOS SDK)
 * and from ITU-T X.690.
 *
 * Thanks Apple, good one. :/
 */
#ifndef _LIB_DER_CONFIG_H_
#define _LIB_DER_CONFIG_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

typedef uint8_t  DERByte;
typedef uint16_t DERShort;
typedef size_t   DERSize;

#define DERMemset(ptr, c, len)     memset(ptr, c, len)
#define DERMemmove(dst, src, len)  memmove(dst, src, len)
#define DERMemcmp(b1, b2, len)     memcmp(b1, b2, len)

#define DER_ENCODE_ENABLE 1
#define DER_DECODE_ENABLE 1

#ifndef DER_MULTIBYTE_TAGS
#define DER_MULTIBYTE_TAGS 1
#endif

/* Tags are held in a fixed-width integer but are still encoded and decoded in
 * their minimal DER form on the wire. */
#ifndef DER_TAG_SIZE
#define DER_TAG_SIZE 8
#endif

#if DER_MULTIBYTE_TAGS
#if DER_TAG_SIZE == 1
typedef uint8_t DERTag;
#elif DER_TAG_SIZE == 2
typedef uint16_t DERTag;
#elif DER_TAG_SIZE == 4
typedef uint32_t DERTag;
#elif DER_TAG_SIZE == 8
typedef uint64_t DERTag;
#else
#error DER_TAG_SIZE invalid
#endif
#else  /* DER_MULTIBYTE_TAGS */
typedef DERByte DERTag;
#endif /* !DER_MULTIBYTE_TAGS */

__END_DECLS

#endif /* _LIB_DER_CONFIG_H_ */
