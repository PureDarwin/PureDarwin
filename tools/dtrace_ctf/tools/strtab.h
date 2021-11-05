/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 2001 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STRTAB_H
#define	_STRTAB_H

#include <sys/types.h>

#include "atom.h"

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef __APPLE__
typedef ulong ulong_t;
#endif

typedef struct strhash strhash_t;

typedef struct strtab {
	strhash_t *str_hash;		/* array of hash buckets */
	char **str_bufs;		/* array of buffer pointers */
	char *str_ptr;			/* pointer to current buffer location */
	ulong_t str_nbufs;		/* size of buffer pointer array */
	size_t str_bufsz;		/* size of individual buffer */
	size_t str_size;		/* total size of strings in bytes */
} strtab_t;

extern void strtab_create(strtab_t *);
extern void strtab_destroy(strtab_t *);
extern size_t strtab_insert(strtab_t *, atom_t *);
extern size_t strtab_size(const strtab_t *);
extern ssize_t strtab_write(const strtab_t *,
    ssize_t (*)(const void *, size_t, void *), void *);

#ifdef	__cplusplus
}
#endif

#endif	/* _STRTAB_H */
