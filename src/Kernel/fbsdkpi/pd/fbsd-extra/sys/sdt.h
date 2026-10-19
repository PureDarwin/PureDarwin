// SPDX-License-Identifier: MIT
// FreeBSD's sys/sdt.h, probes compile away
#pragma once
// no dtrace providers here, probes compile away
#define SDT_PROVIDER_DEFINE(prov)
#define SDT_PROVIDER_DECLARE(prov)
#define SDT_PROBE_DEFINE(...)
#define SDT_PROBE_DECLARE(...)
#define SDT_PROBE_DEFINE0(...)
#define SDT_PROBE_DEFINE1(...)
#define SDT_PROBE_DEFINE2(...)
#define SDT_PROBE_DEFINE3(...)
#define SDT_PROBE_DEFINE4(...)
#define SDT_PROBE_DEFINE5(...)
#define SDT_PROBE_DEFINE6(...)
#define SDT_PROBE(...) do { } while (0)
#define SDT_PROBE0(...) do { } while (0)
#define SDT_PROBE1(...) do { } while (0)
#define SDT_PROBE2(...) do { } while (0)
#define SDT_PROBE3(...) do { } while (0)
#define SDT_PROBE4(...) do { } while (0)
#define SDT_PROBE5(...) do { } while (0)
#define SDT_PROBE6(...) do { } while (0)
