/*
 * Copyright 2003-2007 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _PARSEDATE_H
#define _PARSEDATE_H


#include <time.h>


/* flags that will be set in the flags field by parsedate_etc() */
#define PARSEDATE_RELATIVE_TIME			0x0001
// ToDo: the following flags are not part of the R5 implementation and preliminary only
#define PARSEDATE_DAY_RELATIVE_TIME		PARSEDATE_RELATIVE_TIME
#define PARSEDATE_MINUTE_RELATIVE_TIME	0x0002
#define PARSEDATE_INVALID_DATE			0x0100


#ifdef __cplusplus
extern "C" {
#endif	/*  __cplusplus */

extern time_t parsedate(const char *dateString, time_t now);
extern time_t parsedate_etc(const char *dateString, time_t now, int *_storedFlags);

extern void set_dateformats(const char *table[]);
extern const char **get_dateformats(void);

#ifdef __cplusplus
}
#endif	/*  __cplusplus */

#endif	/* _PARSEDATE_H */
