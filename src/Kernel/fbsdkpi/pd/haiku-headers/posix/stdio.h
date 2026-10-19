/*
 * Copyright 2004-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _STDIO_H_
#define _STDIO_H_


#include <sys/types.h>
#include <stdarg.h>


/* Dodge gcc 2.95.3's fixincludes hack stdio_va_list by including this string:
 * __gnuc_va_list */


#define BUFSIZ			8192
#define _IOFBF			0		/* fully buffered */
#define _IOLBF			1		/* line buffered */
#define _IONBF			2		/* not buffered */

/*
 * FOPEN_MAX is a minimum maximum, and should be the number of descriptors
 * that the kernel can provide without allocation of a resource that can
 * fail without the process sleeping.  Do not use this for anything
 */
#define FOPEN_MAX		128
#define STREAM_MAX		FOPEN_MAX
#define FILENAME_MAX	256
#define TMP_MAX			32768

#define L_ctermid  		32
#define L_cuserid  		32
#define	L_tmpnam		512

#define	P_tmpdir		"/tmp/"

#ifdef EOF
#	undef EOF
#endif
#define EOF (-1)

#ifndef SEEK_SET
#	define SEEK_SET 0
#endif
#ifndef SEEK_CUR
#	define SEEK_CUR 1
#endif
#ifndef SEEK_END
#	define SEEK_END 2
#endif


typedef off_t fpos_t;
typedef struct _IO_FILE FILE;


extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;


#ifdef __cplusplus
extern "C" {
#endif

/* file operations */
extern FILE		*fopen(const char *name, const char *mode);
extern FILE		*freopen(const char *name, const char *mode, FILE *stream);
extern FILE		*fdopen(int fd, const char *mode);
extern int		fclose(FILE *stream);
#ifdef _DEFAULT_SOURCE
extern int		fcloseall(void);
#endif

extern int		fileno(FILE *stream);
extern int		fileno_unlocked(FILE *stream);

extern int		ferror(FILE *stream);
extern int		ferror_unlocked(FILE *stream);
extern void		clearerr(FILE *stream);
extern void		clearerr_unlocked(FILE *stream);

extern int		feof(FILE *stream);
extern int		feof_unlocked(FILE *stream);

extern void		flockfile(FILE *stream);
extern void		funlockfile(FILE *stream);
extern int		ftrylockfile(FILE *stream);

extern int		remove(const char *name);
extern int		rename(const char *from, const char *to);
extern int		renameat(int fromFD, const char *from, int toFD, const char *to);

/* pipes */
extern FILE		*popen(const char *command, const char *mode);
extern int		pclose(FILE *stream);
extern void		perror(const char *errorPrefix);

/* memory streams */
extern FILE		*fmemopen(void *buf, size_t size, const char *mode);
extern FILE		*open_memstream(char **buf, size_t *size);

/* callback streams */
#ifdef _DEFAULT_SOURCE
typedef ssize_t (*cookie_read_function_t)(void *cookie, char *buf, size_t size);
typedef ssize_t (*cookie_write_function_t)(void *cookie, const char *buf, size_t size);
typedef ssize_t (*cookie_seek_function_t)(void *cookie, off_t *offset, int whence);
typedef ssize_t (*cookie_close_function_t)(void *cookie);
typedef struct {
	cookie_read_function_t  *read;
	cookie_write_function_t *write;
	cookie_seek_function_t  *seek;
	cookie_close_function_t *close;
} cookie_io_functions_t;
extern FILE		*fopencookie(void *cookie, const char *mode, cookie_io_functions_t io_funcs);
#endif /* _DEFAULT_SOURCE */

/* file I/O */
extern int		fflush(FILE *stream);
extern int		fflush_unlocked(FILE *stream);

extern int		fgetpos(FILE *stream, fpos_t *position);
extern int		fsetpos(FILE *stream, const fpos_t *position);
extern int		fseek(FILE *stream, long offset, int seekType);
extern int		fseeko(FILE *stream, off_t offset, int seekType);
extern int		_fseek(FILE *stream, fpos_t offset, int seekType);
extern long		ftell(FILE *stream);
extern off_t	ftello(FILE *stream);
extern fpos_t	_ftell(FILE *stream);

extern void		rewind(FILE *stream);

extern void		setbuf (FILE *file, char *buff);
extern int		setvbuf(FILE *file, char *buff, int mode, size_t size);
extern int		setbuffer(FILE *stream, char *buf, size_t size);
extern int 	    	setlinebuf(FILE *stream);

extern size_t	fwrite(const void *buffer, size_t size, size_t numItems, FILE *stream);
extern size_t	fwrite_unlocked(const void *buffer, size_t size, size_t numItems, FILE *stream);
extern size_t	fread(void *buffer, size_t size, size_t numItems, FILE *stream);
extern size_t	fread_unlocked(void *buffer, size_t size, size_t numItems, FILE *stream);

extern int		putc(int c, FILE *stream);
extern int		putchar(int c);
extern int		putc_unlocked(int c, FILE *stream);
extern int		putchar_unlocked(int c);
extern int		fputc(int c, FILE *stream);
extern int		fputc_unlocked(int c, FILE *stream);
extern int		puts(const char *string);
extern int		fputs(const char *string, FILE *stream);
extern int		fputs_unlocked(const char *string, FILE *stream);

extern int		getc(FILE *stream);
extern int		getc_unlocked(FILE *stream);
extern int		ungetc(int c, FILE *stream);
extern int		getchar(void);
extern int		getchar_unlocked(void);
extern int		fgetc(FILE *stream);
extern int		fgetc_unlocked(FILE *stream);
extern char		*gets(char *buffer);
extern char		*fgets(char *string, int stringLength, FILE *stream);
extern char		*fgets_unlocked(char *string, int stringLength, FILE *stream);

extern ssize_t	getdelim(char **_line, size_t *_length, int delimiter,
					FILE *stream);
extern ssize_t	getline(char **_line, size_t *_length, FILE *stream);

/* formatted I/O */
extern int		asprintf(char **ret, char const *format, ...) _PRINTFLIKE(2,3);
extern int		printf(char const *format, ...) _PRINTFLIKE(1,2);
#if !defined(_KERNEL_MODE) && !defined(_BOOT_MODE) && !defined(_LOADER_MODE)
extern int		dprintf(int fd, char const *format, ...) _PRINTFLIKE(2,3);
#endif
extern int		fprintf(FILE *stream, char const *format, ...) _PRINTFLIKE(2,3);
extern int		sprintf(char *string, char const *format, ...) _PRINTFLIKE(2,3);
extern int		snprintf(char *string, size_t size, char const *format, ...) _PRINTFLIKE(3,4);
#ifndef PD_LINUXKPI
extern int		vasprintf(char **ret, char const *format, va_list ap);
#endif
extern int		vprintf(char const *format, va_list ap);
extern int		vfprintf(FILE *stream, char const *format, va_list ap);
extern int		vsprintf(char *string, char const *format, va_list ap);
extern int		vsnprintf(char *string, size_t size, char const *format, va_list ap);

extern int		scanf(char const *format, ...);
extern int		fscanf(FILE *stream, char const *format, ...);
extern int		sscanf(char const *str, char const *format, ...);
extern int		vscanf(char const *format, va_list ap);
extern int		vsscanf(char const *str, char const *format, va_list ap);
extern int		vfscanf(FILE *stream, char const *format, va_list ap);

/* misc */
extern char		*ctermid(char *controllingTerminal);
extern char		*cuserid(char *s);

/* temporary files */
extern char		*tempnam(char const *path, char const *prefix);
extern FILE		*tmpfile(void);
extern char 	*tmpnam(char *nameBuffer);
extern char 	*tmpnam_r(char *nameBuffer);

#include <stdio_post.h>

#ifdef __cplusplus
}
#endif


#endif	/* _STDIO_H_ */
