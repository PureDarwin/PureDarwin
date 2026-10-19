/*
 * Copyright 2018, Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_IFLIB_SYS_LIBKERN_H_
#define _FBSD_IFLIB_SYS_LIBKERN_H_

/* include the real sys/libkern.h */
#include_next <sys/libkern.h>


char *strsep(char **, const char *delim);


#endif /* _FBSD_IFLIB_SYS_LIBKERN_H_ */
