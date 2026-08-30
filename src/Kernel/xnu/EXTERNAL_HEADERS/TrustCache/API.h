/*
 * TrustCache/API.h - PureDarwin reconstruction. Apple's TrustCache library is
 * not open source, but xnu 12377 includes it from osfmk/vm/pmap_cs.h,
 * libkern/amfi/amfi.h and bsd/kern/kern_trustcache.c.
 *
 * Derived from those call sites, not from Apple's header. The opaque struct
 * layouts are placeholders: nothing in the x86_64 build dereferences them
 * (PMAP_CS is off, TXM/PPL paths are arm64-only). Not valid for arm64 PPL/SPTM.
 */

#ifndef PD_TRUSTCACHE_API_H
#define PD_TRUSTCACHE_API_H

#include <stdint.h>
#include <stdbool.h>

/* cdhash bound; CS_CDHASH_LEN in osfmk/kern/cs_blobs.h is 20. */
#define kTCEntryHashSize 20

/* Array bound in amfi.h's vtable prototypes. */
#define kUUIDSize 16

/*
 * kern_trustcache.c:868 iterates `for (type = kTCTypeLTRS; type < kTCTypeTotal;
 * type += 1)` over "the first valid type ... through each", and retries DTRS
 * separately as an engineering root, so DTRS sorts below LTRS. Static,
 * Engineering and Legacy are rejected explicitly at :969, so they sit below it
 * too. Only the relative order matters.
 */
typedef uint32_t TCType_t;
enum {
	kTCTypeInvalid = 0,
	kTCTypeStatic,
	kTCTypeEngineering,
	kTCTypeLegacy,
	kTCTypeDTRS,
	kTCTypeLTRS,
	kTCTypeCryptex1BootOS,
	kTCTypeCryptex1BootApp,
	kTCTypeTotal,
};

/* Bounds-checked at :740 with `query_type >= kTCQueryTypeTotal`. */
typedef uint32_t TCQueryType_t;
enum {
	kTCQueryTypeStatic = 0,
	kTCQueryTypeLoadable,
	kTCQueryTypeAll,
	kTCQueryTypeTotal,
};

/* Cast from a 64-bit TXM return word at :120. */
typedef uint64_t TCCapabilities_t;
#define kTCCapabilityNone ((TCCapabilities_t)0)

/* Printed as "0x%02X | 0x%02X | %u" at :662; .error holds the kTCReturn* below. */
typedef struct _TCReturn {
	uint8_t  component;
	uint8_t  error;
	uint32_t uniqueError;
} TCReturn_t;

enum {
	kTCReturnSuccess = 0,
	kTCReturnError,
	kTCReturnNotFound,
	kTCReturnDuplicate,
};

typedef struct _TrustCache {
	void    *tc_data;
	size_t   tc_length;
} TrustCache_t;

/* Fields read at :1253 and :1260. */
typedef struct _TrustCacheRuntime {
	bool     allowSecondStaticTC;
	bool     allowEngineeringTC;
	void    *tcr_private;
} TrustCacheRuntime_t;

typedef struct _TrustCacheMutableRuntime {
	void    *tcmr_private;
} TrustCacheMutableRuntime_t;

/* Assigned at :273-274. */
typedef struct _TrustCacheQueryToken {
	const TrustCache_t *trustCache;
	const void         *trustCacheEntry;
} TrustCacheQueryToken_t;

/*
 * Indexed by TCType_t at :978. entitlementValue is compared against NULL and
 * passed to IOCurrentTaskHasStringEntitlement(), so it is a C string. NULL for
 * every type here means no type demands an entitlement, which is moot while the
 * loader below is a no-op.
 */
typedef struct _TCTypeConfig {
	const char *entitlementValue;
} TCTypeConfig_t;

static const TCTypeConfig_t TCTypeConfig[kTCTypeTotal] = { { 0 } };

/*
 * The only TrustCache entry point called directly rather than through amfi's
 * vtable (:594). Without the library there is nothing to initialise; the
 * runtime flags it would set are read at :1253/:1260, so seed them from the
 * caller's arguments and leave everything else zeroed.
 */
static inline void
trustCacheInitializeRuntime(
	TrustCacheRuntime_t *runtime,
	TrustCacheMutableRuntime_t *mutableRuntime,
	bool allowSecondStaticTC,
	bool allowEngineeringTC,
	bool allowLegacyTC __attribute__((unused)),
	const void *img4Runtime __attribute__((unused)))
{
	if (runtime != (TrustCacheRuntime_t *)0) {
		runtime->allowSecondStaticTC = allowSecondStaticTC;
		runtime->allowEngineeringTC = allowEngineeringTC;
		runtime->tcr_private = (void *)0;
	}
	if (mutableRuntime != (TrustCacheMutableRuntime_t *)0) {
		mutableRuntime->tcmr_private = (void *)0;
	}
}

#endif /* PD_TRUSTCACHE_API_H */
