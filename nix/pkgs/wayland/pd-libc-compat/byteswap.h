/* glibc's <byteswap.h> over Darwin's OSByteOrder primitives.
 *
 * Ports written against glibc reach for bswap_16/32/64; Darwin spells the same
 * operations OSSwapConstInt*. Supplying the header ports expect is preferable
 * to patching each port that includes it.
 */
#ifndef PD_BYTESWAP_H
#define PD_BYTESWAP_H

#include <libkern/OSByteOrder.h>

#define bswap_16(x) OSSwapConstInt16(x)
#define bswap_32(x) OSSwapConstInt32(x)
#define bswap_64(x) OSSwapConstInt64(x)

#endif /* PD_BYTESWAP_H */
