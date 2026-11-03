/*
 * printerdb.h - printer database. PureDarwin reconstruction; layout is inert
 * because libinfo.c marks the API "no longer supported" and stubs every call.
 */

#ifndef _PRINTERDB_H_
#define _PRINTERDB_H_

#include <sys/cdefs.h>

typedef struct prdb_property {
	char *pp_key;
	char *pp_value;
} prdb_property;

typedef struct prdb_ent {
	char		**pe_name;
	unsigned int	  pe_nprops;
	prdb_property	 *pe_prop;
} prdb_ent;

__BEGIN_DECLS

const prdb_ent *prdb_getbyname(const char *name);
const prdb_ent *prdb_get(void);
void		prdb_set(const char *name);
void		prdb_end(void);

__END_DECLS

#endif /* _PRINTERDB_H_ */
