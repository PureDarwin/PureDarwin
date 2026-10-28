// SPDX-License-Identifier: MIT
// freebsd kpi calls igc uses that the haiku compat layer lacks, force-included into the driver
#ifndef PD_IGC_COMPAT_H
#define PD_IGC_COMPAT_H

#ifndef PRIx64
#define PRIx64 "llx"
// pcireg.h's l1 pm substates capability, for the i225 and i226 l1.2 erratum
#ifndef PCIZ_L1PM
#define PCIZ_L1PM                   0x001e
#define PCIR_L1PM_CTL1              0x08
#define PCIM_L1PM_CTL1_PCIPM_L1_2   0x00000001
#define PCIM_L1PM_CTL1_ASPM_L1_2    0x00000004
#endif

#endif

// net/rss_config.h's hash types, the receive hash igc programs into MRQC
#define RSS_HASHTYPE_RSS_IPV4           (1 << 1)
#define RSS_HASHTYPE_RSS_TCP_IPV4       (1 << 2)
#define RSS_HASHTYPE_RSS_IPV6           (1 << 3)
#define RSS_HASHTYPE_RSS_TCP_IPV6       (1 << 4)
#define RSS_HASHTYPE_RSS_IPV6_EX        (1 << 5)
#define RSS_HASHTYPE_RSS_TCP_IPV6_EX    (1 << 6)
#define RSS_HASHTYPE_RSS_UDP_IPV4       (1 << 7)
#define RSS_HASHTYPE_RSS_UDP_IPV6       (1 << 9)
#define RSS_HASHTYPE_RSS_UDP_IPV6_EX    (1 << 10)

// no kernel rss, so a random key and the tcp and ip hashes, as em does without RSS
#define rss_getkey(key) arc4rand((key), 40, 0)
#define rss_gethashconfig() (RSS_HASHTYPE_RSS_IPV4 | RSS_HASHTYPE_RSS_TCP_IPV4 | \
    RSS_HASHTYPE_RSS_IPV6 | RSS_HASHTYPE_RSS_TCP_IPV6)

// pcireg.h's l1 pm substates capability, for the i225 and i226 l1.2 erratum
#ifndef PCIZ_L1PM
#define PCIZ_L1PM                   0x001e
#define PCIR_L1PM_CTL1              0x08
#define PCIM_L1PM_CTL1_PCIPM_L1_2   0x00000001
#define PCIM_L1PM_CTL1_ASPM_L1_2    0x00000004
#endif

#endif
