/*
 * Copyright 2003-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STDIO_POST_H_
#define _STDIO_POST_H_


/* "Private"/inline functions of our BeOS compatible stdio implementation */

/* ToDo: this is a work in progress to make our stdio
 *		BeOS' GNU/libio (almost) binary compatible
 *		We may not yet be compatible! */

#ifndef _STDIO_H_
#	error "This file must be included from stdio.h!"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern char _single_threaded;
	/* this boolean value is true (1) if there is only the main thread
	 * running - as soon as you spawn the first thread, it's set to
	 * false (0) */

#ifdef __cplusplus
}
#endif

#define getc(stream) \
	(_single_threaded ? getc_unlocked(stream) : getc(stream))
#define putc(c, stream) \
	(_single_threaded ? putc_unlocked(c, stream) : putc(c, stream))

#endif	/* _STDIO_POST_H_ */
