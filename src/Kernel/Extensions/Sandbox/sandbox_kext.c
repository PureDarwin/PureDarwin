/*
 * sandbox_kext.c - MACF policy enforcing the profiles libsandbox compiles.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <mach/mach_types.h>
#include <mach/kmod.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/vnode.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <kern/locks.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOLib.h>
#include <security/mac_policy.h>

#include "sandbox_profile.h"

extern struct label *mac_cred_label(kauth_cred_t cred);
extern int kauth_proc_label_update(struct proc *p, struct label *label);

kern_return_t sandbox_kext_start(kmod_info_t *ki, void *data);
kern_return_t sandbox_kext_stop(kmod_info_t *ki, void *data);

struct sb_kprofile {
	SInt32 refs;
	uint32_t default_action;
	uint32_t rule_count;
	uint32_t strings_size;
	vm_size_t alloc_size;
	const struct sb_rule *rules;
	const char *strings;
	uint8_t blob[];
};

enum sb_context { SB_CTX_PATH, SB_CTX_UNIX, SB_CTX_INET };

static int sb_slot = -1;
static lck_grp_t *sb_lock_group;
static lck_mtx_t *sb_apply_lock;
static struct sb_kprofile *sb_pending;
static SInt32 sb_denials_logged;
static mac_policy_handle_t sb_handle;

/* kauth_proc_label_update() passes this to our cred_label_update hook. */
static char sb_marker_storage;
#define SB_MARKER ((struct label *)&sb_marker_storage)

#define SB_REGEX_BUDGET 4096

static void
sb_profile_retain(struct sb_kprofile *kp)
{
	OSIncrementAtomic(&kp->refs);
}

static void
sb_profile_release(struct sb_kprofile *kp)
{
	if (OSDecrementAtomic(&kp->refs) == 0) {
		IOFree(kp, kp->alloc_size);
	}
}

static struct sb_kprofile *
sb_cred_profile(kauth_cred_t cred)
{
	struct label *label;

	if (cred == NULL || sb_slot < 0) {
		return NULL;
	}
	label = mac_cred_label(cred);
	return label ? (struct sb_kprofile *)mac_label_get(label, sb_slot) : NULL;
}

static int
sb_profile_load(user_addr_t addr, uint64_t size, struct sb_kprofile **out)
{
	const struct sb_profile_header *header;
	struct sb_kprofile *kp;
	uint64_t rules_size;
	vm_size_t alloc;
	int error;

	if (size < sizeof(*header) || size > SB_PROFILE_MAX_SIZE) {
		return EINVAL;
	}
	alloc = sizeof(*kp) + (vm_size_t)size;
	kp = IOMallocZero(alloc);
	if (kp == NULL) {
		return ENOMEM;
	}
	kp->alloc_size = alloc;
	kp->refs = 1;

	error = copyin(addr, kp->blob, (size_t)size);
	if (error != 0) {
		goto fail;
	}

	error = EINVAL;
	header = (const struct sb_profile_header *)kp->blob;
	if (header->magic != SB_PROFILE_MAGIC || header->version != SB_PROFILE_VERSION ||
	    header->default_action > SB_ACTION_DENY) {
		goto fail;
	}
	rules_size = (uint64_t)header->rule_count * sizeof(struct sb_rule);
	if (sizeof(*header) + rules_size + header->strings_size != size) {
		goto fail;
	}
	kp->default_action = header->default_action;
	kp->rule_count = header->rule_count;
	kp->strings_size = header->strings_size;
	kp->rules = (const struct sb_rule *)(kp->blob + sizeof(*header));
	kp->strings = (const char *)(kp->blob + sizeof(*header) + rules_size);

	for (uint32_t i = 0; i < kp->rule_count; i++) {
		const struct sb_rule *r = &kp->rules[i];

		if (r->action > SB_ACTION_DENY || r->filter > SB_FILTER_MAX) {
			goto fail;
		}
		if (r->filter == SB_FILTER_ANY || r->filter == SB_FILTER_IP ||
		    r->filter == SB_FILTER_IP_LOCALHOST) {
			continue;
		}
		if ((uint64_t)r->str_offset + r->str_length >= kp->strings_size ||
		    kp->strings[r->str_offset + r->str_length] != '\0') {
			goto fail;
		}
	}
	*out = kp;
	return 0;

fail:
	IOFree(kp, alloc);
	return error;
}

/* A small regex subset: ^ $ . [] [^] * + ? and \ escapes, no groups or alternation. */
static const char *
sb_re_class_end(const char *re)
{
	const char *p = re + 1;

	if (*p == '^') {
		p++;
	}
	if (*p == ']') {
		p++;
	}
	while (*p != '\0' && *p != ']') {
		if (*p == '\\' && p[1] != '\0') {
			p++;
		}
		p++;
	}
	return *p == ']' ? p : NULL;
}

static bool
sb_re_class_match(const char *re, const char *end, unsigned char ch)
{
	const char *p = re + 1;
	bool negate = false, matched = false;

	if (*p == '^') {
		negate = true;
		p++;
	}
	while (p < end) {
		unsigned char lo = (unsigned char)*p;

		if (lo == '\\' && p + 1 < end) {
			lo = (unsigned char)*++p;
		}
		if (p + 2 < end && p[1] == '-') {
			unsigned char hi = (unsigned char)p[2];

			if (ch >= lo && ch <= hi) {
				matched = true;
			}
			p += 3;
		} else {
			if (ch == lo) {
				matched = true;
			}
			p++;
		}
	}
	return negate ? !matched : matched;
}

static size_t
sb_re_atom_length(const char *re)
{
	if (*re == '[') {
		const char *end = sb_re_class_end(re);

		return end ? (size_t)(end - re + 1) : 0;
	}
	if (*re == '\\') {
		return re[1] != '\0' ? 2 : 0;
	}
	return 1;
}

static bool
sb_re_atom_match(const char *re, size_t length, char ch)
{
	if (ch == '\0') {
		return false;
	}
	if (*re == '[') {
		return sb_re_class_match(re, re + length - 1, (unsigned char)ch);
	}
	if (*re == '\\') {
		return re[1] == ch;
	}
	return *re == '.' || *re == ch;
}

static bool
sb_re_match_here(const char *re, const char *text, unsigned *budget)
{
	size_t length;
	char quantifier;

	if (*budget == 0) {
		return false;
	}
	(*budget)--;

	if (*re == '\0') {
		return true;
	}
	if (re[0] == '$' && re[1] == '\0') {
		return *text == '\0';
	}
	length = sb_re_atom_length(re);
	if (length == 0) {
		return false;
	}
	quantifier = re[length];
	if (quantifier == '*' || quantifier == '+' || quantifier == '?') {
		const char *rest = re + length + 1;
		size_t count = 0, minimum = (quantifier == '+') ? 1 : 0;

		while (text[count] != '\0' && sb_re_atom_match(re, length, text[count]) &&
		    (quantifier != '?' || count < 1)) {
			count++;
		}
		for (;;) {
			if (count >= minimum && sb_re_match_here(rest, text + count, budget)) {
				return true;
			}
			if (count == 0 || *budget == 0) {
				return false;
			}
			count--;
		}
	}
	return sb_re_atom_match(re, length, *text) && sb_re_match_here(re + length, text + 1, budget);
}

static bool
sb_regex_match(const char *re, const char *text)
{
	unsigned budget = SB_REGEX_BUDGET;

	if (*re == '^') {
		return sb_re_match_here(re + 1, text, &budget);
	}
	for (const char *t = text;; t++) {
		if (sb_re_match_here(re, t, &budget)) {
			return true;
		}
		if (*t == '\0' || budget == 0) {
			return false;
		}
	}
}

static bool
sb_rule_matches(const struct sb_kprofile *kp, const struct sb_rule *r, enum sb_context ctx,
    const char *path, size_t length, bool loopback)
{
	const char *s = kp->strings + r->str_offset;
	size_t n = r->str_length;

	switch (r->filter) {
	case SB_FILTER_ANY:
		return true;
	case SB_FILTER_LITERAL:
		return ctx != SB_CTX_INET && length == n && memcmp(path, s, n) == 0;
	case SB_FILTER_UNIX_LITERAL:
		return ctx == SB_CTX_UNIX && length == n && memcmp(path, s, n) == 0;
	case SB_FILTER_SUBPATH:
		if (ctx == SB_CTX_INET) {
			return false;
		}
		if (n == 1 && s[0] == '/') {
			return path[0] == '/';
		}
		return length >= n && memcmp(path, s, n) == 0 && (length == n || path[n] == '/');
	case SB_FILTER_REGEX:
		return ctx != SB_CTX_INET && sb_regex_match(s, path);
	case SB_FILTER_IP:
		return ctx == SB_CTX_INET;
	case SB_FILTER_IP_LOCALHOST:
		return ctx == SB_CTX_INET && loopback;
	default:
		return false;
	}
}

static const char *
sb_op_name(uint32_t op)
{
	switch (op) {
	case SB_OP_FILE_READ_DATA: return "file-read-data";
	case SB_OP_FILE_READ_METADATA: return "file-read-metadata";
	case SB_OP_FILE_WRITE_DATA: return "file-write-data";
	case SB_OP_FILE_WRITE_CREATE: return "file-write-create";
	case SB_OP_FILE_WRITE_UNLINK: return "file-write-unlink";
	case SB_OP_FILE_WRITE_SETUGID: return "file-write-setugid";
	case SB_OP_FILE_WRITE_ATTR: return "file-write-attr";
	case SB_OP_PROCESS_EXEC: return "process-exec";
	case SB_OP_NETWORK_OUTBOUND: return "network-outbound";
	case SB_OP_NETWORK_INBOUND: return "network-inbound";
	default: return "unknown";
	}
}

static int
sb_decide(kauth_cred_t cred, uint32_t op, enum sb_context ctx, const char *path,
    size_t length, bool loopback)
{
	const struct sb_kprofile *kp = sb_cred_profile(cred);
	uint32_t action;

	if (kp == NULL) {
		return 0;
	}
	action = kp->default_action;
	for (uint32_t i = 0; i < kp->rule_count; i++) {
		const struct sb_rule *r = &kp->rules[i];

		if ((r->ops & op) != 0 && sb_rule_matches(kp, r, ctx, path, length, loopback)) {
			action = r->action;
		}
	}
	if (action == SB_ACTION_ALLOW) {
		return 0;
	}
	if (OSIncrementAtomic(&sb_denials_logged) <= 1000) {
		char name[MAXCOMLEN + 1];

		proc_selfname(name, sizeof(name));
		printf("Sandbox: %s(%d) deny %s %s\n", name, proc_selfpid(), sb_op_name(op),
		    ctx == SB_CTX_INET ? "(ip)" : path);
	}
	return EPERM;
}

static int
sb_decide_ops(kauth_cred_t cred, uint32_t ops, const char *path, size_t length)
{
	for (uint32_t bit = 1; bit <= SB_OP_ALL; bit <<= 1) {
		if ((ops & bit) != 0) {
			int error = sb_decide(cred, bit, SB_CTX_PATH, path, length, false);

			if (error != 0) {
				return error;
			}
		}
	}
	return 0;
}

static int
sb_check_vnode(kauth_cred_t cred, struct vnode *vp, uint32_t ops)
{
	char path[MAXPATHLEN];
	int length = sizeof(path);

	if (sb_cred_profile(cred) == NULL) {
		return 0;
	}
	if (vp == NULL || vn_getpath(vp, path, &length) != 0 || length < 1) {
		path[0] = '\0';
		length = 1;
	}
	return sb_decide_ops(cred, ops, path, (size_t)length - 1);
}

static int
sb_check_name(kauth_cred_t cred, struct vnode *dvp, struct componentname *cnp, uint32_t ops)
{
	char path[MAXPATHLEN];
	int length = sizeof(path);
	size_t used;

	if (sb_cred_profile(cred) == NULL) {
		return 0;
	}
	if (dvp == NULL || vn_getpath(dvp, path, &length) != 0 || length < 1) {
		path[0] = '\0';
		length = 1;
	}
	used = (size_t)length - 1;
	if (cnp != NULL && cnp->cn_nameptr != NULL) {
		size_t namelen = (size_t)cnp->cn_namelen;

		if (used + 1 + namelen + 1 > sizeof(path)) {
			return ENAMETOOLONG;
		}
		if (used == 0 || path[used - 1] != '/') {
			path[used++] = '/';
		}
		memcpy(path + used, cnp->cn_nameptr, namelen);
		used += namelen;
		path[used] = '\0';
	}
	return sb_decide_ops(cred, ops, path, used);
}

static int
sb_check_sockaddr(kauth_cred_t cred, struct sockaddr *sa, uint32_t op)
{
	if (sa == NULL || sb_cred_profile(cred) == NULL) {
		return 0;
	}
	switch (sa->sa_family) {
	case AF_UNIX: {
		struct sockaddr_un *sun = (struct sockaddr_un *)sa;
		char path[sizeof(sun->sun_path) + 1];
		size_t offset = offsetof(struct sockaddr_un, sun_path), max = 0, n = 0;

		if (sun->sun_len > offset) {
			max = sun->sun_len - offset;
		}
		if (max > sizeof(sun->sun_path)) {
			max = sizeof(sun->sun_path);
		}
		while (n < max && sun->sun_path[n] != '\0') {
			path[n] = sun->sun_path[n];
			n++;
		}
		path[n] = '\0';
		return sb_decide(cred, op, SB_CTX_UNIX, path, n, false);
	}
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		bool loopback = (ntohl(sin->sin_addr.s_addr) >> 24) == IN_LOOPBACKNET;

		return sb_decide(cred, op, SB_CTX_INET, "", 0, loopback);
	}
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
		bool loopback = IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr) ||
		    (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr) && sin6->sin6_addr.s6_addr[12] == IN_LOOPBACKNET);

		return sb_decide(cred, op, SB_CTX_INET, "", 0, loopback);
	}
	default:
		return 0;
	}
}

static void
sb_cred_label_associate(kauth_cred_t parent, kauth_cred_t child)
{
	struct sb_kprofile *kp = sb_cred_profile(parent);

	if (kp != NULL) {
		sb_profile_retain(kp);
		mac_label_set(mac_cred_label(child), sb_slot, (intptr_t)kp);
	}
}

static void
sb_cred_label_update(kauth_cred_t cred, struct label *newlabel)
{
	struct sb_kprofile *old;

	if (newlabel != SB_MARKER || sb_pending == NULL) {
		return;
	}
	old = sb_cred_profile(cred);
	sb_profile_retain(sb_pending);
	mac_label_set(mac_cred_label(cred), sb_slot, (intptr_t)sb_pending);
	if (old != NULL) {
		sb_profile_release(old);
	}
}

static void
sb_cred_label_destroy(struct label *label)
{
	struct sb_kprofile *kp = (struct sb_kprofile *)mac_label_get(label, sb_slot);

	if (kp != NULL) {
		sb_profile_release(kp);
	}
}

static int
sb_policy_syscall(struct proc *p, int call, user_addr_t arg)
{
	struct sb_set_profile_args args;
	struct sb_kprofile *kp;
	kauth_cred_t cred;
	bool sandboxed;
	int error;

	if (call != SB_CALL_SET_PROFILE) {
		return EINVAL;
	}
	error = copyin(arg, &args, sizeof(args));
	if (error != 0) {
		return error;
	}

	/* A profile can only ever be narrowed by not being replaced. */
	cred = kauth_cred_proc_ref(p);
	sandboxed = sb_cred_profile(cred) != NULL;
	kauth_cred_unref(&cred);
	if (sandboxed) {
		return EPERM;
	}

	error = sb_profile_load((user_addr_t)args.profile, args.size, &kp);
	if (error != 0) {
		return error;
	}
	lck_mtx_lock(sb_apply_lock);
	sb_pending = kp;
	kauth_proc_label_update(p, SB_MARKER);
	sb_pending = NULL;
	lck_mtx_unlock(sb_apply_lock);
	sb_profile_release(kp);
	return 0;
}

static int
sb_vnode_check_open(kauth_cred_t cred, struct vnode *vp, struct label *label, int acc_mode)
{
	uint32_t ops = 0;

	if (acc_mode & FREAD) {
		ops |= SB_OP_FILE_READ_DATA;
	}
	if (acc_mode & FWRITE) {
		ops |= SB_OP_FILE_WRITE_DATA;
	}
	return sb_check_vnode(cred, vp, ops ? ops : SB_OP_FILE_READ_METADATA);
}

static int
sb_vnode_check_access(kauth_cred_t cred, struct vnode *vp, struct label *label, int acc_mode)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_READ_METADATA);
}

static int
sb_vnode_check_create(kauth_cred_t cred, struct vnode *dvp, struct label *dlabel,
    struct componentname *cnp, struct vnode_attr *vap)
{
	return sb_check_name(cred, dvp, cnp, SB_OP_FILE_WRITE_CREATE);
}

static int
sb_vnode_check_link(kauth_cred_t cred, struct vnode *dvp, struct label *dlabel,
    struct vnode *vp, struct label *label, struct componentname *cnp)
{
	return sb_check_name(cred, dvp, cnp, SB_OP_FILE_WRITE_CREATE);
}

static int
sb_vnode_check_unlink(kauth_cred_t cred, struct vnode *dvp, struct label *dlabel,
    struct vnode *vp, struct label *label, struct componentname *cnp)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_UNLINK);
}

static int
sb_vnode_check_rename(kauth_cred_t cred, struct vnode *fdvp, struct label *fdlabel,
    struct vnode *fvp, struct label *flabel, struct componentname *fcnp,
    struct vnode *tdvp, struct label *tdlabel, struct vnode *tvp, struct label *tlabel,
    struct componentname *tcnp)
{
	int error = sb_check_vnode(cred, fvp, SB_OP_FILE_WRITE_UNLINK);

	if (error == 0) {
		error = sb_check_name(cred, tdvp, tcnp, SB_OP_FILE_WRITE_CREATE);
	}
	if (error == 0 && tvp != NULL) {
		error = sb_check_vnode(cred, tvp, SB_OP_FILE_WRITE_UNLINK);
	}
	return error;
}

static int
sb_vnode_check_truncate(kauth_cred_t active_cred, kauth_cred_t file_cred,
    struct vnode *vp, struct label *label)
{
	return sb_check_vnode(active_cred, vp, SB_OP_FILE_WRITE_DATA);
}

static int
sb_vnode_check_setmode(kauth_cred_t cred, struct vnode *vp, struct label *label, mode_t mode)
{
	uint32_t ops = SB_OP_FILE_WRITE_ATTR;

	if (mode & (S_ISUID | S_ISGID)) {
		ops |= SB_OP_FILE_WRITE_SETUGID;
	}
	return sb_check_vnode(cred, vp, ops);
}

static int
sb_vnode_check_setowner(kauth_cred_t cred, struct vnode *vp, struct label *label,
    uid_t uid, gid_t gid)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

static int
sb_vnode_check_setflags(kauth_cred_t cred, struct vnode *vp, struct label *label, u_long flags)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

static int
sb_vnode_check_setutimes(kauth_cred_t cred, struct vnode *vp, struct label *label,
    struct timespec atime, struct timespec mtime)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

/* The attribute data is not passed here, so a mode set this way is not checked for setugid. */
static int
sb_vnode_check_setattrlist(kauth_cred_t cred, struct vnode *vp, struct label *vlabel,
    struct attrlist *alist)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

static int
sb_vnode_check_setextattr(kauth_cred_t cred, struct vnode *vp, struct label *label,
    const char *name, struct uio *uio)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

static int
sb_vnode_check_deleteextattr(kauth_cred_t cred, struct vnode *vp, struct label *vlabel,
    const char *name)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

static int
sb_vnode_check_setacl(kauth_cred_t cred, struct vnode *vp, struct label *label,
    struct kauth_acl *acl)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_WRITE_ATTR);
}

static int
sb_vnode_check_exec(kauth_cred_t cred, struct vnode *vp, struct vnode *scriptvp,
    struct label *vnodelabel, struct label *scriptlabel, struct label *execlabel,
    struct componentname *cnp, u_int *csflags, void *macpolicyattr, size_t macpolicyattrlen)
{
	int error = sb_check_vnode(cred, vp, SB_OP_PROCESS_EXEC);

	if (error == 0 && scriptvp != NULL) {
		error = sb_check_vnode(cred, scriptvp, SB_OP_PROCESS_EXEC);
	}
	return error;
}

static int
sb_vnode_check_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{
	return sb_check_vnode(active_cred, vp, SB_OP_FILE_READ_METADATA);
}

static int
sb_vnode_check_getattrlist(kauth_cred_t cred, struct vnode *vp, struct label *vlabel,
    struct attrlist *alist, uint64_t options)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_READ_METADATA);
}

static int
sb_vnode_check_readlink(kauth_cred_t cred, struct vnode *vp, struct label *label)
{
	return sb_check_vnode(cred, vp, SB_OP_FILE_READ_METADATA);
}

static int
sb_vnode_check_readdir(kauth_cred_t cred, struct vnode *dvp, struct label *dlabel)
{
	return sb_check_vnode(cred, dvp, SB_OP_FILE_READ_DATA);
}

static int
sb_socket_check_connect(kauth_cred_t cred, socket_t so, struct label *socklabel,
    struct sockaddr *addr)
{
	return sb_check_sockaddr(cred, addr, SB_OP_NETWORK_OUTBOUND);
}

static int
sb_socket_check_bind(kauth_cred_t cred, socket_t so, struct label *socklabel,
    struct sockaddr *addr)
{
	return sb_check_sockaddr(cred, addr, SB_OP_NETWORK_INBOUND);
}

static struct mac_policy_ops sb_ops = {
	.mpo_cred_label_associate = sb_cred_label_associate,
	.mpo_cred_label_destroy = sb_cred_label_destroy,
	.mpo_cred_label_update = sb_cred_label_update,
	.mpo_policy_syscall = sb_policy_syscall,
	.mpo_socket_check_bind = sb_socket_check_bind,
	.mpo_socket_check_connect = sb_socket_check_connect,
	.mpo_vnode_check_access = sb_vnode_check_access,
	.mpo_vnode_check_create = sb_vnode_check_create,
	.mpo_vnode_check_deleteextattr = sb_vnode_check_deleteextattr,
	.mpo_vnode_check_exec = sb_vnode_check_exec,
	.mpo_vnode_check_getattrlist = sb_vnode_check_getattrlist,
	.mpo_vnode_check_link = sb_vnode_check_link,
	.mpo_vnode_check_open = sb_vnode_check_open,
	.mpo_vnode_check_readdir = sb_vnode_check_readdir,
	.mpo_vnode_check_readlink = sb_vnode_check_readlink,
	.mpo_vnode_check_rename = sb_vnode_check_rename,
	.mpo_vnode_check_setacl = sb_vnode_check_setacl,
	.mpo_vnode_check_setattrlist = sb_vnode_check_setattrlist,
	.mpo_vnode_check_setextattr = sb_vnode_check_setextattr,
	.mpo_vnode_check_setflags = sb_vnode_check_setflags,
	.mpo_vnode_check_setmode = sb_vnode_check_setmode,
	.mpo_vnode_check_setowner = sb_vnode_check_setowner,
	.mpo_vnode_check_setutimes = sb_vnode_check_setutimes,
	.mpo_vnode_check_stat = sb_vnode_check_stat,
	.mpo_vnode_check_truncate = sb_vnode_check_truncate,
	.mpo_vnode_check_unlink = sb_vnode_check_unlink,
};

static struct mac_policy_conf sb_policy = {
	.mpc_name = SB_POLICY_NAME,
	.mpc_fullname = "PureDarwin Sandbox",
	.mpc_labelnames = NULL,
	.mpc_labelname_count = 0,
	.mpc_ops = &sb_ops,
	/* Credentials keep pointers into this kext, so it can never unload. */
	.mpc_loadtime_flags = 0,
	.mpc_field_off = &sb_slot,
	.mpc_runtime_flags = 0,
};

kern_return_t
sandbox_kext_start(kmod_info_t *ki, void *data)
{
	int error;

	sb_lock_group = lck_grp_alloc_init("Sandbox", LCK_GRP_ATTR_NULL);
	sb_apply_lock = lck_mtx_alloc_init(sb_lock_group, LCK_ATTR_NULL);

	error = mac_policy_register(&sb_policy, &sb_handle, data);
	if (error != 0) {
		printf("Sandbox: policy registration failed (%d)\n", error);
		return KERN_FAILURE;
	}
	printf("Sandbox: policy registered (label slot %d)\n", sb_slot);
	return KERN_SUCCESS;
}

kern_return_t
sandbox_kext_stop(kmod_info_t *ki, void *data)
{
	return KERN_FAILURE;
}
