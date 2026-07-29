/*
 * membership.h - PureDarwin stand-in for Libinfo's membership.subproj.
 *
 * The real mbr_* API resolves uid/gid <-> UUID and supplementary-group
 * membership through opendirectoryd over private xpc_pipe SPI. PureDarwin has
 * neither, and Libinfo's membership.c cannot be built here for the same reason.
 *
 * asl_core.c uses these only as the *fallback* leg of its read-access check:
 * the direct comparisons (msgg == -1, msgg == readg) are evaluated first, and
 * the mbr_* path only asks "is this user in that group by some indirect
 * membership". With no directory service to consult, the honest answer is
 * "no" - which fails that check closed, denying access rather than granting
 * it. Erring the other way would silently widen ASL read access.
 */

#ifndef _PUREDARWIN_MEMBERSHIP_H_
#define _PUREDARWIN_MEMBERSHIP_H_

#include <errno.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int
mbr_uid_to_uuid(uid_t id, uuid_t uu)
{
	(void)id;
	uuid_clear(uu);
	return ENOENT;
}

static inline int
mbr_gid_to_uuid(gid_t id, uuid_t uu)
{
	(void)id;
	uuid_clear(uu);
	return ENOENT;
}

static inline int
mbr_check_membership(uuid_t user, uuid_t group, int *ismember)
{
	(void)user;
	(void)group;
	if (ismember != NULL) {
		*ismember = 0;      /* no directory service: not a member */
	}
	return ENOENT;
}

#ifdef __cplusplus
}
#endif

#endif /* _PUREDARWIN_MEMBERSHIP_H_ */
