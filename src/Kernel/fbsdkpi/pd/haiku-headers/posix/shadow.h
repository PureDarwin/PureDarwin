/*
 * Copyright 2008-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SHADOW_H_
#define _SHADOW_H_


#include <stddef.h>
#include <stdio.h>

struct spwd {
	char*	sp_namp;	/* login name */
	char*	sp_pwdp;	/* encrypted password */
	int		sp_lstchg;	/* date of last change (days since 1970) */
	int		sp_min;		/* min days between password changes */
	int		sp_max;		/* max days between password changes */
	int		sp_warn;	/* days to warn before password expired */
	int		sp_inact;	/* days of inactivity until account expiration */
	int		sp_expire;	/* date when the account expires (days since 1970) */
	int		sp_flag;	/* unused */
};


#ifdef __cplusplus
extern "C" {
#endif


extern struct spwd*	getspent(void);
extern int getspent_r(struct spwd* spwd, char* buffer, size_t bufferSize,
				struct spwd** _result);
extern void	setspent(void);
extern void	endspent(void);

extern struct spwd*	getspnam(const char* name);
extern int getspnam_r(const char* name, struct spwd* spwd, char* buffer,
				size_t bufferSize, struct spwd** _result);

extern struct spwd* sgetspent(const char* line);
extern int sgetspent_r(const char* line, struct spwd *spwd, char *buffer,
				size_t bufferSize, struct spwd** _result);

extern struct spwd* fgetspent(FILE* file);
extern int fgetspent_r(FILE* file, struct spwd* spwd, char* buffer,
				size_t bufferSize, struct spwd** _result);


#ifdef __cplusplus
}
#endif


#endif	/* _SHADOW_H_ */
