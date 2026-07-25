#include <mach/message.h>
#include <sys/types.h>
#include <bsm/audit.h>

void
audit_token_to_au32(audit_token_t atoken,
    uid_t *auidp, uid_t *euidp, gid_t *egidp,
    uid_t *ruidp, gid_t *rgidp, pid_t *pidp,
    au_asid_t *asidp, au_tid_t *tidp)
{
	if (auidp != NULL) {
		*auidp = atoken.val[0];
	}
	if (euidp != NULL) {
		*euidp = atoken.val[1];
	}
	if (egidp != NULL) {
		*egidp = atoken.val[2];
	}
	if (ruidp != NULL) {
		*ruidp = atoken.val[3];
	}
	if (rgidp != NULL) {
		*rgidp = atoken.val[4];
	}
	if (pidp != NULL) {
		*pidp = atoken.val[5];
	}
	if (asidp != NULL) {
		*asidp = atoken.val[6];
	}
	if (tidp != NULL) {
		tidp->port = atoken.val[7];
	}
}

/*
 * Real bsm/libbsm.h declares these as plain extern functions (not static
 * inline), so real Libc/libbsm.c must define them somewhere - these are
 * exactly that: thin single-field extractions via the real
 * audit_token_to_au32() field layout above.
 */
uid_t
audit_token_to_euid(audit_token_t atoken)
{
	uid_t euid;
	audit_token_to_au32(atoken, NULL, &euid, NULL, NULL, NULL, NULL, NULL, NULL);
	return euid;
}

gid_t
audit_token_to_egid(audit_token_t atoken)
{
	gid_t egid;
	audit_token_to_au32(atoken, NULL, NULL, &egid, NULL, NULL, NULL, NULL, NULL);
	return egid;
}

pid_t
audit_token_to_pid(audit_token_t atoken)
{
	pid_t pid;
	audit_token_to_au32(atoken, NULL, NULL, NULL, NULL, NULL, &pid, NULL, NULL);
	return pid;
}

au_asid_t
audit_token_to_asid(audit_token_t atoken)
{
	au_asid_t asid;
	audit_token_to_au32(atoken, NULL, NULL, NULL, NULL, NULL, NULL, &asid, NULL);
	return asid;
}
