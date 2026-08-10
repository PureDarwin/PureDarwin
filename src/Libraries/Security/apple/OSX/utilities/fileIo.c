/*
 * Copyright (c) 2005-2007,2010,2012-2013 Apple Inc. All Rights Reserved.
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include "fileIo.h"

int writeFile(
              const char			*fileName,
              const unsigned char	*bytes,
              unsigned              numBytes)
{
    size_t n = numBytes;
    return writeFileSizet(fileName, bytes, n);
}

int writeFileSizet(
	const char			*fileName,
	const unsigned char	*bytes,
	size_t              numBytes)
{
	int		rtn;
	int 	fd;
    ssize_t wrc;

    if (!fileName) {
        fwrite(bytes, 1, numBytes, stdout);
        fflush(stdout);
        return ferror(stdout);
    }

	fd = open(fileName, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if(fd == -1) {
		return errno;
	}
	wrc = write(fd, bytes, (size_t)numBytes);
	if(wrc != (ssize_t) numBytes) {
		if(wrc >= 0) {
			fprintf(stderr, "writeFile: short write\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
	}
	close(fd);
	return rtn;
}

/*
 * Read entire file.
 */
int readFileSizet(
	const char		*fileName,
	unsigned char	**bytes,		// mallocd and returned
	size_t          *numBytes)		// returned
{
	int rtn;
	int fd;
	char *buf;
	struct stat	sb;
	size_t size;
    ssize_t rrc;

	*numBytes = 0;
	*bytes = NULL;
	fd = open(fileName, O_RDONLY);
    if(fd == -1) {
		return errno;
	}
	rtn = fstat(fd, &sb);
	if(rtn) {
		goto errOut;
	}
	if (sb.st_size > (off_t) ((UINT32_MAX >> 1)-1)) {
		rtn = EFBIG;
		goto errOut;
	}
	size = (size_t)sb.st_size;
	buf = (char *)malloc(size);
	if(buf == NULL) {
		rtn = ENOMEM;
		goto errOut;
	}
	rrc = read(fd, buf, size);
	if(rrc != (ssize_t) size) {
		if(rtn >= 0) {
            free(buf);
			fprintf(stderr, "readFile: short read\n");
		}
		rtn = EIO;
	}
	else {
		rtn = 0;
		*bytes = (unsigned char *)buf;
		*numBytes = size;
	}

errOut:
	close(fd);
	return rtn;
}
