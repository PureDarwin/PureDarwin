/*
 * Internet checksum for ARM cores without VFPv3.
 */
#include <sys/param.h>
#include <sys/mbuf.h>

#if defined (__arm__) && (__ARM_VFP__ < 3)

extern uint32_t in_cksum_mbuf_ref(struct mbuf *m, int len, int off,
    uint32_t initial_sum);

uint32_t
os_cpu_in_cksum_mbuf(struct mbuf *m, int len, int off, uint32_t initial_sum)
{
	return in_cksum_mbuf_ref(m, len, off, initial_sum);
}

#endif /* __arm__ && __ARM_VFP__ < 3 */
