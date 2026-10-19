/*
 * Copyright 2010-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _NL_TYPES_H_
#define _NL_TYPES_H_


#include <sys/cdefs.h>


#define NL_SETD         0
#define NL_CAT_LOCALE   1

typedef int 	nl_item;
typedef void*	nl_catd;

__BEGIN_DECLS

extern nl_catd	catopen(const char *name, int oflag);
extern char*	catgets(nl_catd cat, int setID, int msgID,
					const char *defaultMessage);
extern int		catclose(nl_catd cat);

__END_DECLS


#endif /* _NL_TYPES_H_ */
