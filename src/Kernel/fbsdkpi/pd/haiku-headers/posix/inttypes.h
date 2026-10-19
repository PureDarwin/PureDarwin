/*
 * Copyright 2002-2017 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _INTTYPES_H_
#define _INTTYPES_H_


#include <stdint.h>


typedef struct {
	intmax_t	quot;	/* quotient */
	intmax_t	rem;	/* remainder */
} imaxdiv_t;

/* fprintf() macros for signed integers */
#	define PRId8			"d"
#	define PRIdLEAST8		"d"
#	define PRIdFAST8		"d"
#	define PRIi8			"i"
#	define PRIiLEAST8		"i"
#	define PRIiFAST8		"i"

#	define PRId16			"d"
#	define PRIdLEAST16		"d"
#	define PRIdFAST16		"d"
#	define PRIi16			"i"
#	define PRIiLEAST16		"i"
#	define PRIiFAST16		"i"

#	define PRId32			__HAIKU_STD_PRI_PREFIX_32 "d"
#	define PRIdLEAST32		PRId32
#	define PRIdFAST32		PRId32
#	define PRIi32			__HAIKU_STD_PRI_PREFIX_32 "i"
#	define PRIiLEAST32		PRIi32
#	define PRIiFAST32		PRIi32

#	define PRId64			__HAIKU_STD_PRI_PREFIX_64 "d"
#	define PRIdLEAST64		PRId64
#	define PRIdFAST64		PRId64
#	define PRIi64			__HAIKU_STD_PRI_PREFIX_64 "i"
#	define PRIiLEAST64		PRIi64
#	define PRIiFAST64		PRIi64

#	define PRIdMAX			PRId64
#	define PRIdPTR			__HAIKU_PRI_PREFIX_ADDR "d"
#	define PRIiMAX			PRIi64
#	define PRIiPTR			__HAIKU_PRI_PREFIX_ADDR "i"

/* fprintf() macros for unsigned integers */
#	define PRIu8			"u"
#	define PRIuLEAST8		"u"
#	define PRIuFAST8		"u"
#	define PRIo8			"o"
#	define PRIoLEAST8		"o"
#	define PRIoFAST8		"o"
#	define PRIx8			"x"
#	define PRIxLEAST8		"x"
#	define PRIxFAST8		"x"
#	define PRIX8			"X"
#	define PRIXLEAST8		"X"
#	define PRIXFAST8		"X"

#	define PRIu16			"u"
#	define PRIuLEAST16		"u"
#	define PRIuFAST16		"u"
#	define PRIo16			"o"
#	define PRIoLEAST16		"o"
#	define PRIoFAST16		"o"
#	define PRIx16			"x"
#	define PRIxLEAST16		"x"
#	define PRIxFAST16		"x"
#	define PRIX16			"X"
#	define PRIXLEAST16		"X"
#	define PRIXFAST16		"X"

#	define PRIu32			__HAIKU_STD_PRI_PREFIX_32 "u"
#	define PRIuLEAST32		PRIu32
#	define PRIuFAST32		PRIu32
#	define PRIo32			__HAIKU_STD_PRI_PREFIX_32 "o"
#	define PRIoLEAST32		PRIo32
#	define PRIoFAST32		PRIo32
#	define PRIx32			__HAIKU_STD_PRI_PREFIX_32 "x"
#	define PRIxLEAST32		PRIx32
#	define PRIxFAST32		PRIx32
#	define PRIX32			__HAIKU_STD_PRI_PREFIX_32 "X"
#	define PRIXLEAST32		PRIX32
#	define PRIXFAST32		PRIX32

#	define PRIu64			__HAIKU_STD_PRI_PREFIX_64 "u"
#	define PRIuLEAST64		PRIu64
#	define PRIuFAST64		PRIu64
#	define PRIo64			__HAIKU_STD_PRI_PREFIX_64 "o"
#	define PRIoLEAST64		PRIo64
#	define PRIoFAST64		PRIo64
#	define PRIx64			__HAIKU_STD_PRI_PREFIX_64 "x"
#	define PRIxLEAST64		PRIx64
#	define PRIxFAST64		PRIx64
#	define PRIX64			__HAIKU_STD_PRI_PREFIX_64 "X"
#	define PRIXLEAST64		PRIX64
#	define PRIXFAST64		PRIX64

#	define PRIuMAX			PRIu64
#	define PRIuPTR			__HAIKU_PRI_PREFIX_ADDR "u"
#	define PRIoMAX			PRIo64
#	define PRIoPTR			__HAIKU_PRI_PREFIX_ADDR "o"
#	define PRIxMAX			PRIx64
#	define PRIxPTR			__HAIKU_PRI_PREFIX_ADDR "x"
#	define PRIXMAX			PRIX64
#	define PRIXPTR			__HAIKU_PRI_PREFIX_ADDR "X"

/* fscanf() macros for signed integers */
#	define SCNd8			"hhd"
#	define SCNdLEAST8		"hhd"
#	define SCNdFAST8		"d"
#	define SCNi8			"hhi"
#	define SCNiLEAST8		"hhi"
#	define SCNiFAST8		"i"

#	define SCNd16			"hd"
#	define SCNdLEAST16		"hd"
#	define SCNdFAST16		"d"
#	define SCNi16			"hi"
#	define SCNiLEAST16		"hi"
#	define SCNiFAST16		"i"

#	define SCNd32			__HAIKU_STD_PRI_PREFIX_32 "d"
#	define SCNdLEAST32		SCNd32
#	define SCNdFAST32		SCNd32
#	define SCNi32			__HAIKU_STD_PRI_PREFIX_32 "i"
#	define SCNiLEAST32		SCNi32
#	define SCNiFAST32		SCNi32

#	define SCNd64			__HAIKU_STD_PRI_PREFIX_64 "d"
#	define SCNdLEAST64		SCNd64
#	define SCNdFAST64		SCNd64
#	define SCNi64			__HAIKU_STD_PRI_PREFIX_64 "i"
#	define SCNiLEAST64		SCNi64
#	define SCNiFAST64		SCNi64

#	define SCNdMAX			SCNd64
#	define SCNdPTR			__HAIKU_PRI_PREFIX_ADDR "d"
#	define SCNiMAX			SCNi64
#	define SCNiPTR			__HAIKU_PRI_PREFIX_ADDR "i"

/* fscanf() macros for unsigned integers */
#	define SCNu8			"hhu"
#	define SCNuLEAST8		"hhu"
#	define SCNuFAST8		"u"
#	define SCNo8			"hho"
#	define SCNoLEAST8		"hho"
#	define SCNoFAST8		"o"
#	define SCNx8			"hhx"
#	define SCNxLEAST8		"hhx"
#	define SCNxFAST8		"x"

#	define SCNu16			"hu"
#	define SCNuLEAST16		"hu"
#	define SCNuFAST16		"u"
#	define SCNo16			"ho"
#	define SCNoLEAST16		"ho"
#	define SCNoFAST16		"o"
#	define SCNx16			"hx"
#	define SCNxLEAST16		"hx"
#	define SCNxFAST16		"x"

#	define SCNu32			__HAIKU_STD_PRI_PREFIX_32 "u"
#	define SCNuLEAST32		SCNu32
#	define SCNuFAST32		SCNu32
#	define SCNo32			__HAIKU_STD_PRI_PREFIX_32 "o"
#	define SCNoLEAST32		SCNo32
#	define SCNoFAST32		SCNo32
#	define SCNx32			__HAIKU_STD_PRI_PREFIX_32 "x"
#	define SCNxLEAST32		SCNx32
#	define SCNxFAST32		SCNx32

#	define SCNu64			__HAIKU_STD_PRI_PREFIX_64 "u"
#	define SCNuLEAST64		SCNu64
#	define SCNuFAST64		SCNu64
#	define SCNo64			__HAIKU_STD_PRI_PREFIX_64 "o"
#	define SCNoLEAST64		SCNo64
#	define SCNoFAST64		SCNo64
#	define SCNx64			__HAIKU_STD_PRI_PREFIX_64 "x"
#	define SCNxLEAST64		SCNx64
#	define SCNxFAST64		SCNx64

#	define SCNuMAX			SCNu64
#	define SCNuPTR			__HAIKU_PRI_PREFIX_ADDR "u"
#	define SCNoMAX			SCNo64
#	define SCNoPTR			__HAIKU_PRI_PREFIX_ADDR "o"
#	define SCNxMAX			SCNx64
#	define SCNxPTR			__HAIKU_PRI_PREFIX_ADDR "x"


#ifdef __cplusplus
extern "C" {
#endif

extern intmax_t		imaxabs(intmax_t num);
extern imaxdiv_t	imaxdiv(intmax_t numer, intmax_t denom);

extern intmax_t		strtoimax(const char *string, char **_end, int base);
extern uintmax_t	strtoumax(const char *string, char **_end, int base);
/* extern intmax_t		wcstoimax(const __wchar_t *, __wchar_t **, int); */
/* extern uintmax_t	wcstoumax(const __wchar_t *, __wchar_t **, int); */

#ifdef __cplusplus
}
#endif


#endif	/* _INTTYPES_H_ */
