/*
 * Copyright 2020, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FNMATCH_H
#define _FNMATCH_H


#define FNM_NOESCAPE	0x01
#define FNM_PATHNAME	0x02
#define FNM_PERIOD		0x04

#define FNM_LEADING_DIR	0x08
#define FNM_CASEFOLD	0x10
#define FNM_IGNORECASE	FNM_CASEFOLD
#define FNM_FILE_NAME	FNM_PATHNAME

#define FNM_NOMATCH		1


#ifdef __cplusplus
extern "C" {
#endif

extern int fnmatch(const char *, const char *, int);

#ifdef __cplusplus
}
#endif

#endif /* _FNMATCH_H */
