/*
 * Copyright 2013-2014 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCHITECTURE_H
#define _ARCHITECTURE_H


#include <sys/cdefs.h>

#include <sys/types.h>


__BEGIN_DECLS

const char*	get_architecture();
const char*	get_primary_architecture();
size_t		get_secondary_architectures(const char** architectures,
				size_t count);
size_t		get_architectures(const char** architectures, size_t count);
const char*	guess_architecture_for_path(const char* path);

__END_DECLS


/* C++ API */
#ifdef __cplusplus

#include <StringList.h>

status_t	get_secondary_architectures(BStringList& _architectures);
status_t	get_architectures(BStringList& _architectures);

#endif	/* __cplusplus */

#endif	/* _ARCHITECTURE_H */
