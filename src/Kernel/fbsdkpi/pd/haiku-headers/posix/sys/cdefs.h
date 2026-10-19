#ifndef _SYS_CDEFS_H
#define _SYS_CDEFS_H


#ifndef __THROW
#define __THROW
#endif

#ifndef __P
#define	__P(s) s
#endif

#ifndef __CONCAT
#define	__CONCAT(x,y)	x ## y
#endif

#ifndef __STRING
#define	__STRING(x)		#x
#endif

#if defined(__cplusplus)
#define	__BEGIN_DECLS	extern "C" {
#define	__END_DECLS	};
#else
#define	__BEGIN_DECLS
#define	__END_DECLS
#endif

#define __dead
#define __dead2

#ifndef __weak_reference
#define	__weak_reference(sym,alias)	\
	__asm__(".weak " #alias);	\
	__asm__(".equ "  #alias ", " #sym)
#endif

#endif
