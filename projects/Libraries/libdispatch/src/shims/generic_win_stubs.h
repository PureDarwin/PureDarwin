
#ifndef __DISPATCH__STUBS__INTERNAL
#define __DISPATCH__STUBS__INTERNAL

#include <stdint.h>

#include <winsock2.h>
#include <Windows.h>
#include <crtdbg.h>
#include <ntstatus.h>
#include <Shlwapi.h>
#include <winternl.h>

#include <io.h>
#include <process.h>

/*
 * Stub out defines for missing types
 */

typedef __typeof__(_Generic((__SIZE_TYPE__)0,                                  \
			    unsigned long long int : (long long int)0,         \
			    unsigned long int : (long int)0,                   \
			    unsigned int : (int)0,                             \
			    unsigned short : (short)0,                         \
			    unsigned char : (signed char)0)) ssize_t;

#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#define S_ISFIFO(mode) ((mode) & _S_IFIFO)
#define S_ISREG(mode)  ((mode) & _S_IFREG)
#define S_ISSOCK(mode) 0

#define O_NONBLOCK 04000

#define bzero(ptr,len) memset((ptr), 0, (len))

// Report when an unported code path executes.
#define WIN_PORT_ERROR() \
		_RPTF1(_CRT_ASSERT, "WIN_PORT_ERROR in %s", __FUNCTION__)

#define strcasecmp _stricmp

bool _dispatch_handle_is_socket(HANDLE hFile);

/*
 * Wrappers for dynamically loaded Windows APIs
 */

void _dispatch_QueryInterruptTimePrecise(PULONGLONG lpInterruptTimePrecise);
void _dispatch_QueryUnbiasedInterruptTimePrecise(PULONGLONG lpUnbiasedInterruptTimePrecise);

enum {
	FilePipeLocalInformation = 24,
};

typedef struct _FILE_PIPE_LOCAL_INFORMATION {
	ULONG NamedPipeType;
	ULONG NamedPipeConfiguration;
	ULONG MaximumInstances;
	ULONG CurrentInstances;
	ULONG InboundQuota;
	ULONG ReadDataAvailable;
	ULONG OutboundQuota;
	ULONG WriteQuotaAvailable;
	ULONG NamedPipeState;
	ULONG NamedPipeEnd;
} FILE_PIPE_LOCAL_INFORMATION, *PFILE_PIPE_LOCAL_INFORMATION;

NTSTATUS _dispatch_NtQueryInformationFile(HANDLE FileHandle,
		PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length,
		FILE_INFORMATION_CLASS FileInformationClass);

#endif
