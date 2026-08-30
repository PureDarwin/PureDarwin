/* Copyright (c) (2012,2015-2019,2021,2024) Apple Inc. All rights reserved.
 *
 * corecrypto is licensed under Apple Inc.’s Internal Use License Agreement (which
 * is contained in the License.txt file distributed with corecrypto) and only to
 * people who accept that license. IMPORTANT:  Any license rights granted to you by
 * Apple Inc. (if any) are limited to internal use within your organization only on
 * devices and computers you own or control, for the sole purpose of verifying the
 * security characteristics and correct functioning of the Apple Software.  You may
 * not, directly or indirectly, redistribute the Apple Software or any portions thereof.
 */

#include <corecrypto/ccsha2.h>
#include "ccsha2_internal.h"
#include "cc_runtime_config.h"
#include "fipspost_trace.h"

const struct ccdigest_info *ccsha512_di(void)
{
    FIPSPOST_TRACE_EVENT;

#if CC_USE_ASM && CCSHA2_VNG_INTEL && defined(__x86_64__)
    if (CC_HAS_AVX512_AND_IN_KERNEL()) {
        if (CC_HAS_SupplementalSSE3()) {
            return &ccsha512_vng_intel_SupplementalSSE3_di;
        }

        return &ccsha512_ltc_di;
    }

    if (CC_HAS_AVX2()) {
        return &ccsha512_vng_intel_AVX2_di;
    }

    if (CC_HAS_AVX1()) {
        return &ccsha512_vng_intel_AVX1_di;
    }

    if (CC_HAS_SupplementalSSE3()) {
        return &ccsha512_vng_intel_SupplementalSSE3_di;
    }

    return &ccsha512_ltc_di;
#elif CC_USE_ASM && CCSHA2_VNG_ARM
#if !CC_IBOOT && defined(__arm64__)
    // These two conditions imply a runtime cpu capability check
    if (CC_INTERNAL_SDK && CC_HAS_SHA512()) {
        return &ccsha512_vng_arm_hw_di;
    }
#endif
    return &ccsha512_vng_arm_di;
#else
    return &ccsha512_ltc_di;
#endif
}
