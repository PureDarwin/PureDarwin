/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_LOCK_H_
#define _FBSD_COMPAT_SYS_LOCK_H_


#define MPASS(ex)				MPASS4(ex, #ex, __FILE__, __LINE__)
#define MPASS2(ex, what)		MPASS4(ex, what, __FILE__, __LINE__)
#define MPASS3(ex, file, line)	MPASS4(ex, #ex, file, line)
#define MPASS4(ex, what, file, line)					\
	KASSERT((ex), ("assert %s failed at %s:%d", what, file, line))


#endif /* _FBSD_COMPAT_SYS_LOCK_H_ */