/*
 * Copyright 2010-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _LANGINFO_H_
#define _LANGINFO_H_


#include <locale.h>
#include <nl_types.h>
#include <sys/cdefs.h>


enum {
	CODESET,	/* codeset name */
	D_T_FMT,	/* string for formatting date and time */
	D_FMT,		/* date format string */
	T_FMT,		/* time format string */
	T_FMT_AMPM,	/* a.m. or p.m. time formatting string */
	AM_STR,		/* Ante Meridian affix */
	PM_STR,		/* Post Meridian affix */

	/* week day names */
	DAY_1,
	DAY_2,
	DAY_3,
	DAY_4,
	DAY_5,
	DAY_6,
	DAY_7,

	/* abbreviated week day names */
	ABDAY_1,
	ABDAY_2,
	ABDAY_3,
	ABDAY_4,
	ABDAY_5,
	ABDAY_6,
	ABDAY_7,

	/* month names */
	MON_1,
	MON_2,
	MON_3,
	MON_4,
	MON_5,
	MON_6,
	MON_7,
	MON_8,
	MON_9,
	MON_10,
	MON_11,
	MON_12,

	/* abbreviated month names */
	ABMON_1,
	ABMON_2,
	ABMON_3,
	ABMON_4,
	ABMON_5,
	ABMON_6,
	ABMON_7,
	ABMON_8,
	ABMON_9,
	ABMON_10,
	ABMON_11,
	ABMON_12,

	ERA,			/* era description segments */
	ERA_D_FMT,		/* era date format string */
	ERA_D_T_FMT,	/* era date and time format string */
	ERA_T_FMT,		/* era time format string */
	ALT_DIGITS,		/* alternative symbols for digits */

	RADIXCHAR,		/* radix char */
	THOUSEP,		/* separator for thousands */

	YESEXPR,		/* affirmative response expression */
	NOEXPR,			/* negative response expression */

	CRNCYSTR,		/* currency symbol */

	_NL_LANGINFO_LAST
};

/* According to the POSIX base specs v7, the above need to be available as
   symbolic constants, so we define them individually to their respective
   enumeration name. */
#define CODESET CODESET
#define D_T_FMT D_T_FMT
#define D_FMT D_FMT
#define T_FMT T_FMT
#define T_FMT_AMPM T_FMT_AMPM
#define AM_STR AM_STR
#define PM_STR PM_STR
#define DAY_1 DAY_1
#define DAY_2 DAY_2
#define DAY_3 DAY_3
#define DAY_4 DAY_4
#define DAY_5 DAY_5
#define DAY_6 DAY_6
#define DAY_7 DAY_7
#define ABDAY_1 ABDAY_1
#define ABDAY_2 ABDAY_2
#define ABDAY_3 ABDAY_3
#define ABDAY_4 ABDAY_4
#define ABDAY_5 ABDAY_5
#define ABDAY_6 ABDAY_6
#define ABDAY_7 ABDAY_7
#define MON_1 MON_1
#define MON_2 MON_2
#define MON_3 MON_3
#define MON_4 MON_4
#define MON_5 MON_5
#define MON_6 MON_6
#define MON_7 MON_7
#define MON_8 MON_8
#define MON_9 MON_9
#define MON_10 MON_10
#define MON_11 MON_11
#define MON_12 MON_12
#define ABMON_1 ABMON_1
#define ABMON_2 ABMON_2
#define ABMON_3 ABMON_3
#define ABMON_4 ABMON_4
#define ABMON_5 ABMON_5
#define ABMON_6 ABMON_6
#define ABMON_7 ABMON_7
#define ABMON_8 ABMON_8
#define ABMON_9 ABMON_9
#define ABMON_10 ABMON_10
#define ABMON_11 ABMON_11
#define ABMON_12 ABMON_12
#define ERA ERA
#define ERA_D_FMT ERA_D_FMT
#define ERA_D_T_FMT ERA_D_T_FMT
#define ERA_T_FMT ERA_T_FMT
#define ALT_DIGITS ALT_DIGITS
#define RADIXCHAR RADIXCHAR
#define THOUSEP THOUSEP
#define YESEXPR YESEXPR
#define NOEXPR NOEXPR
#define CRNCYSTR CRNCYSTR

__BEGIN_DECLS

extern char*	nl_langinfo(nl_item item);
extern char*	nl_langinfo_l(nl_item item, locale_t locale);

__END_DECLS


#endif /* _LANGINFO_H_ */
