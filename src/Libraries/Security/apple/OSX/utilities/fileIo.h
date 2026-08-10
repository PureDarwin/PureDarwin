/*
 * Copyright (c) 2005-2007,2010,2012 Apple Inc. All Rights Reserved.
 */

#include <sys/types.h>

/*
 * Read entire file.
 */
#ifdef __cplusplus
extern "C" {
#endif

int readFileSizet(
	const char			*fileName,
	unsigned char		**bytes,		// mallocd and returned
	size_t              *numBytes);		// returned

int writeFile(
              const char			*fileName,
              const unsigned char	*bytes,
              unsigned              numBytes);

int writeFileSizet(
	const char			*fileName,
	const unsigned char	*bytes,
	size_t              numBytes);

#ifdef __cplusplus
}
#endif
