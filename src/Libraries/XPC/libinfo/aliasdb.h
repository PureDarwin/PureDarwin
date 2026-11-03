/*
 * aliasdb.h - mail alias database. PureDarwin reconstruction: the header only
 * existed in the SDK, so struct aliasent is derived from the payload
 * file_module.c:1124 builds, LI_ils_create("L4488s4*4", ...).
 */

#ifndef _ALIASDB_H_
#define _ALIASDB_H_

#include <stdint.h>
#include <sys/cdefs.h>
#include <mach/port.h>
#include <mach/message.h>

/* 32-bit, not glibc's size_t: ils.c's '4' emits exactly four bytes. */
struct aliasent {
	char		 *alias_name;
	uint32_t	  alias_members_len;
	char		**alias_members;
	uint32_t	  alias_local;
};

__BEGIN_DECLS

struct aliasent *alias_getbyname(const char *name);
struct aliasent *alias_getent(void);
void		 alias_setent(void);
void		 alias_endent(void);

/* Callback type spelled out rather than pulled from libinfo.h, to stay standalone. */
mach_port_t	 alias_getbyname_async_call(const char *name,
		     void (*callback)(struct aliasent *, void *context),
		     void *context);
void		 alias_getbyname_async_handle_reply(mach_msg_header_t *msg);

__END_DECLS

#endif /* _ALIASDB_H_ */
