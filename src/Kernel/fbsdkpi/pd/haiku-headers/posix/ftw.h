/*
 * Copyright 2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_FTW_H_
#define	_FTW_H_


#include <sys/stat.h>


#define	FTW_F		0
#define	FTW_D		1
#define	FTW_DNR		2
#define	FTW_DP		3
#define	FTW_NS		4
#define	FTW_SL		5
#define	FTW_SLN		6

#define	FTW_PHYS	0x01
#define	FTW_MOUNT	0x02
#define	FTW_DEPTH	0x04
#define	FTW_CHDIR	0x08

struct FTW {
	int base;
	int level;
};


#ifdef __cplusplus
extern "C" {
#endif

int	ftw(const char *, int (*)(const char *, const struct stat *, int), int);
int	nftw(const char *, int (*)(const char *, const struct stat *, int,
	struct FTW *), int, int);

#ifdef __cplusplus
}
#endif


#endif	/* _FTW_H_ */
