/*
 * Copyright 2004-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _GETOPT_H
#define _GETOPT_H


#include <unistd.h>


/* This header defines the available GNU extensions to the getopt() functionality */

struct option {
	const char	*name;
	int			has_arg;
	int			*flag;
	int			val;
};

/* Options for the "has_arg" field */

#define no_argument			0
#define required_argument	1
#define optional_argument	2


#ifdef	__cplusplus
extern "C" {
#endif

extern int getopt_long(int argc, char * const *argv, const char *shortOptions,
				const struct option *longOptions, int *_longIndex);
extern int getopt_long_only(int argc, char * const *argv, const char *shortOptions,
				const struct option *longOptions, int *_longIndex);

#ifdef	__cplusplus
}
#endif

#endif	/* _GETOPT_H */
