/*
 * OpenBSM's libbsm: the userspace audit-record library. We do not vendor
 * OpenBSM, but the audit_token_t decomposers below are not really part of it -
 * they only unpack a struct whose layout is fixed by the kernel, in
 * xnu bsd/kern/kern_prot.c:set_security_token_task_internal():
 *
 *	val[0] = ai_auid    val[4] = cr_rgid
 *	val[1] = cr_uid     val[5] = p_pid
 *	val[2] = cr_gid     val[6] = ai_asid
 *	val[3] = cr_ruid    val[7] = p_idversion
 *
 * so they are implemented here rather than declared and left dangling. The
 * rest of libbsm (audit record parsing, the audit trail) is genuinely absent.
 */

#ifndef _PUREDARWIN_BSM_LIBBSM_H_
#define _PUREDARWIN_BSM_LIBBSM_H_

#include <sys/types.h>
#include <bsm/audit.h>

__BEGIN_DECLS

static __inline__ void
audit_token_to_au32(audit_token_t atoken, uid_t *auidp, uid_t *euidp,
    gid_t *egidp, uid_t *ruidp, gid_t *rgidp, pid_t *pidp,
    au_asid_t *asidp, au_tid_t *tidp)
{
	if (auidp != NULL)  *auidp = (uid_t)atoken.val[0];
	if (euidp != NULL)  *euidp = (uid_t)atoken.val[1];
	if (egidp != NULL)  *egidp = (gid_t)atoken.val[2];
	if (ruidp != NULL)  *ruidp = (uid_t)atoken.val[3];
	if (rgidp != NULL)  *rgidp = (gid_t)atoken.val[4];
	if (pidp  != NULL)  *pidp  = (pid_t)atoken.val[5];
	if (asidp != NULL)  *asidp = (au_asid_t)atoken.val[6];
	if (tidp  != NULL)  tidp->port = atoken.val[7];
}

static __inline__ uid_t
audit_token_to_auid(audit_token_t atoken)   { return (uid_t)atoken.val[0]; }

static __inline__ uid_t
audit_token_to_euid(audit_token_t atoken)   { return (uid_t)atoken.val[1]; }

static __inline__ gid_t
audit_token_to_egid(audit_token_t atoken)   { return (gid_t)atoken.val[2]; }

static __inline__ uid_t
audit_token_to_ruid(audit_token_t atoken)   { return (uid_t)atoken.val[3]; }

static __inline__ gid_t
audit_token_to_rgid(audit_token_t atoken)   { return (gid_t)atoken.val[4]; }

static __inline__ pid_t
audit_token_to_pid(audit_token_t atoken)    { return (pid_t)atoken.val[5]; }

static __inline__ au_asid_t
audit_token_to_asid(audit_token_t atoken)   { return (au_asid_t)atoken.val[6]; }

static __inline__ uint32_t
audit_token_to_pidversion(audit_token_t atoken) { return atoken.val[7]; }

__END_DECLS

#endif /* _PUREDARWIN_BSM_LIBBSM_H_ */
