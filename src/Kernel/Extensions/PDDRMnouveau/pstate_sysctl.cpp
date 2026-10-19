// SPDX-License-Identifier: MIT
// kern.nouveau.<minor>.<name> string sysctls whose reads and writes call back into the driver

#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include <sys/errno.h>
#include <sys/sysctl.h>

extern "C" {
void *pd_nouveau_sysctl_add(int minor, const char *name,
    int (*read)(void *, char *, size_t), int (*write)(void *, const char *), void *cookie);
void pd_nouveau_sysctl_remove(void *handle);
}

#define PD_NV_TEXT 4096

// a per-gpu node and its one leaf, built at runtime since the minors are not known up front
struct pd_nv_sysctl {
	struct sysctl_oid node;
	struct sysctl_oid_list children;
	struct sysctl_oid leaf;
	char nodename[8];
	char leafname[16];
	int (*read)(void *, char *, size_t);
	int (*write)(void *, const char *);
	void *cookie;
};

// kern rather than hw, xnu exports only kern's child list to kexts
SYSCTL_DECL(_kern);
SYSCTL_NODE(_kern, OID_AUTO, nouveau, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "nouveau gpus, experimental");

static int sNodeUsers;

static int
pd_nv_sysctl_handler(__unused struct sysctl_oid *oidp, void *arg1, __unused int arg2,
    struct sysctl_req *req)
{
	struct pd_nv_sysctl *s = (struct pd_nv_sysctl *)arg1;
	char *text = (char *)IOMallocZero(PD_NV_TEXT);
	int error;

	if (text == NULL)
		return ENOMEM;
	if (req->newptr != USER_ADDR_NULL) {
		size_t len = req->newlen < PD_NV_TEXT - 1 ? req->newlen : PD_NV_TEXT - 1;

		error = SYSCTL_IN(req, text, len);
		if (error == 0) {
			text[len] = 0;
			error = s->write(s->cookie, text);
			error = error < 0 ? -error : error;
		}
		IOFree(text, PD_NV_TEXT);
		return error;
	}
	error = s->read(s->cookie, text, PD_NV_TEXT);
	error = error < 0 ? -error : error;
	if (error == 0)
		error = SYSCTL_OUT(req, text, strlen(text) + 1);
	IOFree(text, PD_NV_TEXT);
	return error;
}

void *
pd_nouveau_sysctl_add(int minor, const char *name,
    int (*read)(void *, char *, size_t), int (*write)(void *, const char *), void *cookie)
{
	struct pd_nv_sysctl *s = (struct pd_nv_sysctl *)IOMallocZero(sizeof(*s));

	if (s == NULL)
		return NULL;
	snprintf(s->nodename, sizeof(s->nodename), "%d", minor);
	strlcpy(s->leafname, name, sizeof(s->leafname));
	s->read = read;
	s->write = write;
	s->cookie = cookie;
	SLIST_INIT(&s->children);

	s->node.oid_parent = &sysctl__kern_nouveau_children;
	s->node.oid_number = OID_AUTO;
	s->node.oid_kind = CTLTYPE_NODE | CTLFLAG_RW | CTLFLAG_LOCKED;
	s->node.oid_arg1 = &s->children;
	s->node.oid_name = s->nodename;
	s->node.oid_fmt = "N";
	s->node.oid_descr = "one nouveau gpu";
	s->node.oid_version = SYSCTL_OID_VERSION;

	s->leaf.oid_parent = &s->children;
	s->leaf.oid_number = OID_AUTO;
	s->leaf.oid_kind = CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_ANYBODY | CTLFLAG_LOCKED;
	s->leaf.oid_arg1 = s;
	s->leaf.oid_handler = pd_nv_sysctl_handler;
	s->leaf.oid_name = s->leafname;
	s->leaf.oid_fmt = "A";
	s->leaf.oid_descr = "experimental, see the pstate notes";
	s->leaf.oid_version = SYSCTL_OID_VERSION;

	if (sNodeUsers++ == 0)
		sysctl_register_oid(&sysctl__kern_nouveau);
	sysctl_register_oid(&s->node);
	sysctl_register_oid(&s->leaf);
	return s;
}

void
pd_nouveau_sysctl_remove(void *handle)
{
	struct pd_nv_sysctl *s = (struct pd_nv_sysctl *)handle;

	if (s == NULL)
		return;
	sysctl_unregister_oid(&s->leaf);
	sysctl_unregister_oid(&s->node);
	if (--sNodeUsers == 0)
		sysctl_unregister_oid(&sysctl__kern_nouveau);
	IOFree(s, sizeof(*s));
}
