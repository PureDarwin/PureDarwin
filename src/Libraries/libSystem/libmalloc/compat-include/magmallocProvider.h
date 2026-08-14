/* PureDarwin: DTrace probes disabled (no dtrace on the build host to generate
 * this from magmallocProvider.d). These no-op stubs are exactly libmalloc's
 * own DARWINTEST fallback (src/dtrace.h) - functionally a probes-off build. */
#ifndef _MAGMALLOCPROVIDER_H
#define _MAGMALLOCPROVIDER_H
#define	MAGMALLOC_ALLOCREGION(arg0, arg1, arg2, arg3)
#define	MAGMALLOC_ALLOCREGION_ENABLED() (0)
#define	MAGMALLOC_DEALLOCREGION(arg0, arg1, arg2)
#define	MAGMALLOC_DEALLOCREGION_ENABLED() (0)
#define	MAGMALLOC_DEPOTREGION(arg0, arg1, arg2, arg3, arg4)
#define	MAGMALLOC_DEPOTREGION_ENABLED() (0)
#define	MAGMALLOC_MADVFREEREGION(arg0, arg1, arg2, arg3)
#define	MAGMALLOC_MADVFREEREGION_ENABLED() (0)
#define	MAGMALLOC_MALLOCERRORBREAK()
#define	MAGMALLOC_MALLOCERRORBREAK_ENABLED() (0)
#define	MAGMALLOC_PRESSURERELIEFBEGIN(arg0, arg1, arg2)
#define	MAGMALLOC_PRESSURERELIEFBEGIN_ENABLED() (0)
#define	MAGMALLOC_PRESSURERELIEFEND(arg0, arg1, arg2, arg3)
#define	MAGMALLOC_PRESSURERELIEFEND_ENABLED() (0)
#define	MAGMALLOC_RECIRCREGION(arg0, arg1, arg2, arg3, arg4)
#define	MAGMALLOC_RECIRCREGION_ENABLED() (0)
#define	MAGMALLOC_REFRESHINDEX(arg0, arg1, arg2)
#define	MAGMALLOC_REFRESHINDEX_ENABLED() (0)
#endif
