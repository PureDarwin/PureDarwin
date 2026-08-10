/*
 * libaks.h - compatibility shim for Apple's AppleKeyStore interface.
 *
 * The real libaks talks to the Secure Enclave / keystore kext and is not part
 * of any open-source release. Security includes it from exactly two places,
 * SecCFWrappers.h and SecAKSWrappers.h, and between them they use a single
 * function and a handful of constants.
 */

#ifndef _PD_LIBAKS_H_
#define _PD_LIBAKS_H_

#include <TargetConditionals.h>
#include <mach/kern_return.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

typedef int32_t keybag_handle_t;
typedef uint32_t keybag_state_t;

/*
 * Protection class of a keychain item. The keystore names these after the
 * data-protection classes (A through D, each with a "this device only"
 * variant); key_class_f is the always-available class used for items with no
 * protection requirement, which is the only one meaningful without a keybag.
 */
typedef uint32_t keyclass_t;

enum {
    key_class_ak = 6,
    key_class_ck = 7,
    key_class_dk = 8,
    key_class_aku = 9,
    key_class_cku = 10,
    key_class_dku = 11,
    key_class_akpu = 12,
    key_class_f = 13,
    key_class_last = key_class_f,
};

/* Well-known keybag handles. Negative values are the pseudo-handles the
 * keystore resolves at call time; only these three are referenced. */
enum {
    bad_keybag_handle       = -1,
    device_keybag_handle    = -1,
    session_keybag_handle   = -2,
};

/* State is a bitfield, not an enumeration of exclusive states. */
enum {
    keybag_state_locked         = 1 << 0,
    keybag_state_no_pin         = 1 << 1,
    keybag_state_been_unlocked  = 1 << 2,
};

/* kAKSReturn* are kern_return_t values; Security compares against these when
 * deciding whether a failure is transient. */
enum {
    kAKSReturnSuccess       = 0,
    kAKSReturnError         = -1,
    kAKSReturnBusy          = -2,
    kAKSReturnNoPermission  = -3,
};

/*
 * A reference key is a key the keystore holds on the caller's behalf, handed
 * out as an opaque handle so the key material never leaves the keystore. With
 * no keystore there is nothing to hold a key, so creation always fails and
 * callers fall back to their software path.
 */
typedef struct aks_ref_key_s *aks_ref_key_t;

enum {
    key_type_sym            = 0,
    key_type_asym_ec_p256   = 1,
};

static inline int
aks_ref_key_create(keybag_handle_t handle, keyclass_t cls, int type,
                   const uint8_t *params, size_t params_len,
                   aks_ref_key_t *ref)
{
    (void)handle; (void)cls; (void)type; (void)params; (void)params_len;
    if (ref != NULL) {
        *ref = NULL;
    }
    return kAKSReturnNoPermission;
}

static inline const uint8_t *
aks_ref_key_get_public_key(aks_ref_key_t ref, size_t *len)
{
    (void)ref;
    if (len != NULL) {
        *len = 0;
    }
    return NULL;
}

static inline void
aks_ref_key_free(aks_ref_key_t *ref)
{
    if (ref != NULL) {
        *ref = NULL;
    }
}

/*
 * Report the lock state of a keybag. With no keystore there is nothing to be
 * locked, so this always succeeds and reports unlocked, with the
 * been-unlocked bit set (and no PIN, since there is none to set).
 */
static inline kern_return_t
aks_get_lock_state(keybag_handle_t handle, keybag_state_t *state)
{
    (void)handle;
    if (state != NULL) {
        *state = keybag_state_been_unlocked | keybag_state_no_pin;
    }
    return KERN_SUCCESS;
}

__END_DECLS

#endif /* _PD_LIBAKS_H_ */
