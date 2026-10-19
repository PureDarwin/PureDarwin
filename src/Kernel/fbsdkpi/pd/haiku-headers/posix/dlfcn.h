/*
 * Copyright 2003-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DLFCN_H
#define	_DLFCN_H


#include <sys/types.h>


#define RTLD_LAZY	0	/* relocations are performed as needed */
#define RTLD_NOW	1	/* the file gets relocated at load time */
#define RTLD_LOCAL	0	/* symbols are not available for relocating any other object */
#define RTLD_GLOBAL	2	/* all symbols are available */
#define RTLD_NOLOAD	4	/* do not load any new object */
#define RTLD_GROUP	8	/* do not lookup symbols in the global symbol table */

/* not-yet-POSIX extensions (dlsym() handles) */
#define RTLD_DEFAULT	((void*)0)
	/* find the symbol in the global scope */
#define RTLD_NEXT		((void*)-1L)
	/* find the next definition of the symbol */

#ifdef __cplusplus
extern "C" {
#endif

/* This is a gnu extension for the dladdr function */
typedef struct {
   const char *dli_fname;  /* Filename of defining object */
   void *dli_fbase;        /* Load address of that object */
   const char *dli_sname;  /* Name of nearest lower symbol */
   void *dli_saddr;        /* Exact value of nearest symbol */
} Dl_info_t;
typedef Dl_info_t Dl_info;

extern int	dlclose(void *image);
extern char	*dlerror(void);
extern void	*dlopen(const char *path, int mode);
extern void *dlsym(void *image, const char *symbolName);
extern int dladdr(const void *addr, Dl_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* _DLFCN_H */
