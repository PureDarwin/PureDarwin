/*
 * Apple withheld the PPL sources from the open-source xnu drop, but pmap.c
 * includes and calls into them unconditionally. No PureDarwin board has a
 * SART (Apple's IOMMU address-range unit), so bootstrap is a no-op.
 */

#ifndef _ARM64_PPL_SART_H_
#define _ARM64_PPL_SART_H_

static inline void
sart_bootstrap(void)
{
}

#endif /* _ARM64_PPL_SART_H_ */
