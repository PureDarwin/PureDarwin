/*
 * Copyright (c) 2005-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */

/*
 *  File:       i386/tsc.c
 *  Purpose:    Initializes the TSC and the various conversion
 *              factors needed by other parts of the system.
 */


#include <mach/mach_types.h>

#include <kern/cpu_data.h>
#include <kern/cpu_number.h>
#include <kern/clock.h>
#include <kern/host_notify.h>
#include <kern/macro_help.h>
#include <kern/misc_protos.h>
#include <kern/spl.h>
#include <kern/assert.h>
#include <kern/timer_call.h>
#include <mach/vm_prot.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>         /* for kernel_map */
#include <architecture/i386/pio.h>
#include <i386/machine_cpu.h>
#include <i386/cpuid.h>
#include <i386/mp.h>
#include <i386/machine_routines.h>
#include <i386/proc_reg.h>
#include <i386/tsc.h>
#include <i386/misc_protos.h>
#include <pexpert/pexpert.h>
#include <machine/limits.h>
#include <machine/commpage.h>
#include <sys/kdebug.h>
#include <pexpert/device_tree.h>

uint64_t        busFCvtt2n = 0;
uint64_t        busFCvtn2t = 0;
uint64_t        tscFreq = 0;
uint64_t        tscFCvtt2n = 0;
uint64_t        tscFCvtn2t = 0;
uint64_t        tscGranularity = 0;
uint64_t        bus2tsc = 0;
uint64_t        busFreq = 0;
uint32_t        flex_ratio = 0;
uint32_t        flex_ratio_min = 0;
uint32_t        flex_ratio_max = 0;

uint64_t        tsc_at_boot = 0;

#define bit(n)          (1ULL << (n))
#define bitmask(h, l)    ((bit(h)|(bit(h)-1)) & ~(bit(l)-1))
#define bitfield(x, h, l) (((x) & bitmask(h,l)) >> l)

/* Decimal powers: */
#define kilo (1000ULL)
#define Mega (kilo * kilo)
#define Giga (kilo * Mega)
#define Tera (kilo * Giga)
#define Peta (kilo * Tera)

#define CPU_FAMILY_PENTIUM_M    (0x6)

static void
tsc_stamp(void *tscptr)
{
    wrmsr64(MSR_P5_TSC, *(uint64_t *)tscptr);
}

/* AMDs TSC is different per core, causing weird things */
/* AMD at somepoint made a piece of software called 'Dual-Core Optimiser' */
/* Anyways, this is mostly just fixing hardware in software. */
/* Sync it in kernel so we don't need any external kernel extenions */
uint64_t tsc_sync_interval_msecs = 5000; /* 5 seconds */
uint64_t tsc_sync_interval_abs;
uint64_t tsc_sync_next_deadline;
static timer_call_data_t sync_tsc_timer;

static void
tsc_sync(thread_call_param_t param0 __unused, thread_call_param_t param1 __unused)
{
    uint64_t tsc;
    
    tsc = rdmsr64(MSR_P5_TSC);
    /* Run on all cores */
    mp_rendezvous_no_intrs(tsc_stamp, (void*)&tsc);
    
    clock_deadline_for_periodic_event(tsc_sync_interval_abs, mach_absolute_time(), &tsc_sync_next_deadline);
    timer_call_enter_with_leeway(&sync_tsc_timer, NULL, tsc_sync_next_deadline, 0, TIMER_CALL_SYS_NORMAL, FALSE);
}

static bool
amd_is_divisor_reserved_zen(uint64_t field)
{
    switch (field) {
        case 0x1B:
        case 0x1D:
        case 0x1F:
        case 0x21:
        case 0x23:
        case 0x25:
        case 0x27:
        case 0x29:
        case 0x2B:
        case 0x2D ... 0x3F:
            return true;
        default:
            return false;
    }
}

/* Referenced from AMDs 17h Family Open-Source Register Reference and AMD's 16h BIOS and Kernel Developer guide */
static float /* ??? Why use divisors that are decimal integers AMD? */
amd_get_divisor_for_tsc_freq(uint64_t field)
{
    i386_cpu_info_t *infop = cpuid_info();
    
    switch (field) {
        case 0x00:
            if (infop->cpuid_family >= CPUID_FAMILY_AMD_17h) {
                return 0; /* switched to OFF on Zen */
            } else {
                return 1;
            }
        case 0x01:
            if (infop->cpuid_family >= CPUID_FAMILY_AMD_17h) {
                panic("Invalid TSC divisor field! 0x%llx", field);
            } else {
                return 2;
            }
        case 0x02:
            if (infop->cpuid_family >= CPUID_FAMILY_AMD_17h) {
                panic("Invalid TSC divisor field! 0x%llx", field);
            } else {
                return 4;
            }
        case 0x03:
            if (infop->cpuid_family >= CPUID_FAMILY_AMD_17h) {
                panic("Invalid TSC divisor field! 0x%llx", field);
            } else {
                return 8;
            }
        case 0x04:
            if (infop->cpuid_family >= CPUID_FAMILY_AMD_17h) {
                panic("Invalid TSC divisor field! 0x%llx", field);
            } else {
                return 16;
            }
        default:
            if (infop->cpuid_family <= CPUID_FAMILY_AMD_16h) {
                panic("Invalid TSC divisor field! 0x%llx", field);
            } else {
                if (amd_is_divisor_reserved_zen(field)) {
                    panic("P0 divisor is reserved!");
                }
                return field / 8;
            }
    }
}

/*
 * This routine extracts a frequency property in Hz from the device tree.
 * Also reads any initial TSC value at boot from the device tree.
 */
static uint64_t
EFI_get_frequency(const char *prop)
{
    uint64_t        frequency = 0;
    DTEntry         entry;
    void const      *value;
    unsigned int    size;

    if (SecureDTLookupEntry(0, "/efi/platform", &entry) != kSuccess) {
        kprintf("EFI_get_frequency: didn't find /efi/platform\n");
        return 0;
    }

    /*
     * While we're here, see if EFI published an initial TSC value.
     */
    if (SecureDTGetProperty(entry, "InitialTSC", &value, &size) == kSuccess) {
        if (size == sizeof(uint64_t)) {
            tsc_at_boot = *(uint64_t const *) value;
            kprintf("EFI_get_frequency: read InitialTSC: %llu\n",
                    tsc_at_boot);
        }
    }

    if (SecureDTGetProperty(entry, prop, &value, &size) != kSuccess) {
        kprintf("EFI_get_frequency: property %s not found\n", prop);
        return 0;
    }
    if (size == sizeof(uint64_t)) {
        frequency = *(uint64_t const *) value;
        kprintf("EFI_get_frequency: read %s value: %llu\n",
                prop, frequency);
    }

    return frequency;
}

/*
 * Initialize the various conversion factors needed by code referencing
 * the TSC.
 */
void
tsc_init(void)
{
    boolean_t       N_by_2_bus_ratio = FALSE;
    
    if (PE_parse_boot_argn("tsc_sync_interval", &tsc_sync_interval_msecs, sizeof(tsc_sync_interval_msecs))) {
        kprintf("Syncing TSC at %s every milliseconds instead of default (5000)");
    }
    
    if (cpuid_vmm_present()) {
        kprintf("VMM vendor %s TSC frequency %u KHz bus frequency %u KHz\n",
                cpuid_vmm_family_string(),
                cpuid_vmm_info()->cpuid_vmm_tsc_frequency,
                cpuid_vmm_info()->cpuid_vmm_bus_frequency);

        if (cpuid_vmm_info()->cpuid_vmm_tsc_frequency &&
            cpuid_vmm_info()->cpuid_vmm_bus_frequency) {
            busFreq = (uint64_t)cpuid_vmm_info()->cpuid_vmm_bus_frequency * kilo;
            busFCvtt2n = ((1 * Giga) << 32) / busFreq;
            busFCvtn2t = 0xFFFFFFFFFFFFFFFFULL / busFCvtt2n;

            tscFreq = (uint64_t)cpuid_vmm_info()->cpuid_vmm_tsc_frequency * kilo;
            tscFCvtt2n = ((1 * Giga) << 32) / tscFreq;
            tscFCvtn2t = 0xFFFFFFFFFFFFFFFFULL / tscFCvtt2n;

            tscGranularity = tscFreq / busFreq;

            bus2tsc = tmrCvt(busFCvtt2n, tscFCvtn2t);

            return;
        }
    }

    switch (cpuid_cpufamily()) {
        case CPUFAMILY_INTEL_KABYLAKE:
        case CPUFAMILY_INTEL_ICELAKE:
        case CPUFAMILY_INTEL_SKYLAKE: {
            /*
             * SkyLake and later has an Always Running Timer (ART) providing
             * the reference frequency. CPUID leaf 0x15 determines the
             * rationship between this and the TSC frequency expressed as
             *   -  multiplier (numerator, N), and
             *   -  divisor (denominator, M).
             * So that TSC = ART * N / M.
             */
            i386_cpu_info_t *infop = cpuid_info();
            cpuid_tsc_leaf_t *tsc_leafp = &infop->cpuid_tsc_leaf;
            uint64_t         N = (uint64_t) tsc_leafp->numerator;
            uint64_t         M = (uint64_t) tsc_leafp->denominator;
            uint64_t         refFreq;

            refFreq = EFI_get_frequency("ARTFrequency");
            if (refFreq == 0) {
                /*
                 * Intel Scalable Processor (Xeon-SP) CPUs use a different
                 * ART frequency.  Use that default here if EFI didn't
                 * specify the frequency.  Since Xeon-SP uses the same
                 * DisplayModel / DisplayFamily as Xeon-W, we need to
                 * use the platform ID (or, as XNU calls it, the "processor
                 * flag") to differentiate the two.
                 */
                if (cpuid_family() == 0x06 &&
                    infop->cpuid_model == CPUID_MODEL_SKYLAKE_W &&
                    is_xeon_sp(infop->cpuid_processor_flag)) {
                    refFreq = BASE_ART_CLOCK_SOURCE_SP;
                } else {
                    refFreq = BASE_ART_CLOCK_SOURCE;
                }
            }

            assert(N != 0);
            assert(M != 1);
            tscFreq = refFreq * N / M;
            busFreq = tscFreq;              /* bus is APIC frequency */

            kprintf(" ART: Frequency = %6d.%06dMHz, N/M = %lld/%llu\n",
                    (uint32_t)(refFreq / Mega),
                    (uint32_t)(refFreq % Mega),
                    N, M);

            break;
        }
        case CPUFAMILY_AMD_BULLDOZER:
        case CPUFAMILY_AMD_PILEDRIVER:
        case CPUFAMILY_AMD_STEAMROLLER:
        case CPUFAMILY_AMD_EXCAVATOR:
        case CPUFAMILY_AMD_JAGUAR:
        case CPUFAMILY_AMD_PUMA:
        case CPUFAMILY_AMD_ZEN:
        case CPUFAMILY_AMD_ZENX:
        case CPUFAMILY_AMD_ZEN2:
        case CPUFAMILY_AMD_ZEN3:
        case CPUFAMILY_AMD_ZEN4: {
            uint64_t msr;
            uint64_t did;
            uint64_t fid;
            uint64_t freq;
            i386_cpu_info_t *infop = cpuid_info();
            
            busFreq = EFI_get_frequency("FSBFrequency");
            if (busFreq == 0) {
                panic("tsc_init: EFI not supported!");
            }
            busFCvtt2n = ((1 * Giga) << 32) / busFreq;
            busFCvtn2t = ((1 * Giga) << 32) / busFCvtt2n; /* Using this instead of the default alleviates HDA crackling on AMD's APU line */
            msr = rdmsr64(MSR_AMD_PSTATE_P0); /* P0 MSR */
            if (infop->cpuid_family == CPUID_FAMILY_AMD_16h && infop->cpuid_family == CPUID_FAMILY_AMD_15h) {
                /* 16h and 15h have different bitfields for the two from 17h+ */
                did = bitfield32(msr, 8, 6); /* CpuDid */
                fid = bitfield32(msr, 5, 0); /* CpuFid */
                freq = 100 * (fid + 0x10) / amd_get_divisor_for_tsc_freq(did);
                kprintf("P0/TSC freq: %lluMHz\n", freq); /* Is it in Hex or Decimal? */
                tscFreq = (freq * kilo) * 1000ULL; /* MHz -> KHz -> Hz*/
            } else if (infop->cpuid_family >= CPUID_FAMILY_AMD_17h) { /* AMDs Zen and newer platforms */
                did = bitfield32(msr, 13, 8); /* CpuDid */
                fid = bitfield32(msr, 7, 0); /* CpuFid */
                freq = (fid * 0x25) / amd_get_divisor_for_tsc_freq(did);
                msr = rdmsr64(MSR_AMD_HARDWARE_CFG);
                wrmsr64(MSR_AMD_HARDWARE_CFG, msr | MSR_AMD_HARDWARE_CFG_TSC_LOCK_AT_P0); /* The P0 Frequency can change? */
                kprintf("P0/TSC freq: %lluMHz\n", freq); /* Is it in Hex or Decimal? */
                tscFreq = (freq * kilo) * 1000ULL; /* MHz -> KHz -> Hz*/
            } else {
                panic("Unsupported AMD CPU family\n");
            }
            tscFCvtt2n = ((1 * Giga) << 32) / tscFreq;
            tscFCvtn2t = ((1 * Giga) << 32) / tscFCvtt2n;
            
            tscGranularity = tscFreq / busFreq;
            bus2tsc = tmrCvt(busFCvtt2n, tscFCvtn2t);
            
            clock_interval_to_absolutetime_interval(tsc_sync_interval_msecs, NSEC_PER_MSEC, &tsc_sync_interval_abs);
            timer_call_setup(&sync_tsc_timer, tsc_sync, NULL);
            tsc_sync_next_deadline = mach_absolute_time() + tsc_sync_interval_abs;
            timer_call_enter_with_leeway(&sync_tsc_timer, NULL, tsc_sync_next_deadline, 0, TIMER_CALL_SYS_NORMAL, FALSE);
            return;
        }
        default: {
            uint64_t msr_flex_ratio;
            uint64_t msr_platform_info;

            /* See if FLEX_RATIO is being used */
            msr_flex_ratio = rdmsr64(MSR_FLEX_RATIO);
            msr_platform_info = rdmsr64(MSR_PLATFORM_INFO);
            flex_ratio_min = (uint32_t)bitfield(msr_platform_info, 47, 40);
            flex_ratio_max = (uint32_t)bitfield(msr_platform_info, 15, 8);
            /* No BIOS-programed flex ratio. Use hardware max as default */
            tscGranularity = flex_ratio_max;
            if (msr_flex_ratio & bit(16)) {
                /* Flex Enabled: Use this MSR if less than max */
                flex_ratio = (uint32_t)bitfield(msr_flex_ratio, 15, 8);
                if (flex_ratio < flex_ratio_max) {
                    tscGranularity = flex_ratio;
                }
            }

            busFreq = EFI_get_frequency("FSBFrequency");
            /* If EFI isn't configured correctly, use a constant
             * value. See 6036811.
             */
            if (busFreq == 0) {
                busFreq = BASE_NHM_CLOCK_SOURCE;
            }

            break;
        }
        case CPUFAMILY_INTEL_PENRYN: {
            uint64_t        prfsts;

            prfsts = rdmsr64(IA32_PERF_STS);
            tscGranularity = (uint32_t)bitfield(prfsts, 44, 40);
            N_by_2_bus_ratio = (prfsts & bit(46)) != 0;

            busFreq = EFI_get_frequency("FSBFrequency");
        }
    }

    if (busFreq != 0) {
        busFCvtt2n = ((1 * Giga) << 32) / busFreq;
        busFCvtn2t = 0xFFFFFFFFFFFFFFFFULL / busFCvtt2n;
    } else {
        panic("tsc_init: EFI not supported!\n");
    }

    kprintf(" BUS: Frequency = %6d.%06dMHz, "
            "cvtt2n = %08X.%08X, cvtn2t = %08X.%08X\n",
            (uint32_t)(busFreq / Mega),
            (uint32_t)(busFreq % Mega),
            (uint32_t)(busFCvtt2n >> 32), (uint32_t)busFCvtt2n,
            (uint32_t)(busFCvtn2t >> 32), (uint32_t)busFCvtn2t);

    if (tscFreq == busFreq) {
        bus2tsc = 1;
        tscGranularity = 1;
        tscFCvtn2t = busFCvtn2t;
        tscFCvtt2n = busFCvtt2n;
    } else {
        /*
         * Get the TSC increment.  The TSC is incremented by this
         * on every bus tick.  Calculate the TSC conversion factors
         * to and from nano-seconds.
         * The tsc granularity is also called the "bus ratio".
         * If the N/2 bit is set this indicates the bus ration is
         * 0.5 more than this - i.e.  that the true bus ratio
         * is (2*tscGranularity + 1)/2.
         */
        if (N_by_2_bus_ratio) {
            tscFCvtt2n = busFCvtt2n * 2 / (1 + 2 * tscGranularity);
        } else {
            tscFCvtt2n = busFCvtt2n / tscGranularity;
        }

        tscFreq = ((1 * Giga) << 32) / tscFCvtt2n;
        tscFCvtn2t = 0xFFFFFFFFFFFFFFFFULL / tscFCvtt2n;

        /*
         * Calculate conversion from BUS to TSC
         */
        bus2tsc = tmrCvt(busFCvtt2n, tscFCvtn2t);
    }

    kprintf(" TSC: Frequency = %6d.%06dMHz, "
            "cvtt2n = %08X.%08X, cvtn2t = %08X.%08X, gran = %lld%s\n",
            (uint32_t)(tscFreq / Mega),
            (uint32_t)(tscFreq % Mega),
            (uint32_t)(tscFCvtt2n >> 32), (uint32_t)tscFCvtt2n,
            (uint32_t)(tscFCvtn2t >> 32), (uint32_t)tscFCvtn2t,
            tscGranularity, N_by_2_bus_ratio ? " (N/2)" : "");
}

void
tsc_get_info(tscInfo_t *info)
{
    info->busFCvtt2n     = busFCvtt2n;
    info->busFCvtn2t     = busFCvtn2t;
    info->tscFreq        = tscFreq;
    info->tscFCvtt2n     = tscFCvtt2n;
    info->tscFCvtn2t     = tscFCvtn2t;
    info->tscGranularity = tscGranularity;
    info->bus2tsc        = bus2tsc;
    info->busFreq        = busFreq;
    info->flex_ratio     = flex_ratio;
    info->flex_ratio_min = flex_ratio_min;
    info->flex_ratio_max = flex_ratio_max;
}

#if DEVELOPMENT || DEBUG
void
cpu_data_tsc_sync_deltas_string(char *buf, uint32_t buflen,
                                uint32_t start_cpu, uint32_t end_cpu)
{
    int cnt;
    uint32_t offset = 0;

    if (start_cpu >= real_ncpus || end_cpu >= real_ncpus) {
        if (buflen >= 1) {
            buf[0] = 0;
        }
        return;
    }

    for (uint32_t curcpu = start_cpu; curcpu <= end_cpu; curcpu++) {
        cnt = snprintf(buf + offset, buflen - offset, "0x%llx ", cpu_datap(curcpu)->tsc_sync_delta);
        if (cnt < 0 || (offset + (unsigned) cnt >= buflen)) {
            break;
        }
        offset += cnt;
    }
    if (offset >= 1) {
        buf[offset - 1] = 0;    /* Clip the final, trailing space */
    }
}
#endif /* DEVELOPMENT || DEBUG */
