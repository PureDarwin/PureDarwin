// NetBSD's endian conversions over Darwin's byte order helpers
#ifndef PD_GPT_SYS_ENDIAN_H
#define PD_GPT_SYS_ENDIAN_H
#include <stdint.h>
#include <libkern/OSByteOrder.h>

#define htole16(x) OSSwapHostToLittleInt16(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define be64toh(x) OSSwapBigToHostInt64(x)

static inline uint16_t le16dec(const void *p) { const uint8_t *b = p; return (uint16_t)(b[0] | b[1] << 8); }
static inline uint32_t le32dec(const void *p) { const uint8_t *b = p; return b[0] | b[1] << 8 | b[2] << 16 | (uint32_t)b[3] << 24; }
static inline uint64_t le64dec(const void *p) { return le32dec(p) | (uint64_t)le32dec((const uint8_t *)p + 4) << 32; }
static inline void le16enc(void *p, uint16_t v) { uint8_t *b = p; b[0] = v; b[1] = v >> 8; }
static inline void le32enc(void *p, uint32_t v) { uint8_t *b = p; b[0] = v; b[1] = v >> 8; b[2] = v >> 16; b[3] = v >> 24; }
static inline void le64enc(void *p, uint64_t v) { le32enc(p, (uint32_t)v); le32enc((uint8_t *)p + 4, (uint32_t)(v >> 32)); }
static inline uint16_t be16dec(const void *p) { const uint8_t *b = p; return (uint16_t)(b[0] << 8 | b[1]); }
static inline uint32_t be32dec(const void *p) { const uint8_t *b = p; return (uint32_t)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]; }
static inline void be16enc(void *p, uint16_t v) { uint8_t *b = p; b[0] = v >> 8; b[1] = v; }
static inline void be32enc(void *p, uint32_t v) { uint8_t *b = p; b[0] = v >> 24; b[1] = v >> 16; b[2] = v >> 8; b[3] = v; }
#endif
