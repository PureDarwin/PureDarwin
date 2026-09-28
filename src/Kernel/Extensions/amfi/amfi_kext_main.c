/*
 * PureDarwin does not enforce Apple code signing, so this is a permissive
 * stand-in for AppleMobileFileIntegrity.kext
 */

#include <mach/mach_types.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/errno.h>

// sys/systm.h drags in libkern.h, which does not build after stdio.h here
extern int copyin(const user_addr_t uaddr, void *kaddr, size_t len);
extern int copyout(const void *kaddr, user_addr_t udaddr, size_t len);
#include <kern/cs_blobs.h>
#include <security/mac_policy.h>

/*
 * xnu's header-install step runs unifdef over the kext-visible copy of
 * amfi.h and strips its whole `#if XNU_KERNEL_PRIVATE` block, including the
 * CoreEntitlementsPriv.h include that defines coreentitlements_t.
 */
typedef struct { void *reserved[27]; } coreentitlements_t;

#include <libkern/amfi/amfi.h>

static void
pd_amfi_OSEntitlements_invalidate(void *osentitlements)
{
	(void)osentitlements;
}

static void *
pd_amfi_OSEntitlements_asdict(void *osentitlements)
{
	(void)osentitlements;
	return NULL;
}

static CEError_t
pd_amfi_OSEntitlements_query(void *osentitlements, uint8_t cdhash[CS_CDHASH_LEN],
    CEQuery_t query, size_t queryLength)
{
	(void)osentitlements;
	(void)cdhash;
	(void)query;
	(void)queryLength;
	return (CEError_t)0;
}

static bool
pd_amfi_OSEntitlements_get_transmuted(void *osentitlements, const CS_GenericBlob **blob)
{
	(void)osentitlements;
	*blob = NULL;
	return false;
}

static bool
pd_amfi_OSEntitlements_get_xml(void *osentitlements, CS_GenericBlob **blob)
{
	(void)osentitlements;
	*blob = NULL;
	return false;
}

static bool
pd_amfi_get_legacy_profile_exemptions(const uint8_t **profile, size_t *profileLength)
{
	*profile = NULL;
	*profileLength = 0;
	return false;
}

static bool
pd_amfi_get_udid(const uint8_t **udid, size_t *udidLength)
{
	*udid = NULL;
	*udidLength = 0;
	return false;
}

static void *
pd_amfi_query_context_to_object(CEQueryContext_t ctx)
{
	(void)ctx;
	return NULL;
}

static kern_return_t
pd_OSEntitlements_adjustContext(void *os_entitlements, struct cs_blob *code_signing_blob,
    const CEContext_t *ce_ctx)
{
	(void)os_entitlements;
	(void)code_signing_blob;
	(void)ce_ctx;
	return KERN_SUCCESS;
}

static kern_return_t
pd_OSEntitlements_adjustContextWithMonitor(void *os_entitlements, const CEQueryContext_t ce_ctx,
    const void *monitor_sig_obj, const char *identity, const uint32_t code_signing_flags)
{
	(void)os_entitlements;
	(void)ce_ctx;
	(void)monitor_sig_obj;
	(void)identity;
	(void)code_signing_flags;
	return KERN_SUCCESS;
}

static kern_return_t
pd_OSEntitlements_adjustContextWithoutMonitor(void *os_entitlements, struct cs_blob *code_signing_blob)
{
	(void)os_entitlements;
	(void)code_signing_blob;
	return KERN_SUCCESS;
}

/*
 * Default to "not entitled", not "grant everything"
 */
static kern_return_t
pd_OSEntitlements_queryEntitlementBoolean(const void *os_entitlements, const char *entitlement_name)
{
	(void)os_entitlements;
	(void)entitlement_name;
	return KERN_FAILURE;
}

// Per-process queries read the signed entitlements blob (amfi_entitlements.cpp)
extern void *pd_amfi_copy_entitlement(proc_t proc, const char *name);
extern bool pd_amfi_entitlement_is_true(proc_t proc, const char *name);
extern bool pd_amfi_entitlement_string_equals(proc_t proc, const char *name, const char *value);

static kern_return_t
pd_OSEntitlements_queryEntitlementBooleanWithProc(const proc_t proc, const char *entitlement_name)
{
	return pd_amfi_entitlement_is_true(proc, entitlement_name) ? KERN_SUCCESS : KERN_FAILURE;
}

static kern_return_t
pd_OSEntitlements_queryEntitlementString(const void *os_entitlements, const char *entitlement_name,
    const char *entitlement_value)
{
	(void)os_entitlements;
	(void)entitlement_name;
	(void)entitlement_value;
	return KERN_FAILURE;
}

static kern_return_t
pd_OSEntitlements_queryEntitlementStringWithProc(const proc_t proc, const char *entitlement_name,
    const char *entitlement_value)
{
	return pd_amfi_entitlement_string_equals(proc, entitlement_name, entitlement_value) ?
	       KERN_SUCCESS : KERN_FAILURE;
}

static kern_return_t
pd_OSEntitlements_copyEntitlementAsOSObject(const void *os_entitlements, const char *entitlement_name,
    void **entitlement_object)
{
	(void)os_entitlements;
	(void)entitlement_name;
	(void)entitlement_object;
	return KERN_FAILURE;
}

static kern_return_t
pd_OSEntitlements_copyEntitlementAsOSObjectWithProc(const proc_t proc, const char *entitlement_name,
    void **entitlement_object)
{
	// The caller releases the returned object
	*entitlement_object = pd_amfi_copy_entitlement(proc, entitlement_name);
	return *entitlement_object != NULL ? KERN_SUCCESS : KERN_FAILURE;
}

static TCReturn_t
pd_tc_ok(void)
{
	TCReturn_t ret = { 0, kTCReturnSuccess, 0 };
	return ret;
}

static TCReturn_t
pd_tc_loadModule(TrustCacheRuntime_t *runtime, const TCType_t type, TrustCache_t *trustCache,
    const uintptr_t dataAddr, const size_t dataSize)
{
	(void)runtime; (void)type; (void)trustCache; (void)dataAddr; (void)dataSize;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_load(TrustCacheRuntime_t *runtime, TCType_t type, TrustCache_t *trustCache,
    const uintptr_t payloadAddr, const size_t payloadSize,
    const uintptr_t manifestAddr, const size_t manifestSize)
{
	(void)runtime; (void)type; (void)trustCache;
	(void)payloadAddr; (void)payloadSize; (void)manifestAddr; (void)manifestSize;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_query(const TrustCacheRuntime_t *runtime, TCQueryType_t queryType,
    const uint8_t CDHash[kTCEntryHashSize], TrustCacheQueryToken_t *queryToken)
{
	(void)runtime;
	(void)queryType;
	(void)CDHash;
	queryToken->trustCache = NULL;
	queryToken->trustCacheEntry = NULL;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_getCapabilities(const TrustCache_t *trustCache, TCCapabilities_t *capabilities)
{
	(void)trustCache;
	*capabilities = kTCCapabilityNone;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_queryGetTCType(const TrustCacheQueryToken_t *queryToken, TCType_t *typeRet)
{
	(void)queryToken;
	*typeRet = kTCTypeStatic;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_queryGetCapabilities(const TrustCacheQueryToken_t *queryToken, TCCapabilities_t *capabilities)
{
	(void)queryToken;
	*capabilities = kTCCapabilityNone;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_queryGetHashType(const TrustCacheQueryToken_t *queryToken, uint8_t *hashTypeRet)
{
	(void)queryToken;
	*hashTypeRet = 0;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_queryGetFlags(const TrustCacheQueryToken_t *queryToken, uint64_t *flagsRet)
{
	(void)queryToken;
	*flagsRet = 0;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_queryGetConstraintCategory(const TrustCacheQueryToken_t *queryToken, uint8_t *constraintCategoryRet)
{
	(void)queryToken;
	*constraintCategoryRet = 0;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_queryGetUUID(const TrustCacheQueryToken_t *queryToken, uint8_t returnUUID[kUUIDSize])
{
	(void)queryToken;
	for (int i = 0; i < kUUIDSize; i++) {
		returnUUID[i] = 0;
	}
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_constructInvalid(TrustCache_t *trustCache, const uint8_t *moduleAddr, size_t moduleSize)
{
	(void)moduleAddr; (void)moduleSize;
	trustCache->tc_data = NULL;
	trustCache->tc_length = 0;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_checkRuntimeForUUID(const TrustCacheRuntime_t *runtime, const uint8_t checkUUID[kUUIDSize],
    const TrustCache_t **trustCacheRet)
{
	(void)runtime;
	(void)checkUUID;
	*trustCacheRet = NULL;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_extractModule(TrustCache_t *trustCache, const uint8_t *dataAddr, size_t dataSize)
{
	(void)dataAddr; (void)dataSize;
	trustCache->tc_data = NULL;
	trustCache->tc_length = 0;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_getModule(const TrustCache_t *trustCache, const uint8_t **moduleAddrRet, size_t *moduleSizeRet)
{
	(void)trustCache;
	*moduleAddrRet = NULL;
	*moduleSizeRet = 0;
	return pd_tc_ok();
}

static TCReturn_t
pd_tc_getUUID(const TrustCache_t *trustCache, uint8_t returnUUID[kUUIDSize])
{
	(void)trustCache;
	for (int i = 0; i < kUUIDSize; i++) {
		returnUUID[i] = 0;
	}
	return pd_tc_ok();
}

static bool
pd_amfi_has_mte_soft_mode(const proc_t proc)
{
	(void)proc;
	return false;
}

static bool
pd_amfi_has_mte_opt_out(struct cs_blob *blob)
{
	(void)blob;
	return false;
}

static bool
pd_amfi_has_mte_inheritance_opt_out(struct cs_blob *blob)
{
	(void)blob;
	return false;
}

static bool
pd_amfi_has_mte_data_tagging_opt_out(struct cs_blob *blob)
{
	(void)blob;
	return false;
}

static bool
pd_amfi_has_mte_alias_restriction_opt_in(struct cs_blob *blob)
{
	(void)blob;
	return false;
}

static const amfi_t pd_amfi_interface = {
	.OSEntitlements_invalidate = pd_amfi_OSEntitlements_invalidate,
	.OSEntitlements_asdict = pd_amfi_OSEntitlements_asdict,
	.OSEntitlements_query = pd_amfi_OSEntitlements_query,
	.OSEntitlements_get_transmuted = pd_amfi_OSEntitlements_get_transmuted,
	.OSEntitlements_get_xml = pd_amfi_OSEntitlements_get_xml,
	.CoreEntitlements = { 0 },
	.get_legacy_profile_exemptions = pd_amfi_get_legacy_profile_exemptions,
	.get_udid = pd_amfi_get_udid,
	.query_context_to_object = pd_amfi_query_context_to_object,

	.TrustCache = {
		.version = TRUST_CACHE_INTERFACE_VERSION,
		.loadModule = pd_tc_loadModule,
		.load = pd_tc_load,
		.query = pd_tc_query,
		.getCapabilities = pd_tc_getCapabilities,
		.queryGetTCType = pd_tc_queryGetTCType,
		.queryGetCapabilities = pd_tc_queryGetCapabilities,
		.queryGetHashType = pd_tc_queryGetHashType,
		.queryGetFlags = pd_tc_queryGetFlags,
		.queryGetConstraintCategory = pd_tc_queryGetConstraintCategory,
		.queryGetUUID = pd_tc_queryGetUUID,
		.constructInvalid = pd_tc_constructInvalid,
		.checkRuntimeForUUID = pd_tc_checkRuntimeForUUID,
		.extractModule = pd_tc_extractModule,
		.getModule = pd_tc_getModule,
		.getUUID = pd_tc_getUUID,
	},

	.OSEntitlements = {
		.version = OSENTITLEMENTS_INTERFACE_VERSION,
		.adjustContext = pd_OSEntitlements_adjustContext,
		.adjustContextWithMonitor = pd_OSEntitlements_adjustContextWithMonitor,
		.adjustContextWithoutMonitor = pd_OSEntitlements_adjustContextWithoutMonitor,
		.queryEntitlementBoolean = pd_OSEntitlements_queryEntitlementBoolean,
		.queryEntitlementBooleanWithProc = pd_OSEntitlements_queryEntitlementBooleanWithProc,
		.queryEntitlementString = pd_OSEntitlements_queryEntitlementString,
		.queryEntitlementStringWithProc = pd_OSEntitlements_queryEntitlementStringWithProc,
		.copyEntitlementAsOSObject = pd_OSEntitlements_copyEntitlementAsOSObject,
		.copyEntitlementAsOSObjectWithProc = pd_OSEntitlements_copyEntitlementAsOSObjectWithProc,
	},

	.has_mte_soft_mode = pd_amfi_has_mte_soft_mode,
	.has_mte_opt_out = pd_amfi_has_mte_opt_out,
	.has_mte_inheritance_opt_out = pd_amfi_has_mte_inheritance_opt_out,
	.has_mte_data_tagging_opt_out = pd_amfi_has_mte_data_tagging_opt_out,
	.has_mte_alias_restriction_opt_in = pd_amfi_has_mte_alias_restriction_opt_in,
};

static const struct { int unused; } pd_core_entitlements_storage;

// AMFI marks every checked image CS_SIGNED. Without it posix_spawn upcalls taskgated for each exec,
// and taskgated's own exec deadlocks on that upcall
static int
pd_amfi_vnode_check_signature(struct vnode *vp, struct label *label, cpu_type_t cpu_type,
    struct cs_blob *cs_blob, unsigned int *cs_flags, unsigned int *signer_type, int flags,
    unsigned int platform, char **fatal_failure_desc, size_t *fatal_failure_desc_len)
{
	(void)vp; (void)label; (void)cpu_type; (void)cs_blob; (void)signer_type;
	(void)flags; (void)platform; (void)fatal_failure_desc; (void)fatal_failure_desc_len;
	*cs_flags |= CS_SIGNED;
	return 0;
}

// Privileges non-root processes get from signed entitlements (priv.h numbers. Header is private)
static const struct {
	int         priv;
	const char *entitlement;
} pd_amfi_priv_entitlements[] = {
	{ 1008, "com.apple.private.kernel.work-interval" },     // PRIV_WORK_INTERVAL: WindowServer
};

static int
pd_amfi_priv_grant(kauth_cred_t cred, int priv)
{
	proc_t p = proc_self();
	int error = EPERM;

	(void)cred;
	if (p == NULL) {
		return EPERM;
	}
	for (size_t i = 0; i < sizeof(pd_amfi_priv_entitlements) / sizeof(pd_amfi_priv_entitlements[0]); i++) {
		if (pd_amfi_priv_entitlements[i].priv == priv &&
		    pd_amfi_entitlement_is_true(p, pd_amfi_priv_entitlements[i].entitlement)) {
			error = 0;
			break;
		}
	}
	proc_rele(p);
	return error;
}

// __mac_syscall("AMFI", ...): 90 is dyld's amfi_check_dyld_policy_self {input flags, output ptr}
static int
pd_amfi_policy_syscall(struct proc *p, int call, user_addr_t arg)
{
	(void)p;
	if (call == 90) {
		uint64_t args[2];
		uint64_t allow = ~0ull;         // Same grant as PureDarwin's own dyld stub
		int error = copyin(arg, args, sizeof(args));

		if (error == 0 && args[1] != 0) {
			error = copyout(&allow, (user_addr_t)args[1], sizeof(allow));
		}
		return error;
	}
	return 0;
}

static struct mac_policy_ops pd_amfi_ops = {
	.mpo_vnode_check_signature = pd_amfi_vnode_check_signature,
	.mpo_priv_grant = pd_amfi_priv_grant,
	.mpo_policy_syscall = pd_amfi_policy_syscall,
};

// Apple's Quarantine policy: cfprefsd exits(-2) on ENOPOLICY and exits(-1) on ENOATTR for
// __mac_syscall("Quarantine", 87), so nothing being quarantined is answered with success
static int
pd_qtn_policy_syscall(struct proc *p, int call, user_addr_t arg)
{
	(void)p;
	(void)call;
	(void)arg;
	return 0;
}

static struct mac_policy_ops pd_qtn_ops = {
	.mpo_policy_syscall = pd_qtn_policy_syscall,
};

static struct mac_policy_conf pd_qtn_policy = {
	.mpc_name = "Quarantine",
	.mpc_fullname = "PureDarwin Quarantine (nothing quarantined)",
	.mpc_labelnames = NULL,
	.mpc_labelname_count = 0,
	.mpc_ops = &pd_qtn_ops,
	.mpc_loadtime_flags = 0,
	.mpc_field_off = NULL,
	.mpc_runtime_flags = 0,
};

static mac_policy_handle_t pd_qtn_handle;

static struct mac_policy_conf pd_amfi_policy = {
	.mpc_name = "AMFI",
	.mpc_fullname = "PureDarwin Apple Mobile File Integrity",
	.mpc_labelnames = NULL,
	.mpc_labelname_count = 0,
	.mpc_ops = &pd_amfi_ops,
	.mpc_loadtime_flags = 0,
	.mpc_field_off = NULL,
	.mpc_runtime_flags = 0,
};

static mac_policy_handle_t pd_amfi_handle;

kern_return_t
amfi_kext_start(kmod_info_t *ki, void *d)
{
	int error;

	(void)ki;
	amfi_interface_register(&pd_amfi_interface);
	amfi_core_entitlements_register((const CEKernelAPI_t *)&pd_core_entitlements_storage);
	error = mac_policy_register(&pd_amfi_policy, &pd_amfi_handle, d);
	if (error != 0) {
		printf("amfi: policy registration failed (%d)\n", error);
	}
	error = mac_policy_register(&pd_qtn_policy, &pd_qtn_handle, d);
	if (error != 0) {
		printf("amfi: Quarantine policy registration failed (%d)\n", error);
	}
	printf("amfi: permissive stub registered\n");
	return KERN_SUCCESS;
}

kern_return_t
amfi_kext_stop(kmod_info_t *ki, void *d)
{
	(void)ki;
	(void)d;
	return KERN_FAILURE;
}
