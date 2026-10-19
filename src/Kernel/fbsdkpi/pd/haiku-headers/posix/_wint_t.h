/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef __WINT_T_H
#define __WINT_T_H


#include <stddef.h>


/* stddef.h is not supposed to define wint_t, but gcc 2.95.3's one does.
 * In all other cases we will do that. */
#ifndef _WINT_T
#define _WINT_T

#ifndef __WINT_TYPE__
#define __WINT_TYPE__ unsigned int
#endif

typedef __WINT_TYPE__ wint_t;

#endif	/* _WINT_T */

#define WEOF		((wint_t)(-1))

typedef int wctype_t;


#endif /* __WINT_T_H */
