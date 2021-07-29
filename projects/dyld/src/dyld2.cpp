/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2004-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <pthread.h>
#include <libproc.h>
#include <sys/param.h>
#include <mach/mach_time.h> // mach_absolute_time()
#include <mach/mach_init.h>
#include <mach/mach_traps.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/xattr.h>
#include <mach/mach.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h> 
#include <mach-o/ldsyms.h> 
#include <libkern/OSByteOrder.h> 
#include <libkern/OSAtomic.h>
#include <sys/sysctl.h>
#include <sys/mman.h>
#include <sys/dtrace.h>
#include <libkern/OSAtomic.h>
#include <Availability.h>
#include <System/sys/codesign.h>
#include <System/sys/csr.h>
#include <_simple.h>
#include <os/lock_private.h>
#include <System/machine/cpu_capabilities.h>
#include <System/sys/reason.h>
#include <kern/kcdata.h>
#include <sys/attr.h>
#include <sys/fsgetpath.h>
#include <System/sys/content_protection.h>

#define SUPPORT_LOGGING_TO_CONSOLE !TARGET_OS_SIMULATOR
#if SUPPORT_LOGGING_TO_CONSOLE
#include <paths.h> // for logging to console
#endif

#if !TARGET_OS_SIMULATOR

// The comm page is being renamed, so set our define to the new value if the old
// value is missing
#ifndef _COMM_PAGE_DYLD_SYSTEM_FLAGS

#ifndef _COMM_PAGE_DYLD_FLAGS
#error Must define _COMM_PAGE_DYLD_FLAGS or _COMM_PAGE_DYLD_SYSTEM_FLAGS
#endif

#define _COMM_PAGE_DYLD_SYSTEM_FLAGS _COMM_PAGE_DYLD_FLAGS

#endif

#endif

#if TARGET_OS_SIMULATOR
	enum {
		AMFI_DYLD_INPUT_PROC_IN_SIMULATOR = (1 << 0),
	};
	enum amfi_dyld_policy_output_flag_set {
		AMFI_DYLD_OUTPUT_ALLOW_AT_PATH = (1 << 0),
		AMFI_DYLD_OUTPUT_ALLOW_PATH_VARS = (1 << 1),
		AMFI_DYLD_OUTPUT_ALLOW_CUSTOM_SHARED_CACHE = (1 << 2),
		AMFI_DYLD_OUTPUT_ALLOW_FALLBACK_PATHS = (1 << 3),
		AMFI_DYLD_OUTPUT_ALLOW_PRINT_VARS = (1 << 4),
		AMFI_DYLD_OUTPUT_ALLOW_FAILED_LIBRARY_INSERTION = (1 << 5),
		AMFI_DYLD_OUTPUT_ALLOW_LIBRARY_INTERPOSING = (1 << 6),
	};
	extern "C" int amfi_check_dyld_policy_self(uint64_t input_flags, uint64_t* output_flags);
#else
	#include <libamfi.h>
#endif

#include <sandbox.h>
#include <sandbox/private.h>
#if __has_feature(ptrauth_calls)
	#include <ptrauth.h>
#endif

extern "C" int __fork();

#include <array>
#include <algorithm>
#include <vector>


#include "dyld2.h"
#include "ImageLoader.h"
#include "ImageLoaderMachO.h"
#include "dyldLibSystemInterface.h"
#include "dyld_cache_format.h"
#include "dyld_process_info_internal.h"

#if SUPPORT_ACCELERATE_TABLES
	#include "ImageLoaderMegaDylib.h"
#endif

#if TARGET_OS_SIMULATOR
	extern "C" void* gSyscallHelpers;
#else
	#include "dyldSyscallInterface.h"
#endif

#include "Closure.h"
#include "libdyldEntryVector.h"
#include "MachOLoaded.h"
#include "Loading.h"
#include "DyldSharedCache.h"
#include "SharedCacheRuntime.h"
#include "StringUtils.h"
#include "Tracing.h"
#include "ClosureBuilder.h"
#include "ClosureFileSystemPhysical.h"
#include "FileUtils.h"
#include "BootArgs.h"
#include "Defines.h"
#include "RootsChecker.h"

#ifndef MH_HAS_OBJC
  #define MH_HAS_OBJC			0x40000000
#endif

// not libc header for send() syscall interface
extern "C" ssize_t __sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);


// ARM and x86_64 are the only architecture that use cpu-sub-types
#define CPU_SUBTYPES_SUPPORTED  ((__arm__ || __arm64__ || __x86_64__) && !TARGET_OS_SIMULATOR)

#if __LP64__
	#define LC_SEGMENT_COMMAND		LC_SEGMENT_64
	#define LC_SEGMENT_COMMAND_WRONG LC_SEGMENT
	#define LC_ENCRYPT_COMMAND		LC_ENCRYPTION_INFO
	#define macho_segment_command	segment_command_64
	#define macho_section			section_64
#else
	#define LC_SEGMENT_COMMAND		LC_SEGMENT
	#define LC_SEGMENT_COMMAND_WRONG LC_SEGMENT_64
	#define LC_ENCRYPT_COMMAND		LC_ENCRYPTION_INFO_64
	#define macho_segment_command	segment_command
	#define macho_section			section
#endif

#define DYLD_CLOSURE_XATTR_NAME "com.apple.dyld"

#define CPU_TYPE_MASK 0x00FFFFFF	/* complement of CPU_ARCH_MASK */


/* implemented in dyld_gdb.cpp */
extern void resetAllImages();
extern void addImagesToAllImages(uint32_t infoCount, const dyld_image_info info[]);
extern void addAotImagesToAllAotImages(uint32_t aotInfoCount, const dyld_aot_image_info aotInfo[]);
extern void removeImageFromAllImages(const mach_header* mh);
extern void addNonSharedCacheImageUUID(const dyld_uuid_info& info);
extern const char* notifyGDB(enum dyld_image_states state, uint32_t infoCount, const dyld_image_info info[]);
extern size_t allImagesCount();

// magic so CrashReporter logs message
extern "C" {
	char error_string[1024];
}

// magic linker symbol for start of dyld binary
extern "C" const macho_header __dso_handle;

extern bool gEnableSharedCacheDataConst;


//
// The file contains the core of dyld used to get a process to main().  
// The API's that dyld supports are implemented in dyldAPIs.cpp.
//
//
//
//
//
namespace dyld {
	struct RegisteredDOF { const mach_header* mh; int registrationID; };
	struct DylibOverride { const char* installName; const char* override; };
}


VECTOR_NEVER_DESTRUCTED(ImageLoader*);
VECTOR_NEVER_DESTRUCTED(dyld::RegisteredDOF);
VECTOR_NEVER_DESTRUCTED(dyld::ImageCallback);
VECTOR_NEVER_DESTRUCTED(dyld::DylibOverride);
VECTOR_NEVER_DESTRUCTED(ImageLoader::DynamicReference);

VECTOR_NEVER_DESTRUCTED(dyld_image_state_change_handler);

namespace dyld {


// 
// state of all environment variables dyld uses
//
struct EnvironmentVariables {
	const char* const *			DYLD_FRAMEWORK_PATH;
	const char* const *			DYLD_FALLBACK_FRAMEWORK_PATH;
	const char* const *			DYLD_LIBRARY_PATH;
	const char* const *			DYLD_FALLBACK_LIBRARY_PATH;
	const char* const *			DYLD_INSERT_LIBRARIES;
	const char* const *			LD_LIBRARY_PATH;			// for unix conformance
	const char* const *			DYLD_VERSIONED_LIBRARY_PATH;
	const char* const *			DYLD_VERSIONED_FRAMEWORK_PATH;
	bool						DYLD_PRINT_LIBRARIES_POST_LAUNCH;
	bool						DYLD_BIND_AT_LAUNCH;
	bool						DYLD_PRINT_STATISTICS;
	bool						DYLD_PRINT_STATISTICS_DETAILS;
	bool						DYLD_PRINT_OPTS;
	bool						DYLD_PRINT_ENV;
	bool						DYLD_DISABLE_DOFS;
	bool						hasOverride;
                            //  DYLD_SHARED_CACHE_DIR           ==> sSharedCacheOverrideDir
							//	DYLD_ROOT_PATH					==> gLinkContext.rootPaths
							//	DYLD_IMAGE_SUFFIX				==> gLinkContext.imageSuffix
							//	DYLD_PRINT_OPTS					==> gLinkContext.verboseOpts
							//	DYLD_PRINT_ENV					==> gLinkContext.verboseEnv
							//	DYLD_FORCE_FLAT_NAMESPACE		==> gLinkContext.bindFlat
							//	DYLD_PRINT_INITIALIZERS			==> gLinkContext.verboseInit
							//	DYLD_PRINT_SEGMENTS				==> gLinkContext.verboseMapping
							//	DYLD_PRINT_BINDINGS				==> gLinkContext.verboseBind
							//  DYLD_PRINT_WEAK_BINDINGS		==> gLinkContext.verboseWeakBind
							//	DYLD_PRINT_REBASINGS			==> gLinkContext.verboseRebase
							//	DYLD_PRINT_DOFS					==> gLinkContext.verboseDOF
							//	DYLD_PRINT_APIS					==> gLogAPIs
							//	DYLD_IGNORE_PREBINDING			==> gLinkContext.prebindUsage
							//	DYLD_PREBIND_DEBUG				==> gLinkContext.verbosePrebinding
							//	DYLD_NEW_LOCAL_SHARED_REGIONS	==> gLinkContext.sharedRegionMode
							//	DYLD_SHARED_REGION				==> gLinkContext.sharedRegionMode
							//	DYLD_PRINT_WARNINGS				==> gLinkContext.verboseWarnings
							//	DYLD_PRINT_RPATHS				==> gLinkContext.verboseRPaths
							//	DYLD_PRINT_INTERPOSING			==> gLinkContext.verboseInterposing
							//  DYLD_PRINT_LIBRARIES			==> gLinkContext.verboseLoading
};



typedef std::vector<dyld_image_state_change_handler> StateHandlers;


enum EnvVarMode { envNone, envPrintOnly, envAll };
	
// all global state
static const char*					sExecPath = NULL;
static const char*					sExecShortName = NULL;
static const macho_header*			sMainExecutableMachHeader = NULL;
static uintptr_t					sMainExecutableSlide = 0;
#if CPU_SUBTYPES_SUPPORTED
static cpu_type_t					sHostCPU;
static cpu_subtype_t				sHostCPUsubtype;
#endif
typedef ImageLoaderMachO* __ptrauth_dyld_address_auth MainExecutablePointerType;
static MainExecutablePointerType	sMainExecutable = NULL;
static size_t						sInsertedDylibCount = 0;
static std::vector<ImageLoader*>	sAllImages;
static std::vector<ImageLoader*>	sImageRoots;
static std::vector<ImageLoader*>	sImageFilesNeedingTermination;
static std::vector<RegisteredDOF>	sImageFilesNeedingDOFUnregistration;
static std::vector<ImageCallback>   sAddImageCallbacks;
static std::vector<ImageCallback>   sRemoveImageCallbacks;
static std::vector<LoadImageCallback> sAddLoadImageCallbacks;
static std::vector<LoadImageBulkCallback> sAddBulkLoadImageCallbacks;
static bool							sRemoveImageCallbacksInUse = false;
static void*						sSingleHandlers[7][3];
static void*						sBatchHandlers[7][3];
static ImageLoader*					sLastImageByAddressCache;
static EnvironmentVariables			sEnv;
#if TARGET_OS_OSX
static const char*					sFrameworkFallbackPaths[] = { "$HOME/Library/Frameworks", "/Library/Frameworks", "/Network/Library/Frameworks", "/System/Library/Frameworks", NULL };
static const char*					sLibraryFallbackPaths[] = { "$HOME/lib", "/usr/local/lib", "/usr/lib", NULL };
static const char*					sRestrictedFrameworkFallbackPaths[] = { "/System/Library/Frameworks", NULL };
static const char*					sRestrictedLibraryFallbackPaths[] = { "/usr/lib", NULL };
#else
static const char*					sFrameworkFallbackPaths[] = { "/System/Library/Frameworks", NULL };
static const char*					sLibraryFallbackPaths[] = { "/usr/local/lib", "/usr/lib", NULL };
#endif
static UndefinedHandler				sUndefinedHandler = NULL;
static ImageLoader*					sBundleBeingLoaded = NULL;	// hack until OFI is reworked
static dyld3::SharedCacheLoadInfo	sSharedCacheLoadInfo;
static const char*					sSharedCacheOverrideDir;
       bool							gSharedCacheOverridden = false;
ImageLoader::LinkContext			gLinkContext;
bool								gLogAPIs = false;
#if SUPPORT_ACCELERATE_TABLES
bool								gLogAppAPIs = false;
#endif
const struct LibSystemHelpers*		gLibSystemHelpers = NULL;
#if SUPPORT_OLD_CRT_INITIALIZATION
bool								gRunInitializersOldWay = false;
#endif
static std::vector<DylibOverride>	sDylibOverrides;
#if !TARGET_OS_SIMULATOR	
static int							sLogSocket = -1;
#endif
static bool							sFrameworksFoundAsDylibs = false;
#if __x86_64__ && !TARGET_OS_SIMULATOR
static bool							sHaswell = false;
#endif
static std::vector<ImageLoader::DynamicReference> sDynamicReferences;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
static OSSpinLock					sDynamicReferencesLock = 0;
#pragma clang diagnostic pop
#if !TARGET_OS_SIMULATOR
static bool							sLogToFile = false;
#endif
static char							sLoadingCrashMessage[1024] = "dyld: launch, loading dependent libraries";
static _dyld_objc_notify_mapped		sNotifyObjCMapped;
static _dyld_objc_notify_init		sNotifyObjCInit;
static _dyld_objc_notify_unmapped	sNotifyObjCUnmapped;

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static bool							sForceStderr = false;
#endif


#if SUPPORT_ACCELERATE_TABLES
static ImageLoaderMegaDylib*		sAllCacheImagesProxy = NULL;
// Note these are now off by default as everything should use dyld3.
static bool							sDisableAcceleratorTables = true;
#endif

bool								gUseDyld3 = false;
static uint32_t						sLaunchModeUsed = 0;
static bool							sSkipMain = false;
static void                       (*sEntryOverride)() = nullptr;
static bool							sJustBuildClosure = false;
#if !TARGET_OS_SIMULATOR
static bool							sLogClosureFailure = false;
#endif
static bool 						sKeysDisabled = false;
static bool							sOnlyPlatformArm64e = false; // arm64e binaries can only be loaded if they are part of the OS

static dyld3::RootsChecker			sRootsChecker;

enum class ClosureMode {
	// Unset means we haven't provided an env variable or boot-arg to explicitly choose a mode
	Unset,
	// On means we set DYLD_USE_CLOSURES=1, or we didn't have DYLD_USE_CLOSURES=0 but did have
	// -force_dyld3=1 env variable or a customer cache on iOS
	On,
	// Off means we set DYLD_USE_CLOSURES=0, or we didn't have DYLD_USE_CLOSURES=1 but did have
	// -force_dyld2=1 env variable or an internal cache on iOS
	Off,
	// PreBuiltOnly means only use a shared cache closure and don't try build a new one
	PreBuiltOnly,
};

enum class ClosureKind {
	unset,
	full,
	minimal,
};

static ClosureMode					sClosureMode = ClosureMode::Unset;
static ClosureKind					sClosureKind = ClosureKind::unset;
static bool							sForceInvalidSharedCacheClosureFormat = false;
static uint64_t						launchTraceID = 0;

// These flags are the values in the 64-bit _COMM_PAGE_DYLD_SYSTEM_FLAGS entry
// Note we own this and can write it from PID 1
enum CommPageFlags : uint64_t {
    None                                = 0,

	// The boot args can set the low 32-bits of the comm page.  We'll reserve the high 32-bits
	// for runtime (launchd) set values.
	CommPageBootArgMask					= 0xFFFFFFFF,

	// Are the simulator support dylibs definitely roots when launchd scanned them
	libsystemKernelIsRoot 				= 1ULL << 32,
	libsystemPlatformIsRoot				= 1ULL << 33,
	libsystemPThreadIsRoot 				= 1ULL << 34,

	// Is the file system writable, ie, could the simulator support dylibs be written
	// later, after PID 1
	fileSystemCanBeModified				= 1ULL << 35
};

//
// The MappedRanges structure is used for fast address->image lookups.
// The table is only updated when the dyld lock is held, so we don't
// need to worry about multiple writers.  But readers may look at this
// data without holding the lock. Therefore, all updates must be done
// in an order that will never cause readers to see inconsistent data.
// The general rule is that if the image field is non-NULL then
// the other fields are valid.
//
struct MappedRanges
{
	MappedRanges*		next;
	unsigned long		count;
	struct {
		ImageLoader*	image;
		uintptr_t		start;
		uintptr_t		end;
	} array[1];
};

static MappedRanges*	sMappedRangesStart;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
void addMappedRange(ImageLoader* image, uintptr_t start, uintptr_t end)
{
	//dyld::log("addMappedRange(0x%lX->0x%lX) for %s\n", start, end, image->getShortName());
	for (MappedRanges* p = sMappedRangesStart; p != NULL; p = p->next) {
		for (unsigned long i=0; i < p->count; ++i) {
			if ( p->array[i].image == NULL ) {
				p->array[i].start = start;
				p->array[i].end = end;
				// add image field last with a barrier so that any reader will see consistent records
				OSMemoryBarrier();
				p->array[i].image = image;
				return;
			}
		}
	}
	// table must be full, chain another
#if SUPPORT_ACCELERATE_TABLES
	unsigned count = (sAllCacheImagesProxy != NULL) ? 16 : 400;
#else
	unsigned count = 400;
#endif
	size_t allocationSize = sizeof(MappedRanges) + (count-1)*3*sizeof(void*);
	MappedRanges* newRanges = (MappedRanges*)malloc(allocationSize);
	bzero(newRanges, allocationSize);
	newRanges->count = count;
	newRanges->array[0].start = start;
	newRanges->array[0].end = end;
	newRanges->array[0].image = image;
	OSMemoryBarrier();
	if ( sMappedRangesStart == NULL ) {
		sMappedRangesStart = newRanges;
	}
	else {
		for (MappedRanges* p = sMappedRangesStart; p != NULL; p = p->next) {
			if ( p->next == NULL ) {
				OSMemoryBarrier();
				p->next = newRanges;
				break;
			}
		}
	}
}

void removedMappedRanges(ImageLoader* image)
{
	for (MappedRanges* p = sMappedRangesStart; p != NULL; p = p->next) {
		for (unsigned long i=0; i < p->count; ++i) {
			if ( p->array[i].image == image ) {
				// clear with a barrier so that any reader will see consistent records
				OSMemoryBarrier();
				p->array[i].image = NULL;
			}
		}
	}
}
#pragma clang diagnostic pop

ImageLoader* findMappedRange(uintptr_t target)
{
	for (MappedRanges* p = sMappedRangesStart; p != NULL; p = p->next) {
		for (unsigned long i=0; i < p->count; ++i) {
			if ( p->array[i].image != NULL ) {
				if ( (p->array[i].start <= target) && (target < p->array[i].end) )
					return p->array[i].image;
			}
		}
	}
	return NULL;
}



const char* mkstringf(const char* format, ...)
{
	_SIMPLE_STRING buf = _simple_salloc();
	if ( buf != NULL ) {
		va_list	list;
		va_start(list, format);
		_simple_vsprintf(buf, format, list);
		va_end(list);
		const char*	t = strdup(_simple_string(buf));
		_simple_sfree(buf);
		if ( t != NULL )
			return t;
	}
	return "mkstringf, out of memory error";
}


void throwf(const char* format, ...) 
{
	_SIMPLE_STRING buf = _simple_salloc();
	if ( buf != NULL ) {
		va_list	list;
		va_start(list, format);
		_simple_vsprintf(buf, format, list);
		va_end(list);
		const char*	t = strdup(_simple_string(buf));
		_simple_sfree(buf);
		if ( t != NULL )
			throw t;
	}
	throw "throwf, out of memory error";
}


#if !TARGET_OS_SIMULATOR
static int sLogfile = STDERR_FILENO;
#endif

#if !TARGET_OS_SIMULATOR	
// based on CFUtilities.c: also_do_stderr()
static bool useSyslog()
{
	// Use syslog() for processes managed by launchd
	static bool launchdChecked = false;
	static bool launchdOwned = false;
	if ( !launchdChecked && gProcessInfo->libSystemInitialized ) {
		if ( (gLibSystemHelpers != NULL) && (gLibSystemHelpers->version >= 11) ) {
			// <rdar://problem/23520449> only call isLaunchdOwned() after libSystem is initialized
			launchdOwned = (*gLibSystemHelpers->isLaunchdOwned)();
			launchdChecked = true;
		}
	}
	if ( launchdChecked && launchdOwned )
		return true;

	// If stderr is not available, use syslog()
	struct stat sb;
	int result = fstat(STDERR_FILENO, &sb);
	if ( result < 0 )
		return true; // file descriptor 2 is closed

	return false;
}

	
static void socket_syslogv(int priority, const char* format, va_list list)
{
	// lazily create socket and connection to syslogd
	if ( sLogSocket == -1 ) {
		sLogSocket = ::socket(AF_UNIX, SOCK_DGRAM, 0);
		if (sLogSocket == -1)
			return;  // cannot log
		::fcntl(sLogSocket, F_SETFD, 1);
	
		struct sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		strncpy(addr.sun_path, _PATH_LOG, sizeof(addr.sun_path));
		if ( ::connect(sLogSocket, (struct sockaddr *)&addr, sizeof(addr)) == -1 ) {
			::close(sLogSocket);
			sLogSocket = -1;
			return;
		}
	}
	
	// format message to syslogd like: "<priority>Process[pid]: message"
	_SIMPLE_STRING buf = _simple_salloc();
	if ( buf == NULL )
		return;
	if ( _simple_sprintf(buf, "<%d>%s[%d]: ", LOG_USER|LOG_NOTICE, sExecShortName, getpid()) == 0 ) {
		if ( _simple_vsprintf(buf, format, list) == 0 ) {
			const char* p = _simple_string(buf);
			::__sendto(sLogSocket, p, strlen(p), 0, NULL, 0);
		}
	}
	_simple_sfree(buf);
}



void vlog(const char* format, va_list list)
{
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
	// <rdar://problem/25965832> log to console when running iOS app from Xcode
	if ( !sLogToFile && !sForceStderr && useSyslog() )
#else
	if ( !sLogToFile && useSyslog() )
#endif
		socket_syslogv(LOG_ERR, format, list);
	else {
		_simple_vdprintf(sLogfile, format, list);
	}
}

void log(const char* format, ...)
{
	va_list	list;
	va_start(list, format);
	vlog(format, list);
	va_end(list);
}


void vwarn(const char* format, va_list list) 
{
	_simple_dprintf(sLogfile, "dyld: warning, ");
	_simple_vdprintf(sLogfile, format, list);
}

void warn(const char* format, ...) 
{
	va_list	list;
	va_start(list, format);
	vwarn(format, list);
	va_end(list);
}

void logToConsole(const char* format, ...) {
#if SUPPORT_LOGGING_TO_CONSOLE
    int cfd = open(_PATH_CONSOLE, O_WRONLY|O_NOCTTY);
    if (cfd == -1)
        return;

    va_list list;
    va_start(list, format);
    _simple_vdprintf(cfd, format, list);
    va_end(list);

    close(cfd);
#endif
}

#else
	extern void vlog(const char* format, va_list list);
#endif // !TARGET_OS_SIMULATOR	


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
// <rdar://problem/8867781> control access to sAllImages through a lock
// because global dyld lock is not held during initialization phase of dlopen()
// <rdar://problem/16145518> Use OSSpinLockLock to allow yielding
static OSSpinLock sAllImagesLock = 0;

static void allImagesLock()
{
	OSSpinLockLock(&sAllImagesLock);
}

static void allImagesUnlock()
{
	OSSpinLockUnlock(&sAllImagesLock);
}
#pragma clang diagnostic pop


// utility class to assure files are closed when an exception is thrown
class FileOpener {
public:
	FileOpener(const char* path);
	~FileOpener();
	int getFileDescriptor() { return fd; }
private:
	int fd;
};

FileOpener::FileOpener(const char* path)
 : fd(-1)
{
	fd = dyld3::open(path, O_RDONLY, 0);
}

FileOpener::~FileOpener()
{
	if ( fd != -1 )
		close(fd);
}


static void	registerDOFs(const std::vector<ImageLoader::DOFInfo>& dofs)
{
	const size_t dofSectionCount = dofs.size();
	if ( !sEnv.DYLD_DISABLE_DOFS && (dofSectionCount != 0) ) {
		int fd = open("/dev/" DTRACEMNR_HELPER, O_RDWR);
		if ( fd < 0 ) {
			//dyld::warn("can't open /dev/" DTRACEMNR_HELPER " to register dtrace DOF sections\n");
		}
		else {
			// allocate a buffer on the stack for the variable length dof_ioctl_data_t type
			uint8_t buffer[sizeof(dof_ioctl_data_t) + dofSectionCount*sizeof(dof_helper_t)];
			dof_ioctl_data_t* ioctlData = (dof_ioctl_data_t*)buffer;
			
			// fill in buffer with one dof_helper_t per DOF section
			ioctlData->dofiod_count = dofSectionCount;
			for (unsigned int i=0; i < dofSectionCount; ++i) {
				strlcpy(ioctlData->dofiod_helpers[i].dofhp_mod, dofs[i].imageShortName, DTRACE_MODNAMELEN);
				ioctlData->dofiod_helpers[i].dofhp_dof = (uintptr_t)(dofs[i].dof);
				ioctlData->dofiod_helpers[i].dofhp_addr = (uintptr_t)(dofs[i].dof);
			}
			
			// tell kernel about all DOF sections en mas
			// pass pointer to ioctlData because ioctl() only copies a fixed size amount of data into kernel
			user_addr_t val = (user_addr_t)(unsigned long)ioctlData;
			if ( ioctl(fd, DTRACEHIOC_ADDDOF, &val) != -1 ) {
				// kernel returns a unique identifier for each section in the dofiod_helpers[].dofhp_dof field.
				for (unsigned int i=0; i < dofSectionCount; ++i) {
					RegisteredDOF info;
					info.mh = dofs[i].imageHeader;
					info.registrationID = (int)(ioctlData->dofiod_helpers[i].dofhp_dof);
					sImageFilesNeedingDOFUnregistration.push_back(info);
					if ( gLinkContext.verboseDOF ) {
						dyld::log("dyld: registering DOF section %p in %s with dtrace, ID=0x%08X\n", 
							dofs[i].dof, dofs[i].imageShortName, info.registrationID);
					}
				}
			}
			else {
				//dyld::log( "dyld: ioctl to register dtrace DOF section failed\n");
			}
			close(fd);
		}
	}
}

static void	unregisterDOF(int registrationID)
{
	int fd = open("/dev/" DTRACEMNR_HELPER, O_RDWR, 0);
	if ( fd < 0 ) {
		dyld::warn("can't open /dev/" DTRACEMNR_HELPER " to unregister dtrace DOF section\n");
	}
	else {
		ioctl(fd, DTRACEHIOC_REMOVE, registrationID);
		close(fd);
		if ( gLinkContext.verboseInit )
			dyld::warn("unregistering DOF section ID=0x%08X with dtrace\n", registrationID);
	}
}


//
// _dyld_register_func_for_add_image() is implemented as part of the general image state change notification
// Returns true if we did call add image callbacks on this image
//
static bool notifyAddImageCallbacks(ImageLoader* image)
{
	// use guard so that we cannot notify about the same image twice
	if ( ! image->addFuncNotified() ) {
		for (std::vector<ImageCallback>::iterator it=sAddImageCallbacks.begin(); it != sAddImageCallbacks.end(); it++) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)image->machHeader(), (uint64_t)(*it), 0);
			(*it)(image->machHeader(), image->getSlide());
		}
		for (LoadImageCallback func : sAddLoadImageCallbacks) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)image->machHeader(), (uint64_t)(*func), 0);
			(*func)(image->machHeader(), image->getPath(), !image->neverUnload());
		}
		image->setAddFuncNotified();
		return true;
	}
	return false;
}



// notify gdb about these new images
static const char* updateAllImages(enum dyld_image_states state, uint32_t infoCount, const struct dyld_image_info info[])
{
	// <rdar://problem/8812589> don't add images without paths to all-image-info-list
	if ( info[0].imageFilePath != NULL )
		addImagesToAllImages(infoCount, info);
	return NULL;
}


static StateHandlers* stateToHandlers(dyld_image_states state, void* handlersArray[7][3])
{
	switch ( state ) {
		case dyld_image_state_mapped:
			return reinterpret_cast<StateHandlers*>(&handlersArray[0]);
			
		case dyld_image_state_dependents_mapped:
			return reinterpret_cast<StateHandlers*>(&handlersArray[1]);
			
		case dyld_image_state_rebased:
			return reinterpret_cast<StateHandlers*>(&handlersArray[2]);
			
		case dyld_image_state_bound:
			return reinterpret_cast<StateHandlers*>(&handlersArray[3]);
			
		case dyld_image_state_dependents_initialized:
			return reinterpret_cast<StateHandlers*>(&handlersArray[4]);

		case dyld_image_state_initialized:
			return reinterpret_cast<StateHandlers*>(&handlersArray[5]);
			
		case dyld_image_state_terminated:
			return reinterpret_cast<StateHandlers*>(&handlersArray[6]);
	}
	return NULL;
}

#if SUPPORT_ACCELERATE_TABLES
static dyld_image_state_change_handler getPreInitNotifyHandler(unsigned index)
{
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(dyld_image_state_dependents_initialized, sSingleHandlers);
	if ( index >= handlers->size() )
		return NULL;
	return (*handlers)[index];
}

static dyld_image_state_change_handler getBoundBatchHandler(unsigned index)
{
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(dyld_image_state_bound, sBatchHandlers);
	if ( index >= handlers->size() )
		return NULL;
	return (*handlers)[index];
}

static void notifySingleFromCache(dyld_image_states state, const mach_header* mh, const char* path)
{
	//dyld::log("notifySingle(state=%d, image=%s)\n", state, image->getPath());
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(state, sSingleHandlers);
	if ( handlers != NULL ) {
		dyld_image_info info;
		info.imageLoadAddress	= mh;
		info.imageFilePath		= path;
		info.imageFileModDate	= 0;
		for (dyld_image_state_change_handler handler : *handlers) {
			const char* result = (*handler)(state, 1, &info);
			if ( (result != NULL) && (state == dyld_image_state_mapped) ) {
				//fprintf(stderr, "  image rejected by handler=%p\n", *it);
				// make copy of thrown string so that later catch clauses can free it
				const char* str = strdup(result);
				throw str;
			}
		}
	}
	if ( (state == dyld_image_state_dependents_initialized) && (sNotifyObjCInit != NULL) && (mh->flags & MH_HAS_OBJC) ) {
		dyld3::ScopedTimer timer(DBG_DYLD_TIMING_OBJC_INIT, (uint64_t)mh, 0, 0);
		(*sNotifyObjCInit)(path, mh);
	}
}
#endif

#if !TARGET_OS_SIMULATOR
#define DYLD_PROCESS_INFO_NOTIFY_MAGIC 0x49414E46

struct RemoteNotificationResponder {
	RemoteNotificationResponder(const RemoteNotificationResponder&) = delete;
	RemoteNotificationResponder(RemoteNotificationResponder&&) = delete;
	RemoteNotificationResponder() {
		if (dyld::gProcessInfo->notifyPorts[0] != DYLD_PROCESS_INFO_NOTIFY_MAGIC) {
			// No notifier found, early out
			_namesCnt = 0;
			return;
		}
		kern_return_t kr = task_dyld_process_info_notify_get(_names, &_namesCnt);
		while (kr == KERN_NO_SPACE) {
			// In the future the SPI may return the size we need, but for now we just double the count. Since we don't want to depend on the
			// return value in _nameCnt we set it to have a minimm of 16, double the inline storage value
			_namesCnt = std::max<uint32_t>(16, 2*_namesCnt);
			_namesSize = _namesCnt*sizeof(mach_port_t);
			kr = vm_allocate(mach_task_self(), (vm_address_t*)&_names, _namesSize, VM_FLAGS_ANYWHERE);
			if (kr != KERN_SUCCESS) {
				// We could not allocate memory, time to error out
				break;
			}
			kr = task_dyld_process_info_notify_get(_names, &_namesCnt);
			if (kr != KERN_SUCCESS) {
				// We failed, so deallocate the memory. If the failures was KERN_NO_SPACE we will loop back and try again
				(void)vm_deallocate(mach_task_self(), (vm_address_t)_names, _namesSize);
				_namesSize = 0;
			}
		}
		if (kr != KERN_SUCCESS) {
			// We failed, set _namesCnt to 0 so nothing else will happen
			_namesCnt = 0;
		}
	}
	~RemoteNotificationResponder() {
		if (_namesCnt) {
			for (auto i = 0; i < _namesCnt; ++i) {
				(void)mach_port_deallocate(mach_task_self(), _names[i]);
			}
			if (_namesSize != 0) {
				// We are not using inline memory, we need to free it
				(void)vm_deallocate(mach_task_self(), (vm_address_t)_names, _namesSize);
			}
		}
	}
	void sendMessage(mach_msg_id_t msgId, mach_msg_size_t sendSize, mach_msg_header_t* buffer) {
		if (_namesCnt == 0) { return; }
		// Allocate a port to listen on in this monitoring task
		mach_port_t replyPort = MACH_PORT_NULL;
		mach_port_options_t options = { .flags = MPO_CONTEXT_AS_GUARD | MPO_STRICT, .mpl = { 1 }};
		kern_return_t kr = mach_port_construct(mach_task_self(), &options, (mach_port_context_t)&replyPort, &replyPort);
		if (kr != KERN_SUCCESS) {
			return;
		}
		for (auto i = 0; i < _namesCnt; ++i) {
			if (_names[i] == MACH_PORT_NULL) { continue; }
			// Assemble a message
			uint8_t replyBuffer[sizeof(mach_msg_header_t) + MAX_TRAILER_SIZE];
			mach_msg_header_t* 	msg = buffer;
			msg->msgh_bits         = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,MACH_MSG_TYPE_MAKE_SEND_ONCE);
			msg->msgh_id           = msgId;
			msg->msgh_local_port   = replyPort;
			msg->msgh_remote_port  = _names[i];
			msg->msgh_reserved     = 0;
			msg->msgh_size         = sendSize;
			kr = mach_msg_overwrite(msg, MACH_SEND_MSG | MACH_RCV_MSG, msg->msgh_size, sizeof(replyBuffer), replyPort, 0, MACH_PORT_NULL,
									 (mach_msg_header_t*)&replyBuffer[0], 0);
			if (kr != KERN_SUCCESS) {
				// Send failed, we may have been psuedo recieved. destroy the message
				(void)mach_msg_destroy(msg);
				// Mark the port as null. It does not matter why we failed... if it is s single message we will not retry, if it
				// is a fragmented message then subsequent messages will not decode correctly
				_names[i] = MACH_PORT_NULL;
			}
		}
		(void)mach_port_destruct(mach_task_self(), replyPort, 0, (mach_port_context_t)&replyPort);
	}

	bool const active() const {
		for (auto i = 0; i < _namesCnt; ++i) {
			if (_names[i] != MACH_PORT_NULL) {
				return true;
			}
		}
		return false;
	}
private:
	mach_port_t             _namesArray[8] = {0};
	mach_port_name_array_t  _names = (mach_port_name_array_t)&_namesArray[0];
	mach_msg_type_number_t  _namesCnt = 8;
	vm_size_t               _namesSize = 0;
};

//FIXME: Remove this once we drop support for iOS 11 simulators
// This is an enormous hack to keep remote introspection of older simulators working
//   It works by interposing mach_msg, and redirecting message sent to a special port name. Messages to that portname will trigger a full set
//   of sends to all kernel registered notifiers. In this mode mach_msg_sim_interposed() must return KERN_SUCCESS or the older dyld_sim may
//   try to cleanup the notifer array.
kern_return_t mach_msg_sim_interposed(	mach_msg_header_t* msg, mach_msg_option_t option, mach_msg_size_t send_size, mach_msg_size_t rcv_size,
									  mach_port_name_t rcv_name, mach_msg_timeout_t timeout, mach_port_name_t notify) {
	if (msg->msgh_remote_port != DYLD_PROCESS_INFO_NOTIFY_MAGIC) {
		// Not the magic port, so just pass through to the real mach_msg()
		return mach_msg(msg, option, send_size, rcv_size, rcv_name, timeout, notify);
	}

	// The magic port. We know dyld_sim is trying to message observers, so lets call into our messaging code directly.
	// This is kind of weird since we effectively built a buffer in dyld_sim, then pass it to mach_msg, which we interpose, unpack, and then
	// pass to send_message which then sends the buffer back out vis mach_message_overwrite(), but it should work at least as well as the old
	// way.
	RemoteNotificationResponder responder;
	responder.sendMessage(msg->msgh_id, send_size, msg);

	// We always return KERN_SUCCESS, otherwise old dyld_sims might clear the port
	return KERN_SUCCESS;
}

static void notifyMonitoringDyld(RemoteNotificationResponder& responder, bool unloading, unsigned imageCount,
								 const struct mach_header* loadAddresses[], const char* imagePaths[])
{
	// Make sure there is at least enough room to hold a the largest single file entry that can exist.
	static_assert((MAXPATHLEN + sizeof(dyld_process_info_image_entry) + 1 + MAX_TRAILER_SIZE) <= DYLD_PROCESS_INFO_NOTIFY_MAX_BUFFER_SIZE);

	unsigned entriesSize = imageCount*sizeof(dyld_process_info_image_entry);
	unsigned pathsSize = 0;
	for (unsigned j=0; j < imageCount; ++j) {
		pathsSize += (strlen(imagePaths[j]) + 1);
	}

	unsigned totalSize = (sizeof(struct dyld_process_info_notify_header) + entriesSize + pathsSize + 127) & -128;   // align
	// The reciever has a fixed buffer of DYLD_PROCESS_INFO_NOTIFY_MAX_BUFFER_SIZE, whcih needs to hold both the message and a trailer.
	// If the total size exceeds that we need to fragment the message.
	if ( (totalSize + MAX_TRAILER_SIZE) > DYLD_PROCESS_INFO_NOTIFY_MAX_BUFFER_SIZE ) {
		// Putting all image paths into one message would make buffer too big.
		// Instead split into two messages.  Recurse as needed until paths fit in buffer.
		unsigned imageHalfCount = imageCount/2;
		notifyMonitoringDyld(responder, unloading, imageHalfCount, loadAddresses, imagePaths);
		notifyMonitoringDyld(responder, unloading, imageCount - imageHalfCount, &loadAddresses[imageHalfCount], &imagePaths[imageHalfCount]);
		return;
	}
	uint8_t	buffer[totalSize + MAX_TRAILER_SIZE];
	dyld_process_info_notify_header* header = (dyld_process_info_notify_header*)buffer;
	header->version			= 1;
	header->imageCount		= imageCount;
	header->imagesOffset	= sizeof(dyld_process_info_notify_header);
	header->stringsOffset	= sizeof(dyld_process_info_notify_header) + entriesSize;
	header->timestamp		= dyld::gProcessInfo->infoArrayChangeTimestamp;
	dyld_process_info_image_entry* entries = (dyld_process_info_image_entry*)&buffer[header->imagesOffset];
	char* const pathPoolStart = (char*)&buffer[header->stringsOffset];
	char* pathPool = pathPoolStart;
	for (unsigned j=0; j < imageCount; ++j) {
		strcpy(pathPool, imagePaths[j]);
		uint32_t len = (uint32_t)strlen(pathPool);
		bzero(entries->uuid, 16);
		dyld3::MachOFile* mf = (dyld3::MachOFile*)loadAddresses[j];
		mf->getUuid(entries->uuid);
		entries->loadAddress = (uint64_t)loadAddresses[j];
		entries->pathStringOffset = (uint32_t)(pathPool - pathPoolStart);
		entries->pathLength  = len;
		pathPool += (len +1);
		++entries;
	}
	if (unloading) {
		responder.sendMessage(DYLD_PROCESS_INFO_NOTIFY_UNLOAD_ID, totalSize, (mach_msg_header_t*)buffer);
	} else {
		responder.sendMessage(DYLD_PROCESS_INFO_NOTIFY_LOAD_ID, totalSize, (mach_msg_header_t*)buffer);
	}
}

static void notifyMonitoringDyld(bool unloading, unsigned imageCount, const struct mach_header* loadAddresses[],
								 const char* imagePaths[])
{
	dyld3::ScopedTimer(DBG_DYLD_REMOTE_IMAGE_NOTIFIER, 0, 0, 0);
	RemoteNotificationResponder responder;
	if (!responder.active()) { return; }
	notifyMonitoringDyld(responder, unloading, imageCount, loadAddresses, imagePaths);
}

static void notifyMonitoringDyldMain() {
	dyld3::ScopedTimer(DBG_DYLD_REMOTE_IMAGE_NOTIFIER, 0, 0, 0);
	RemoteNotificationResponder responder;
	uint8_t buffer[sizeof(mach_msg_header_t) + MAX_TRAILER_SIZE];
	responder.sendMessage(DYLD_PROCESS_INFO_NOTIFY_MAIN_ID, sizeof(mach_msg_header_t), (mach_msg_header_t*)buffer);
}
#else
extern void notifyMonitoringDyldMain() VIS_HIDDEN;
extern void notifyMonitoringDyld(bool unloading, unsigned imageCount, const struct mach_header* loadAddresses[],
								 const char* imagePaths[]) VIS_HIDDEN;
#endif

void notifyKernel(const ImageLoader& image, bool loading) {
	uint32_t baseCode = loading ? DBG_DYLD_UUID_MAP_A : DBG_DYLD_UUID_UNMAP_A;
	uuid_t uuid;
	image.getUUID(uuid);
	if ( image.inSharedCache() ) {
		dyld3::kdebug_trace_dyld_image(baseCode, image.getInstallPath(), (const uuid_t *)&uuid, {0}, {{ 0, 0 }}, image.machHeader());
	} else {
		fsid_t fsid = {{0, 0}};
		fsobj_id_t fsobj = {0};
		ino_t inode = image.getInode();
		fsobj.fid_objno = (uint32_t)inode;
		fsobj.fid_generation = (uint32_t)(inode>>32);
		fsid.val[0] = image.getDevice();
		dyld3::kdebug_trace_dyld_image(baseCode, image.getPath(), (const uuid_t *)&uuid, fsobj, fsid, image.machHeader());
	}
}

static void notifySingle(dyld_image_states state, const ImageLoader* image, ImageLoader::InitializerTimingList* timingInfo)
{
	//dyld::log("notifySingle(state=%d, image=%s)\n", state, image->getPath());
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(state, sSingleHandlers);
	if ( handlers != NULL ) {
		dyld_image_info info;
		info.imageLoadAddress	= image->machHeader();
		info.imageFilePath		= image->getRealPath();
		info.imageFileModDate	= image->lastModified();
		for (std::vector<dyld_image_state_change_handler>::iterator it = handlers->begin(); it != handlers->end(); ++it) {
			const char* result = (*it)(state, 1, &info);
			if ( (result != NULL) && (state == dyld_image_state_mapped) ) {
				//fprintf(stderr, "  image rejected by handler=%p\n", *it);
				// make copy of thrown string so that later catch clauses can free it
				const char* str = strdup(result);
				throw str;
			}
		}
	}
	if ( state == dyld_image_state_mapped ) {
		// <rdar://problem/7008875> Save load addr + UUID for images from outside the shared cache
		// <rdar://problem/50432671> Include UUIDs for shared cache dylibs in all image info when using private mapped shared caches
		if (!image->inSharedCache()
			|| (gLinkContext.sharedRegionMode == ImageLoader::kUsePrivateSharedRegion)) {
			dyld_uuid_info info;
			if ( image->getUUID(info.imageUUID) ) {
				info.imageLoadAddress = image->machHeader();
				addNonSharedCacheImageUUID(info);
			}
		}
	}
	if ( (state == dyld_image_state_dependents_initialized) && (sNotifyObjCInit != NULL) && image->notifyObjC() ) {
		uint64_t t0 = mach_absolute_time();
		dyld3::ScopedTimer timer(DBG_DYLD_TIMING_OBJC_INIT, (uint64_t)image->machHeader(), 0, 0);
		(*sNotifyObjCInit)(image->getRealPath(), image->machHeader());
		uint64_t t1 = mach_absolute_time();
		uint64_t t2 = mach_absolute_time();
		uint64_t timeInObjC = t1-t0;
		uint64_t emptyTime = (t2-t1)*100;
		if ( (timeInObjC > emptyTime) && (timingInfo != NULL) ) {
			timingInfo->addTime(image->getShortName(), timeInObjC);
		}
	}
    // mach message csdlc about dynamically unloaded images
	if ( image->addFuncNotified() && (state == dyld_image_state_terminated) ) {
		notifyKernel(*image, false);
		const struct mach_header* loadAddress[] = { image->machHeader() };
		const char* loadPath[] = { image->getPath() };
		notifyMonitoringDyld(true, 1, loadAddress, loadPath);
	}
}


//
// Normally, dyld_all_image_infos is only updated in batches after an entire
// graph is loaded.  But if there is an error loading the initial set of
// dylibs needed by the main executable, dyld_all_image_infos is not yet set 
// up, leading to usually brief crash logs.
//
// This function manually adds the images loaded so far to dyld::gProcessInfo.
// It should only be called before terminating.
//
void syncAllImages()
{
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); ++it) {
		dyld_image_info info;
		ImageLoader* image = *it;
		info.imageLoadAddress = image->machHeader();
		info.imageFilePath = image->getRealPath();
		info.imageFileModDate = image->lastModified();
		// add to all_image_infos if not already there
		bool found = false;
		int existingCount = dyld::gProcessInfo->infoArrayCount;
		const dyld_image_info* existing = dyld::gProcessInfo->infoArray;
		if ( existing != NULL ) {
			for (int i=0; i < existingCount; ++i) {
				if ( existing[i].imageLoadAddress == info.imageLoadAddress ) {
					//dyld::log("not adding %s\n", info.imageFilePath);
					found = true;
					break;
				}
			}
		}
		if ( ! found ) {
			//dyld::log("adding %s\n", info.imageFilePath);
			addImagesToAllImages(1, &info);
		}
	}
}


static int imageSorter(const void* l, const void* r)
{
	const ImageLoader* left = *((ImageLoader**)l);
	const ImageLoader* right= *((ImageLoader**)r);
	return left->compare(right);
}

static void notifyBatchPartial(dyld_image_states state, bool orLater, dyld_image_state_change_handler onlyHandler, bool preflightOnly, bool onlyObjCMappedNotification)
{
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(state, sBatchHandlers);
	if ( (handlers != NULL) || ((state == dyld_image_state_bound) && (sNotifyObjCMapped != NULL)) ) {
		// don't use a vector because it will use malloc/free and we want notifcation to be low cost
        allImagesLock();
		dyld_image_info	infos[allImagesCount()+1];
        ImageLoader* images[allImagesCount()+1];
        ImageLoader** end = images;
        for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
            dyld_image_states imageState = (*it)->getState();
            if ( (imageState == state) || (orLater && (imageState > state)) )
                *end++ = *it;
        }
		if ( sBundleBeingLoaded != NULL ) {
			dyld_image_states imageState = sBundleBeingLoaded->getState();
			if ( (imageState == state) || (orLater && (imageState > state)) )
				*end++ = sBundleBeingLoaded;
		}
        const char* dontLoadReason = NULL;
		uint32_t imageCount = (uint32_t)(end-images);
		if ( imageCount != 0 ) {
			// sort bottom up
			qsort(images, imageCount, sizeof(ImageLoader*), &imageSorter);

			const mach_header* mhs[imageCount];
			const char*        paths[imageCount];
			uint32_t 		   bulkNotifyImageCount = 0;

			// build info array
			for (unsigned int i=0; i < imageCount; ++i) {
				dyld_image_info* p = &infos[i];
				ImageLoader* image = images[i];
				//dyld::log("  state=%d, name=%s\n", state, image->getPath());
				p->imageLoadAddress = image->machHeader();
				p->imageFilePath    = image->getRealPath();
				p->imageFileModDate = image->lastModified();
				// get these registered with the kernel as early as possible
				if ( state == dyld_image_state_dependents_mapped)
					notifyKernel(*image, true);
				// special case for add_image hook
				if ( state == dyld_image_state_bound ) {
					if ( notifyAddImageCallbacks(image) ) {
						// Add this to the list of images to bulk notify
						mhs[bulkNotifyImageCount]   = infos[i].imageLoadAddress;
						paths[bulkNotifyImageCount] = infos[i].imageFilePath;
						++bulkNotifyImageCount;
					}
				}
			}

			if ( (state == dyld_image_state_bound) && !sAddBulkLoadImageCallbacks.empty() && (bulkNotifyImageCount != 0) ) {
				for (LoadImageBulkCallback func : sAddBulkLoadImageCallbacks) {
					dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)mhs[0], (uint64_t)func, 0);
					(*func)(bulkNotifyImageCount, mhs, paths);
				}
			}
		}
#if SUPPORT_ACCELERATE_TABLES
		if ( sAllCacheImagesProxy != NULL ) {
			unsigned cacheCount = sAllCacheImagesProxy->appendImagesToNotify(state, orLater, &infos[imageCount]);
			// support _dyld_register_func_for_add_image()
			if ( state == dyld_image_state_bound ) {
				for (ImageCallback callback : sAddImageCallbacks) {
					for (unsigned i=0; i < cacheCount; ++i) {
						dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)infos[imageCount+i].imageLoadAddress, (uint64_t)(*callback), 0);
						(*callback)(infos[imageCount+i].imageLoadAddress, sSharedCacheLoadInfo.slide);
					}
				}
				for (LoadImageCallback func : sAddLoadImageCallbacks) {
					for (unsigned i=0; i < cacheCount; ++i) {
						dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)infos[imageCount+i].imageLoadAddress, (uint64_t)(*func), 0);
						(*func)(infos[imageCount+i].imageLoadAddress, infos[imageCount+i].imageFilePath, false);
					}
				}
				if ( !sAddBulkLoadImageCallbacks.empty() ) {
					const mach_header* bulk_mhs[cacheCount];
					const char*        bulk_paths[cacheCount];
					for (int i=0; i < cacheCount; ++i) {
						bulk_mhs[i]   = infos[imageCount+i].imageLoadAddress;
						bulk_paths[i] = infos[imageCount+i].imageFilePath;
					}
					for (LoadImageBulkCallback func : sAddBulkLoadImageCallbacks) {
						dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)bulk_mhs[0], (uint64_t)func, 0);
						(*func)(cacheCount, bulk_mhs, bulk_paths);
					}
				}
			}
			imageCount += cacheCount;
		}
#endif
		if ( imageCount != 0 ) {
			if ( !onlyObjCMappedNotification ) {
				if ( onlyHandler != NULL ) {
					const char* result = NULL;
					if ( result == NULL ) {
						result = (*onlyHandler)(state, imageCount, infos);
					}
					if ( (result != NULL) && (state == dyld_image_state_dependents_mapped) ) {
						//fprintf(stderr, "  images rejected by handler=%p\n", onlyHandler);
						// make copy of thrown string so that later catch clauses can free it
						dontLoadReason = strdup(result);
					}
				}
				else {
					// call each handler with whole array
					if ( handlers != NULL ) {
						for (std::vector<dyld_image_state_change_handler>::iterator it = handlers->begin(); it != handlers->end(); ++it) {
							const char* result = (*it)(state, imageCount, infos);
							if ( (result != NULL) && (state == dyld_image_state_dependents_mapped) ) {
								//fprintf(stderr, "  images rejected by handler=%p\n", *it);
								// make copy of thrown string so that later catch clauses can free it
								dontLoadReason = strdup(result);
								break;
							}
						}
					}
				}
			}
			// tell objc about new images
			if ( (onlyHandler == NULL) && ((state == dyld_image_state_bound) || (orLater && (dyld_image_state_bound > state))) && (sNotifyObjCMapped != NULL) ) {
				const char* paths[imageCount];
				const mach_header* mhs[imageCount];
				unsigned objcImageCount = 0;
				for (int i=0; i < imageCount; ++i) {
					ImageLoader* image = findImageByMachHeader(infos[i].imageLoadAddress);
					bool hasObjC = false;
					if ( image != NULL ) {
						if ( image->objCMappedNotified() )
							continue;
						hasObjC = image->notifyObjC();
					}
#if SUPPORT_ACCELERATE_TABLES
					else if ( sAllCacheImagesProxy != NULL ) {
						const mach_header* mh;
						const char* path;
						unsigned index;
						if ( sAllCacheImagesProxy->addressInCache(infos[i].imageLoadAddress, &mh, &path, &index) ) {
							hasObjC = (mh->flags & MH_HAS_OBJC);
						}
					}
#endif
					if ( hasObjC ) {
						paths[objcImageCount] = infos[i].imageFilePath;
						mhs[objcImageCount]   = infos[i].imageLoadAddress;
						++objcImageCount;
						if ( image != NULL )
							image->setObjCMappedNotified();
					}
				}
				if ( objcImageCount != 0 ) {
					dyld3::ScopedTimer timer(DBG_DYLD_TIMING_OBJC_MAP, 0, 0, 0);
					uint64_t t0 = mach_absolute_time();
					(*sNotifyObjCMapped)(objcImageCount, paths, mhs);
					uint64_t t1 = mach_absolute_time();
					ImageLoader::fgTotalObjCSetupTime += (t1-t0);
				}
			}
		}
        allImagesUnlock();
        if ( dontLoadReason != NULL )
            throw dontLoadReason;
		if ( !preflightOnly && (state == dyld_image_state_dependents_mapped) ) {
			const struct mach_header* loadAddresses[imageCount];
			const char* loadPaths[imageCount];
			for(uint32_t i = 0; i<imageCount; ++i) {
				loadAddresses[i] = infos[i].imageLoadAddress;
				loadPaths[i] = infos[i].imageFilePath;
			}
			notifyMonitoringDyld(false, imageCount, loadAddresses, loadPaths);
		}
	}
}

static void notifyBatch(dyld_image_states state, bool preflightOnly)
{
	notifyBatchPartial(state, false, NULL, preflightOnly, false);
}

#if TARGET_OS_OSX
static
void coresymbolication_load_notifier(void* connection, uint64_t timestamp, const char* path, const struct mach_header* mh)
{
	const struct mach_header* loadAddress[] = { mh };
	const char* loadPath[] = { path };
	notifyMonitoringDyld(false, 1, loadAddress, loadPath);
}

static
void coresymbolication_unload_notifier(void* connection, uint64_t timestamp, const char* path, const struct mach_header* mh)
{
	const struct mach_header* loadAddress = { mh };
	const char* loadPath = { path };
	notifyMonitoringDyld(true, 1, &loadAddress, &loadPath);
}

static
kern_return_t legacy_task_register_dyld_image_infos(task_t task, dyld_kernel_image_info_array_t dyld_images,
											 mach_msg_type_number_t dyld_imagesCnt)
{
	return KERN_SUCCESS;
}

static
kern_return_t legacy_task_unregister_dyld_image_infos(task_t task, dyld_kernel_image_info_array_t dyld_images,
											   mach_msg_type_number_t dyld_imagesCnt)
{
	return KERN_SUCCESS;
}

static
kern_return_t legacy_task_get_dyld_image_infos(task_inspect_t task, dyld_kernel_image_info_array_t *dyld_images,
										mach_msg_type_number_t *dyld_imagesCnt)
{
	return KERN_SUCCESS;
}

static
kern_return_t legacy_task_register_dyld_shared_cache_image_info(task_t task, dyld_kernel_image_info_t dyld_cache_image,
														 boolean_t no_cache, boolean_t private_cache)
{
	return KERN_SUCCESS;
}

static
kern_return_t legacy_task_register_dyld_set_dyld_state(task_t task, uint8_t dyld_state)
{
	return KERN_SUCCESS;
}

static
kern_return_t legacy_task_register_dyld_get_process_state(task_t task, dyld_kernel_process_info_t *dyld_process_state)
{
	return KERN_SUCCESS;
}
#endif

// In order for register_func_for_add_image() callbacks to to be called bottom up,
// we need to maintain a list of root images. The main executable is usally the
// first root. Any images dynamically added are also roots (unless already loaded).
// If DYLD_INSERT_LIBRARIES is used, those libraries are first.
static void addRootImage(ImageLoader* image)
{
	//dyld::log("addRootImage(%p, %s)\n", image, image->getPath());
	// add to list of roots
	sImageRoots.push_back(image);
}


static void clearAllDepths()
{
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++)
		(*it)->clearDepth();
}

static void printAllDepths()
{
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++)
		dyld::log("%03d %s\n",  (*it)->getDepth(), (*it)->getShortName());
}


static unsigned int imageCount()
{
    allImagesLock();
		unsigned int result = (unsigned int)sAllImages.size();
    allImagesUnlock();
	return (result);
}


static void setNewProgramVars(const ProgramVars& newVars)
{
	// make a copy of the pointers to program variables
	gLinkContext.programVars = newVars;
	
	// now set each program global to their initial value
	*gLinkContext.programVars.NXArgcPtr = gLinkContext.argc;
	*gLinkContext.programVars.NXArgvPtr = gLinkContext.argv;
	*gLinkContext.programVars.environPtr = gLinkContext.envp;
	*gLinkContext.programVars.__prognamePtr = gLinkContext.progname;
}

#if SUPPORT_OLD_CRT_INITIALIZATION
static void setRunInitialzersOldWay()
{
	gRunInitializersOldWay = true;		
}
#endif

static bool sandboxBlocked(const char* path, const char* kind)
{
#if TARGET_OS_SIMULATOR
	// sandbox calls not yet supported in simulator runtime
	return false;
#else
	sandbox_filter_type filter = (sandbox_filter_type)(SANDBOX_FILTER_PATH | SANDBOX_CHECK_NO_REPORT);
	return ( sandbox_check(getpid(), kind, filter, path) > 0 );
#endif
}

bool sandboxBlockedMmap(const char* path)
{
	return sandboxBlocked(path, "file-map-executable");
}

bool sandboxBlockedOpen(const char* path)
{
	return sandboxBlocked(path, "file-read-data");
}

bool sandboxBlockedStat(const char* path)
{
	return sandboxBlocked(path, "file-read-metadata");
}


static void addDynamicReference(ImageLoader* from, ImageLoader* to) {
	// don't add dynamic reference if target is in the shared cache (since it can't be unloaded)
	if ( to->inSharedCache() )
		return;

	// don't add dynamic reference if there already is a static one
	if ( from->dependsOn(to) )
		return;
	
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	// don't add if this combination already exists
	OSSpinLockLock(&sDynamicReferencesLock);
	for (std::vector<ImageLoader::DynamicReference>::iterator it=sDynamicReferences.begin(); it != sDynamicReferences.end(); ++it) {
		if ( (it->from == from) && (it->to == to) ) {
			OSSpinLockUnlock(&sDynamicReferencesLock);
			return;
		}
	}

	//dyld::log("addDynamicReference(%s, %s\n", from->getShortName(), to->getShortName());
	ImageLoader::DynamicReference t;
	t.from = from;
	t.to = to;
	sDynamicReferences.push_back(t);
	OSSpinLockUnlock(&sDynamicReferencesLock);
#pragma clang diagnostic pop
}

static void addImage(ImageLoader* image)
{
	// add to master list
    allImagesLock();
        sAllImages.push_back(image);
    allImagesUnlock();
	
	// update mapped ranges
	uintptr_t lastSegStart = 0;
	uintptr_t lastSegEnd = 0;
	for(unsigned int i=0, e=image->segmentCount(); i < e; ++i) {
		if ( image->segUnaccessible(i) ) 
			continue;
		uintptr_t start = image->segActualLoadAddress(i);
		uintptr_t end = image->segActualEndAddress(i);
		if ( start == lastSegEnd ) {
			// two segments are contiguous, just record combined segments
			lastSegEnd = end;
		}
		else {
			// non-contiguous segments, record last (if any)
			if ( lastSegEnd != 0 )
				addMappedRange(image, lastSegStart, lastSegEnd);
			lastSegStart = start;
			lastSegEnd = end;
		}		
	}
	if ( lastSegEnd != 0 )
		addMappedRange(image, lastSegStart, lastSegEnd);

	
	if ( gLinkContext.verboseLoading || (sEnv.DYLD_PRINT_LIBRARIES_POST_LAUNCH && (sMainExecutable!=NULL) && sMainExecutable->isLinked()) ) {
		const char *imagePath = image->getPath();
		uuid_t imageUUID;
		if ( image->getUUID(imageUUID) ) {
			uuid_string_t imageUUIDStr;
			uuid_unparse_upper(imageUUID, imageUUIDStr);
			dyld::log("dyld: loaded: <%s> %s\n", imageUUIDStr, imagePath);
		}
		else {
			dyld::log("dyld: loaded: %s\n", imagePath);
		}
	}
	
}

//
// Helper for std::remove_if
//
class RefUsesImage {
public:
	RefUsesImage(ImageLoader* image) : _image(image) {}
	bool operator()(const ImageLoader::DynamicReference& ref) const {
		return ( (ref.from == _image) || (ref.to == _image) );
	}
private:
	ImageLoader* _image;
};



void removeImage(ImageLoader* image)
{
	// if has dtrace DOF section, tell dtrace it is going away, then remove from sImageFilesNeedingDOFUnregistration
	for (std::vector<RegisteredDOF>::iterator it=sImageFilesNeedingDOFUnregistration.begin(); it != sImageFilesNeedingDOFUnregistration.end(); ) {
		if ( it->mh == image->machHeader() ) {
			unregisterDOF(it->registrationID);
			sImageFilesNeedingDOFUnregistration.erase(it);
			// don't increment iterator, the erase caused next element to be copied to where this iterator points
		}
		else {
			++it;
		}
	}
	
	// tell all registered remove image handlers about this
	// do this before removing image from internal data structures so that the callback can query dyld about the image
	if ( image->getState() >= dyld_image_state_bound ) {
		sRemoveImageCallbacksInUse = true; // This only runs inside dyld's global lock, so ok to use a global for the in-use flag.
		for (std::vector<ImageCallback>::iterator it=sRemoveImageCallbacks.begin(); it != sRemoveImageCallbacks.end(); it++) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_REMOVE_IMAGE, (uint64_t)image->machHeader(), (uint64_t)(*it), 0);
			(*it)(image->machHeader(), image->getSlide());
		}
		sRemoveImageCallbacksInUse = false;

		if ( sNotifyObjCUnmapped !=  NULL && image->notifyObjC() )
			(*sNotifyObjCUnmapped)(image->getRealPath(), image->machHeader());
	}
	
	// notify 
	notifySingle(dyld_image_state_terminated, image, NULL);
	
	// remove from mapped images table
	removedMappedRanges(image);

	// remove from master list
    allImagesLock();
        for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
            if ( *it == image ) {
                sAllImages.erase(it);
                break;
            }
        }
    allImagesUnlock();
	
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	// remove from sDynamicReferences
	OSSpinLockLock(&sDynamicReferencesLock);
		sDynamicReferences.erase(std::remove_if(sDynamicReferences.begin(), sDynamicReferences.end(), RefUsesImage(image)), sDynamicReferences.end());
	OSSpinLockUnlock(&sDynamicReferencesLock);
#pragma clang diagnostic pop

	// flush find-by-address cache (do this after removed from master list, so there is no chance it can come back)
	if ( sLastImageByAddressCache == image )
		sLastImageByAddressCache = NULL;

	// if in root list, pull it out 
	for (std::vector<ImageLoader*>::iterator it=sImageRoots.begin(); it != sImageRoots.end(); it++) {
		if ( *it == image ) {
			sImageRoots.erase(it);
			break;
		}
	}

	// If this image is the potential canonical definition of any weak defs, then set them to a tombstone value
	if ( gLinkContext.weakDefMapInitialized && image->hasCoalescedExports() && (image->getState() >= dyld_image_state_bound) ) {
		Diagnostics diag;
		const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)image->machHeader();
		ma->forEachWeakDef(diag, ^(const char *symbolName, uint64_t imageOffset, bool isFromExportTrie) {
			auto it = gLinkContext.weakDefMap.find(symbolName);
			if ( it == gLinkContext.weakDefMap.end() )
				return;
			it->second = { nullptr, 0 };
			if ( !isFromExportTrie ) {
				// The string was already duplicated if we are an export trie
				// so only strdup as we are the nlist
				size_t hash1 = ImageLoader::HashCString::hash(it->first);
				it->first = strdup(it->first);
				size_t hash2 = ImageLoader::HashCString::hash(it->first);
				assert(hash1 == hash2);
			}
		});
	}

	// log if requested
	if ( gLinkContext.verboseLoading || (sEnv.DYLD_PRINT_LIBRARIES_POST_LAUNCH && (sMainExecutable!=NULL) && sMainExecutable->isLinked()) ) {
		const char *imagePath = image->getPath();
		uuid_t imageUUID;
		if ( image->getUUID(imageUUID) ) {
			uuid_string_t imageUUIDStr;
			uuid_unparse_upper(imageUUID, imageUUIDStr);
			dyld::log("dyld: unloaded: <%s> %s\n", imageUUIDStr, imagePath);
		}
		else {
			dyld::log("dyld: unloaded: %s\n", imagePath);
		}
	}

	// tell gdb, new way
	removeImageFromAllImages(image->machHeader());
}


void runImageStaticTerminators(ImageLoader* image)
{
	// if in termination list, pull it out and run terminator
	bool mightBeMore;
	do {
		mightBeMore = false;
		for (std::vector<ImageLoader*>::iterator it=sImageFilesNeedingTermination.begin(); it != sImageFilesNeedingTermination.end(); it++) {
			if ( *it == image ) {
				sImageFilesNeedingTermination.erase(it);
				if (gLogAPIs) dyld::log("dlclose(), running static terminators for %p %s\n", image, image->getShortName());
				image->doTermination(gLinkContext);
				mightBeMore = true;
				break;
			}
		}
	} while ( mightBeMore );
}

static void terminationRecorder(ImageLoader* image)
{
	bool add = true;
#if __arm64e__
	// <rdar://problem/71820555> Don't run static terminator for arm64e
	const mach_header* mh = image->machHeader();
	if ( (mh->cputype == CPU_TYPE_ARM64) && ((mh->cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E) )
		add = false;
#endif
	if ( add )
		sImageFilesNeedingTermination.push_back(image);
}

const char* getExecutablePath()
{
	return sExecPath;
}

static void runAllStaticTerminators(void* extra)
{
	try {
		const size_t imageCount = sImageFilesNeedingTermination.size();
		for(size_t i=imageCount; i > 0; --i){
			ImageLoader* image = sImageFilesNeedingTermination[i-1];
			image->doTermination(gLinkContext);
		}
		sImageFilesNeedingTermination.clear();
		notifyBatch(dyld_image_state_terminated, false);
	}
	catch (const char* msg) {
		halt(msg);
	}
}

void initializeMainExecutable()
{
	// record that we've reached this step
	gLinkContext.startedInitializingMainExecutable = true;

	// run initialzers for any inserted dylibs
	ImageLoader::InitializerTimingList initializerTimes[allImagesCount()];
	initializerTimes[0].count = 0;
	const size_t rootCount = sImageRoots.size();
	if ( rootCount > 1 ) {
		for(size_t i=1; i < rootCount; ++i) {
			sImageRoots[i]->runInitializers(gLinkContext, initializerTimes[0]);
		}
	}
	
	// run initializers for main executable and everything it brings up 
	sMainExecutable->runInitializers(gLinkContext, initializerTimes[0]);
	
	// register cxa_atexit() handler to run static terminators in all loaded images when this process exits
	if ( gLibSystemHelpers != NULL ) 
		(*gLibSystemHelpers->cxa_atexit)(&runAllStaticTerminators, NULL, NULL);

	// dump info if requested
	if ( sEnv.DYLD_PRINT_STATISTICS )
		ImageLoader::printStatistics((unsigned int)allImagesCount(), initializerTimes[0]);
	if ( sEnv.DYLD_PRINT_STATISTICS_DETAILS )
		ImageLoaderMachO::printStatisticsDetails((unsigned int)allImagesCount(), initializerTimes[0]);
}

bool mainExecutablePrebound()
{
	return sMainExecutable->usablePrebinding(gLinkContext);
}

ImageLoader* mainExecutable()
{
	return sMainExecutable;
}




#if SUPPORT_VERSIONED_PATHS

// forward reference
static bool getDylibVersionAndInstallname(const char* dylibPath, uint32_t* version, char* installName);


//
// Examines a dylib file and if its current_version is newer than the installed
// dylib at its install_name, then add the dylib file to sDylibOverrides.
//
static void checkDylibOverride(const char* dylibFile)
{
	//dyld::log("checkDylibOverride('%s')\n", dylibFile);
	uint32_t altVersion;
 	char sysInstallName[PATH_MAX];
	if ( getDylibVersionAndInstallname(dylibFile, &altVersion, sysInstallName) && (sysInstallName[0] =='/') ) {
		//dyld::log("%s has version 0x%08X and install name %s\n", dylibFile, altVersion, sysInstallName);
		uint32_t sysVersion;
		if ( getDylibVersionAndInstallname(sysInstallName, &sysVersion, NULL) ) {
			//dyld::log("%s has version 0x%08X\n", sysInstallName, sysVersion);
			if ( altVersion > sysVersion ) {
				//dyld::log("override found: %s -> %s\n", sysInstallName, dylibFile);
				// see if there already is an override for this dylib
				bool entryExists = false;
				for (std::vector<DylibOverride>::iterator it = sDylibOverrides.begin(); it != sDylibOverrides.end(); ++it) {
					if ( strcmp(it->installName, sysInstallName) == 0 ) {
						entryExists = true;
						uint32_t prevVersion;
						if ( getDylibVersionAndInstallname(it->override, &prevVersion, NULL) ) {
							if ( altVersion > prevVersion ) {
								// found an even newer override
								free((void*)(it->override));
								char resolvedPath[PATH_MAX];
								if ( realpath(dylibFile, resolvedPath) != NULL )
									it->override = strdup(resolvedPath);
								else
									it->override = strdup(dylibFile);
								break;
							}
						}
					}
				}
				if ( ! entryExists ) {
					DylibOverride entry;
					entry.installName = strdup(sysInstallName);
					char resolvedPath[PATH_MAX];
					if ( realpath(dylibFile, resolvedPath) != NULL )
						entry.override = strdup(resolvedPath);
					else
						entry.override = strdup(dylibFile);
					sDylibOverrides.push_back(entry);
					//dyld::log("added override: %s -> %s\n", entry.installName, entry.override);
				}
			}
		}
	}
	
}

static void checkDylibOverridesInDir(const char* dirPath)
{
	//dyld::log("checkDylibOverridesInDir('%s')\n", dirPath);
	char dylibPath[PATH_MAX];
	long dirPathLen = strlcpy(dylibPath, dirPath, PATH_MAX-1);
	if ( dirPathLen >= PATH_MAX )
		return;
	DIR* dirp = opendir(dirPath);
	if ( dirp != NULL) {
		dirent entry;
		dirent* entp = NULL;
		while ( readdir_r(dirp, &entry, &entp) == 0 ) {
			if ( entp == NULL )
				break;
			if ( entp->d_type != DT_REG ) 
				continue;
			dylibPath[dirPathLen] = '/';
			dylibPath[dirPathLen+1] = '\0';
			if ( strlcat(dylibPath, entp->d_name, PATH_MAX) >= PATH_MAX )
				continue;
			checkDylibOverride(dylibPath);
		}
		closedir(dirp);
	}
}


static void checkFrameworkOverridesInDir(const char* dirPath)
{
	//dyld::log("checkFrameworkOverridesInDir('%s')\n", dirPath);
	char frameworkPath[PATH_MAX];
	long dirPathLen = strlcpy(frameworkPath, dirPath, PATH_MAX-1);
	if ( dirPathLen >= PATH_MAX )
		return;
	DIR* dirp = opendir(dirPath);
	if ( dirp != NULL) {
		dirent entry;
		dirent* entp = NULL;
		while ( readdir_r(dirp, &entry, &entp) == 0 ) {
			if ( entp == NULL )
				break;
			if ( entp->d_type != DT_DIR ) 
				continue;
			frameworkPath[dirPathLen] = '/';
			frameworkPath[dirPathLen+1] = '\0';
			int dirNameLen = (int)strlen(entp->d_name);
			if ( dirNameLen < 11 )
				continue;
			if ( strcmp(&entp->d_name[dirNameLen-10], ".framework") != 0 )
				continue;
			if ( strlcat(frameworkPath, entp->d_name, PATH_MAX) >= PATH_MAX )
				continue;
			if ( strlcat(frameworkPath, "/", PATH_MAX) >= PATH_MAX )
				continue;
			if ( strlcat(frameworkPath, entp->d_name, PATH_MAX) >= PATH_MAX )
				continue;
			frameworkPath[strlen(frameworkPath)-10] = '\0';
			checkDylibOverride(frameworkPath);
		}
		closedir(dirp);
	}
}
#endif // SUPPORT_VERSIONED_PATHS


//
// Turns a colon separated list of strings into a NULL terminated array 
// of string pointers. If mainExecutableDir param is not NULL,
// substitutes @loader_path with main executable's dir.
//
static const char** parseColonList(const char* list, const char* mainExecutableDir)
{
	static const char* sEmptyList[] = { NULL };

	if ( list[0] == '\0' ) 
		return sEmptyList;
	
	int colonCount = 0;
	for(const char* s=list; *s != '\0'; ++s) {
		if (*s == ':') 
			++colonCount;
	}
	
	int index = 0;
	const char* start = list;
	char** result = new char*[colonCount+2];
	for(const char* s=list; *s != '\0'; ++s) {
		if (*s == ':') {
			size_t len = s-start;
			if ( (mainExecutableDir != NULL) && (strncmp(start, "@loader_path/", 13) == 0) ) {
				if ( !gLinkContext.allowAtPaths ) {
					dyld::log("dyld: warning: @loader_path/ ignored because of amfi policy (Codesign main executable with Library Validation to allow @ paths)\n");
					continue;
				}
				size_t mainExecDirLen = strlen(mainExecutableDir);
				char* str = new char[mainExecDirLen+len+1];
				strcpy(str, mainExecutableDir);
				strlcat(str, &start[13], mainExecDirLen+len+1);
				str[mainExecDirLen+len-13] = '\0';
				start = &s[1];
				result[index++] = str;
			}
			else if ( (mainExecutableDir != NULL) && (strncmp(start, "@executable_path/", 17) == 0) ) {
				if ( !gLinkContext.allowAtPaths ) {
					dyld::log("dyld: warning: @executable_path/ ignored because of amfi policy (Codesign main executable with Library Validation to allow @ paths)\n");
					continue;
				}
				size_t mainExecDirLen = strlen(mainExecutableDir);
				char* str = new char[mainExecDirLen+len+1];
				strcpy(str, mainExecutableDir);
				strlcat(str, &start[17], mainExecDirLen+len+1);
				str[mainExecDirLen+len-17] = '\0';
				start = &s[1];
				result[index++] = str;
			}
			else {
				char* str = new char[len+1];
				strncpy(str, start, len);
				str[len] = '\0';
				start = &s[1];
				result[index++] = str;
			}
		}
	}
	size_t len = strlen(start);
	if ( (mainExecutableDir != NULL) && (strncmp(start, "@loader_path/", 13) == 0) ) {
		if ( !gLinkContext.allowAtPaths ) {
			dyld::log("dyld: warning: @loader_path/ ignored because of amfi policy (Codesign main executable with Library Validation to allow @ paths)\n");
		}
		else
		{
			size_t mainExecDirLen = strlen(mainExecutableDir);
			char* str = new char[mainExecDirLen+len+1];
			strcpy(str, mainExecutableDir);
			strlcat(str, &start[13], mainExecDirLen+len+1);
			str[mainExecDirLen+len-13] = '\0';
			result[index++] = str;
		}
	}
	else if ( (mainExecutableDir != NULL) && (strncmp(start, "@executable_path/", 17) == 0) ) {
		if ( !gLinkContext.allowAtPaths ) {
			dyld::log("dyld: warning: @executable_path/ ignored because of amfi policy (Codesign main executable with Library Validation to allow @ paths)\n");
		}
		else
		{
			size_t mainExecDirLen = strlen(mainExecutableDir);
			char* str = new char[mainExecDirLen+len+1];
			strcpy(str, mainExecutableDir);
			strlcat(str, &start[17], mainExecDirLen+len+1);
			str[mainExecDirLen+len-17] = '\0';
			result[index++] = str;
		}
	}
	else {
		char* str = new char[len+1];
		strcpy(str, start);
		result[index++] = str;
	}
	result[index] = NULL;
	
	//dyld::log("parseColonList(%s)\n", list);
	//for(int i=0; result[i] != NULL; ++i)
	//	dyld::log("  %s\n", result[i]);
	return (const char**)result;
}

static void	appendParsedColonList(const char* list, const char* mainExecutableDir, const char* const ** storage)
{
	const char** newlist = parseColonList(list, mainExecutableDir);
	if ( *storage == NULL ) {
		// first time, just set
		*storage = newlist;
	}
	else {
		// need to append to existing list
		const char* const* existing = *storage;
		int count = 0;
		for(int i=0; existing[i] != NULL; ++i)
			++count;
		for(int i=0; newlist[i] != NULL; ++i)
			++count;
		const char** combinedList = new const char*[count+2];
		int index = 0;
		for(int i=0; existing[i] != NULL; ++i)
			combinedList[index++] = existing[i];
		for(int i=0; newlist[i] != NULL; ++i)
			combinedList[index++] = newlist[i];
		combinedList[index] = NULL;
		delete[] newlist; // free array, note: strings in newList may be leaked
		*storage = combinedList;
	}
}

#if TARGET_OS_OSX
static void paths_expand_roots(const char **paths, const char *key, const char *val)
{
// 	assert(val != NULL);
// 	assert(paths != NULL);
	if(NULL != key) {
		size_t keyLen = strlen(key);
		for(int i=0; paths[i] != NULL; ++i) {
			if ( strncmp(paths[i], key, keyLen) == 0 ) {
				char* newPath = new char[strlen(val) + (strlen(paths[i]) - keyLen) + 1];
				strcpy(newPath, val);
				strcat(newPath, &paths[i][keyLen]);
				paths[i] = newPath;
			}
		}
	}
	return;
}

static void removePathWithPrefix(const char* paths[], const char* prefix)
{
    size_t prefixLen = strlen(prefix);
    int skip = 0;
    int i;
    for(i = 0; paths[i] != NULL; ++i) {
        if ( strncmp(paths[i], prefix, prefixLen) == 0 )
            ++skip;
        else
            paths[i-skip] = paths[i];
    }
    paths[i-skip] = NULL;
}
#endif


#if 0
static void paths_dump(const char **paths)
{
//   assert(paths != NULL);
  const char **strs = paths;
  while(*strs != NULL)
  {
    dyld::log("\"%s\"\n", *strs);
    strs++;
  }
  return;
}
#endif



static void printOptions(const char* argv[])
{
	uint32_t i = 0;
	while ( NULL != argv[i] ) {
		dyld::log("opt[%i] = \"%s\"\n", i, argv[i]);
		i++;
	}
}

static void printEnvironmentVariables(const char* envp[])
{
	while ( NULL != *envp ) {
		dyld::log("%s\n", *envp);
		envp++;
	}
}

void processDyldEnvironmentVariable(const char* key, const char* value, const char* mainExecutableDir)
{
	if ( strcmp(key, "DYLD_FRAMEWORK_PATH") == 0 ) {
		appendParsedColonList(value, mainExecutableDir, &sEnv.DYLD_FRAMEWORK_PATH);
		sEnv.hasOverride = true;
	}
	else if ( strcmp(key, "DYLD_FALLBACK_FRAMEWORK_PATH") == 0 ) {
		appendParsedColonList(value, mainExecutableDir, &sEnv.DYLD_FALLBACK_FRAMEWORK_PATH);
		sEnv.hasOverride = true;
	}
	else if ( strcmp(key, "DYLD_LIBRARY_PATH") == 0 ) {
		appendParsedColonList(value, mainExecutableDir, &sEnv.DYLD_LIBRARY_PATH);
		sEnv.hasOverride = true;
	}
	else if ( strcmp(key, "DYLD_FALLBACK_LIBRARY_PATH") == 0 ) {
		appendParsedColonList(value, mainExecutableDir, &sEnv.DYLD_FALLBACK_LIBRARY_PATH);
		sEnv.hasOverride = true;
	}
#if SUPPORT_ROOT_PATH
	else if ( (strcmp(key, "DYLD_ROOT_PATH") == 0) || (strcmp(key, "DYLD_PATHS_ROOT") == 0) ) {
		if ( strcmp(value, "/") != 0 ) {
			gLinkContext.rootPaths = parseColonList(value, mainExecutableDir);
			for (int i=0; gLinkContext.rootPaths[i] != NULL; ++i) {
				if ( gLinkContext.rootPaths[i][0] != '/' ) {
					dyld::warn("DYLD_ROOT_PATH not used because it contains a non-absolute path\n");
					gLinkContext.rootPaths = NULL;
					break;
				}
			}
		}
		sEnv.hasOverride = true;
	}
#endif
	else if ( strcmp(key, "DYLD_IMAGE_SUFFIX") == 0 ) {
		gLinkContext.imageSuffix = parseColonList(value, NULL);
		sEnv.hasOverride = true;
	}
	else if ( strcmp(key, "DYLD_INSERT_LIBRARIES") == 0 ) {
		sEnv.DYLD_INSERT_LIBRARIES = parseColonList(value, NULL);
#if SUPPORT_ACCELERATE_TABLES
		sDisableAcceleratorTables = true;
#endif
		sEnv.hasOverride = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_OPTS") == 0 ) {
		sEnv.DYLD_PRINT_OPTS = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_ENV") == 0 ) {
		sEnv.DYLD_PRINT_ENV = true;
	}
	else if ( strcmp(key, "DYLD_DISABLE_DOFS") == 0 ) {
		sEnv.DYLD_DISABLE_DOFS = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_LIBRARIES") == 0 ) {
		gLinkContext.verboseLoading = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_LIBRARIES_POST_LAUNCH") == 0 ) {
		sEnv.DYLD_PRINT_LIBRARIES_POST_LAUNCH = true;
	}
	else if ( strcmp(key, "DYLD_BIND_AT_LAUNCH") == 0 ) {
		sEnv.DYLD_BIND_AT_LAUNCH = true;
	}
	else if ( strcmp(key, "DYLD_FORCE_FLAT_NAMESPACE") == 0 ) {
		gLinkContext.bindFlat = true;
	}
	else if ( strcmp(key, "DYLD_NEW_LOCAL_SHARED_REGIONS") == 0 ) {
		// ignore, no longer relevant but some scripts still set it
	}
	else if ( strcmp(key, "DYLD_NO_FIX_PREBINDING") == 0 ) {
	}
	else if ( strcmp(key, "DYLD_PREBIND_DEBUG") == 0 ) {
		gLinkContext.verbosePrebinding = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_INITIALIZERS") == 0 ) {
		gLinkContext.verboseInit = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_DOFS") == 0 ) {
		gLinkContext.verboseDOF = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_STATISTICS") == 0 ) {
		sEnv.DYLD_PRINT_STATISTICS = true;
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
		// <rdar://problem/26614838> DYLD_PRINT_STATISTICS no longer logs to xcode console for device apps
		sForceStderr = true;
#endif
	}
	else if ( strcmp(key, "DYLD_PRINT_TO_STDERR") == 0 ) {
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
		// <rdar://problem/26633440> DYLD_PRINT_STATISTICS no longer logs to xcode console for device apps
		sForceStderr = true;
#endif
	}
	else if ( strcmp(key, "DYLD_PRINT_STATISTICS_DETAILS") == 0 ) {
		sEnv.DYLD_PRINT_STATISTICS_DETAILS = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_SEGMENTS") == 0 ) {
		gLinkContext.verboseMapping = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_BINDINGS") == 0 ) {
		gLinkContext.verboseBind = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_WEAK_BINDINGS") == 0 ) {
		gLinkContext.verboseWeakBind = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_REBASINGS") == 0 ) {
		gLinkContext.verboseRebase = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_APIS") == 0 ) {
		gLogAPIs = true;
	}
#if SUPPORT_ACCELERATE_TABLES
	else if ( strcmp(key, "DYLD_PRINT_APIS_APP") == 0 ) {
		gLogAppAPIs = true;
	}
#endif
	else if ( strcmp(key, "DYLD_PRINT_WARNINGS") == 0 ) {
		gLinkContext.verboseWarnings = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_RPATHS") == 0 ) {
		gLinkContext.verboseRPaths = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_INTERPOSING") == 0 ) {
		gLinkContext.verboseInterposing = true;
	}
	else if ( strcmp(key, "DYLD_PRINT_CODE_SIGNATURES") == 0 ) {
		gLinkContext.verboseCodeSignatures = true;
	}
	else if ( (strcmp(key, "DYLD_SHARED_REGION") == 0) && gLinkContext.allowEnvVarsSharedCache ) {
		if ( strcmp(value, "private") == 0 ) {
			gLinkContext.sharedRegionMode = ImageLoader::kUsePrivateSharedRegion;
		}
		else if ( strcmp(value, "avoid") == 0 ) {
			gLinkContext.sharedRegionMode = ImageLoader::kDontUseSharedRegion;
		}
		else if ( strcmp(value, "use") == 0 ) {
			gLinkContext.sharedRegionMode = ImageLoader::kUseSharedRegion;
		}
		else if ( value[0] == '\0' ) {
			gLinkContext.sharedRegionMode = ImageLoader::kUseSharedRegion;
		}
		else {
			dyld::warn("unknown option to DYLD_SHARED_REGION.  Valid options are: use, private, avoid\n");
		}
	}
	else if ( (strcmp(key, "DYLD_SHARED_CACHE_DIR") == 0) && gLinkContext.allowEnvVarsSharedCache  ) {
		sSharedCacheOverrideDir = value;
	}
	else if ( strcmp(key, "DYLD_USE_CLOSURES") == 0 ) {
		// Handled elsewhere
	}
	else if ( strcmp(key, "DYLD_SHARED_REGION_DATA_CONST") == 0 ) {
		// Handled elsewhere
	}
	else if ( strcmp(key, "DYLD_FORCE_INVALID_CACHE_CLOSURES") == 0 ) {
		if ( dyld3::internalInstall() ) {
			sForceInvalidSharedCacheClosureFormat = true;
		}
	}
	else if ( strcmp(key, "DYLD_IGNORE_PREBINDING") == 0 ) {
		if ( strcmp(value, "all") == 0 ) {
			gLinkContext.prebindUsage = ImageLoader::kUseNoPrebinding;
		}
		else if ( strcmp(value, "app") == 0 ) {
			gLinkContext.prebindUsage = ImageLoader::kUseAllButAppPredbinding;
		}
		else if ( strcmp(value, "nonsplit") == 0 ) {
			gLinkContext.prebindUsage = ImageLoader::kUseSplitSegPrebinding;
		}
		else if ( value[0] == '\0' ) {
			gLinkContext.prebindUsage = ImageLoader::kUseSplitSegPrebinding;
		}
		else {
			dyld::warn("unknown option to DYLD_IGNORE_PREBINDING.  Valid options are: all, app, nonsplit\n");
		}
	}
#if SUPPORT_VERSIONED_PATHS
	else if ( strcmp(key, "DYLD_VERSIONED_LIBRARY_PATH") == 0 ) {
		appendParsedColonList(value, mainExecutableDir, &sEnv.DYLD_VERSIONED_LIBRARY_PATH);
	#if SUPPORT_ACCELERATE_TABLES
		sDisableAcceleratorTables = true;
	#endif
		sEnv.hasOverride = true;
	}
	else if ( strcmp(key, "DYLD_VERSIONED_FRAMEWORK_PATH") == 0 ) {
		appendParsedColonList(value, mainExecutableDir, &sEnv.DYLD_VERSIONED_FRAMEWORK_PATH);
	#if SUPPORT_ACCELERATE_TABLES
		sDisableAcceleratorTables = true;
	#endif
		sEnv.hasOverride = true;
	}
#endif
#if !TARGET_OS_SIMULATOR
	else if ( (strcmp(key, "DYLD_PRINT_TO_FILE") == 0) && (mainExecutableDir == NULL) && gLinkContext.allowEnvVarsSharedCache ) {
		int fd = dyld3::open(value, O_WRONLY | O_CREAT | O_APPEND, 0644);
		if ( fd != -1 ) {
			sLogfile = fd;
			sLogToFile = true;
		}
		else {
			dyld::log("dyld: could not open DYLD_PRINT_TO_FILE='%s', errno=%d\n", value, errno);
		}
	}
	else if ( (strcmp(key, "DYLD_SKIP_MAIN") == 0)) {
		if ( dyld3::internalInstall() )
			sSkipMain = true;
	}
	else if ( (strcmp(key, "DYLD_JUST_BUILD_CLOSURE") == 0) ) {
		// handled elsewhere
	}
#endif
	else if (strcmp(key, "DYLD_FORCE_PLATFORM") == 0) {
		// handled elsewhere
	}
	else if (strcmp(key, "DYLD_AMFI_FAKE") == 0) {
		// handled elsewhere
	}
	else {
		dyld::warn("unknown environment variable: %s\n", key);
	}
}


#if SUPPORT_LC_DYLD_ENVIRONMENT
static void checkLoadCommandEnvironmentVariables()
{
	// <rdar://problem/8440934> Support augmenting dyld environment variables in load commands
	const uint32_t cmd_count = sMainExecutableMachHeader->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)sMainExecutableMachHeader)+sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_DYLD_ENVIRONMENT:
			{
				const struct dylinker_command* envcmd = (struct dylinker_command*)cmd;
				const char* keyEqualsValue = (char*)envcmd + envcmd->name.offset;
				char mainExecutableDir[strlen(sExecPath)+2];
				strcpy(mainExecutableDir, sExecPath);
				char* lastSlash = strrchr(mainExecutableDir, '/');
				if ( lastSlash != NULL)
					lastSlash[1] = '\0';
				// only process variables that start with DYLD_ and end in _PATH
				if ( (strncmp(keyEqualsValue, "DYLD_", 5) == 0) ) {
					const char* equals = strchr(keyEqualsValue, '=');
					if ( equals != NULL ) {
						if ( strncmp(&equals[-5], "_PATH", 5) == 0 ) {
							const char* value = &equals[1];
							const size_t keyLen = equals-keyEqualsValue;
							// <rdar://problem/22799635> don't let malformed load command overflow stack
							if ( keyLen < 40 ) {
								char key[keyLen+1];
								strncpy(key, keyEqualsValue, keyLen);
								key[keyLen] = '\0';
								//dyld::log("processing: %s\n", keyEqualsValue);
								//dyld::log("mainExecutableDir: %s\n", mainExecutableDir);
#if SUPPORT_ROOT_PATH
								if ( (strcmp(key, "DYLD_ROOT_PATH") == 0) || (strcmp(key, "DYLD_PATHS_ROOT") == 0) )
									continue;
#endif
								processDyldEnvironmentVariable(key, value, mainExecutableDir);
							}
						}
					}
				}
			}
			break;
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
}
#endif // SUPPORT_LC_DYLD_ENVIRONMENT	

	
static bool hasCodeSignatureLoadCommand(const macho_header* mh)
{
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		if (cmd->cmd == LC_CODE_SIGNATURE) 
			return true;
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
	return false;
}
	

#if SUPPORT_VERSIONED_PATHS
static void checkVersionedPaths()
{
	// search DYLD_VERSIONED_LIBRARY_PATH directories for dylibs and check if they are newer
	if ( sEnv.DYLD_VERSIONED_LIBRARY_PATH != NULL ) {
		for(const char* const* lp = sEnv.DYLD_VERSIONED_LIBRARY_PATH; *lp != NULL; ++lp) {
			checkDylibOverridesInDir(*lp);
		}
	}
	
	// search DYLD_VERSIONED_FRAMEWORK_PATH directories for dylibs and check if they are newer
	if ( sEnv.DYLD_VERSIONED_FRAMEWORK_PATH != NULL ) {
		for(const char* const* fp = sEnv.DYLD_VERSIONED_FRAMEWORK_PATH; *fp != NULL; ++fp) {
			checkFrameworkOverridesInDir(*fp);
		}
	}
}
#endif	


#if TARGET_OS_OSX
//
// For security, setuid programs ignore DYLD_* environment variables.
// Additionally, the DYLD_* enviroment variables are removed
// from the environment, so that any child processes don't see them.
//
static void pruneEnvironmentVariables(const char* envp[], const char*** applep)
{
#if SUPPORT_LC_DYLD_ENVIRONMENT
	checkLoadCommandEnvironmentVariables();
#endif

    // Are we testing dyld on an internal config?
    if ( _simple_getenv(envp, "DYLD_SKIP_MAIN") != NULL ) {
		if ( dyld3::internalInstall() )
            sSkipMain = true;
    }

	// delete all DYLD_* and LD_LIBRARY_PATH environment variables
	int removedCount = 0;
	const char** d = envp;
	for(const char** s = envp; *s != NULL; s++) {
		
	    if ( (strncmp(*s, "DYLD_", 5) != 0) && (strncmp(*s, "LD_LIBRARY_PATH=", 16) != 0) ) {
			*d++ = *s;
		}
		else {
			++removedCount;
		}
	}
	*d++ = NULL;
	// slide apple parameters
	if ( removedCount > 0 ) {
		*applep = d;
		do {
			*d = d[removedCount];
		} while ( *d++ != NULL );
		for(int i=0; i < removedCount; ++i)
			*d++ = NULL;
	}
	
	// disable framework and library fallback paths for setuid binaries rdar://problem/4589305
	sEnv.DYLD_FALLBACK_FRAMEWORK_PATH = NULL;
	sEnv.DYLD_FALLBACK_LIBRARY_PATH = NULL;

	if ( removedCount > 0 )
		strlcat(sLoadingCrashMessage, ", ignoring DYLD_* env vars", sizeof(sLoadingCrashMessage));
}
#endif

static void defaultUninitializedFallbackPaths(const char* envp[])
{
#if TARGET_OS_OSX
	if ( !gLinkContext.allowClassicFallbackPaths ) {
		sEnv.DYLD_FALLBACK_FRAMEWORK_PATH = sRestrictedFrameworkFallbackPaths;
		sEnv.DYLD_FALLBACK_LIBRARY_PATH   = sRestrictedLibraryFallbackPaths;
		return;
	}

	// default value for DYLD_FALLBACK_FRAMEWORK_PATH, if not set in environment
	const char* home = _simple_getenv(envp, "HOME");;
	if ( sEnv.DYLD_FALLBACK_FRAMEWORK_PATH == NULL ) {
		const char** fpaths = sFrameworkFallbackPaths;
		if ( home == NULL )
			removePathWithPrefix(fpaths, "$HOME");
		else
			paths_expand_roots(fpaths, "$HOME", home);
		sEnv.DYLD_FALLBACK_FRAMEWORK_PATH = fpaths;
	}

    // default value for DYLD_FALLBACK_LIBRARY_PATH, if not set in environment
	if ( sEnv.DYLD_FALLBACK_LIBRARY_PATH == NULL ) {
		const char** lpaths = sLibraryFallbackPaths;
		if ( home == NULL )
			removePathWithPrefix(lpaths, "$HOME");
		else
			paths_expand_roots(lpaths, "$HOME", home);
		sEnv.DYLD_FALLBACK_LIBRARY_PATH = lpaths;
	}
#else
	if ( sEnv.DYLD_FALLBACK_FRAMEWORK_PATH == NULL )
		sEnv.DYLD_FALLBACK_FRAMEWORK_PATH = sFrameworkFallbackPaths;

	if ( sEnv.DYLD_FALLBACK_LIBRARY_PATH == NULL )
		sEnv.DYLD_FALLBACK_LIBRARY_PATH = sLibraryFallbackPaths;
#endif
}


static void checkEnvironmentVariables(const char* envp[])
{
	if ( !gLinkContext.allowEnvVarsPath && !gLinkContext.allowEnvVarsPrint )
		return;
	const char** p;
	for(p = envp; *p != NULL; p++) {
		const char* keyEqualsValue = *p;
	    if ( strncmp(keyEqualsValue, "DYLD_", 5) == 0 ) {
			const char* equals = strchr(keyEqualsValue, '=');
			if ( equals != NULL ) {
				strlcat(sLoadingCrashMessage, "\n", sizeof(sLoadingCrashMessage));
				strlcat(sLoadingCrashMessage, keyEqualsValue, sizeof(sLoadingCrashMessage));
				const char* value = &equals[1];
				const size_t keyLen = equals-keyEqualsValue;
				char key[keyLen+1];
				strncpy(key, keyEqualsValue, keyLen);
				key[keyLen] = '\0';
				if ( (strncmp(key, "DYLD_PRINT_", 11) == 0) && !gLinkContext.allowEnvVarsPrint )
					continue;
				processDyldEnvironmentVariable(key, value, NULL);
			}
		}
		else if ( strncmp(keyEqualsValue, "LD_LIBRARY_PATH=", 16) == 0 ) {
			const char* path = &keyEqualsValue[16];
			sEnv.LD_LIBRARY_PATH = parseColonList(path, NULL);
		}
	}

#if SUPPORT_LC_DYLD_ENVIRONMENT
	checkLoadCommandEnvironmentVariables();
#endif // SUPPORT_LC_DYLD_ENVIRONMENT	
	
#if SUPPORT_ROOT_PATH
	// <rdar://problem/11281064> DYLD_IMAGE_SUFFIX and DYLD_ROOT_PATH cannot be used together
	if ( (gLinkContext.imageSuffix != NULL && *gLinkContext.imageSuffix != NULL) && (gLinkContext.rootPaths != NULL) ) {
		dyld::warn("Ignoring DYLD_IMAGE_SUFFIX because DYLD_ROOT_PATH is used.\n");
		gLinkContext.imageSuffix = NULL; // this leaks allocations from parseColonList
	}
#endif
}

#if __x86_64__ && !TARGET_OS_SIMULATOR
static bool isGCProgram(const macho_header* mh, uintptr_t slide)
{
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_SEGMENT_COMMAND:
			{
				const struct macho_segment_command* seg = (struct macho_segment_command*)cmd;
				if (strcmp(seg->segname, "__DATA") == 0) {
					const struct macho_section* const sectionsStart = (struct macho_section*)((char*)seg + sizeof(struct macho_segment_command));
					const struct macho_section* const sectionsEnd = &sectionsStart[seg->nsects];
					for (const struct macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
						if (strncmp(sect->sectname, "__objc_imageinfo", 16) == 0) {
							const uint32_t*  objcInfo = (uint32_t*)(sect->addr + slide);
							return (objcInfo[1] & 6); // 6 = (OBJC_IMAGE_SUPPORTS_GC | OBJC_IMAGE_REQUIRES_GC)
						}
					}
				}
			}
			break;
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
	return false;
}
#endif

static void getHostInfo(const macho_header* mainExecutableMH, uintptr_t mainExecutableSlide)
{
#if CPU_SUBTYPES_SUPPORTED
#if __ARM_ARCH_7K__
	sHostCPU		= CPU_TYPE_ARM;
	sHostCPUsubtype = CPU_SUBTYPE_ARM_V7K;
#elif __ARM_ARCH_7A__
	sHostCPU		= CPU_TYPE_ARM;
	sHostCPUsubtype = CPU_SUBTYPE_ARM_V7;
#elif __ARM_ARCH_6K__
	sHostCPU		= CPU_TYPE_ARM;
	sHostCPUsubtype = CPU_SUBTYPE_ARM_V6;
#elif __ARM_ARCH_7F__
	sHostCPU		= CPU_TYPE_ARM;
	sHostCPUsubtype = CPU_SUBTYPE_ARM_V7F;
#elif __ARM_ARCH_7S__
	sHostCPU		= CPU_TYPE_ARM;
	sHostCPUsubtype = CPU_SUBTYPE_ARM_V7S;
#elif __ARM64_ARCH_8_32__
	sHostCPU		= CPU_TYPE_ARM64_32;
	sHostCPUsubtype = CPU_SUBTYPE_ARM64_32_V8;
#elif __arm64e__
	sHostCPU		= CPU_TYPE_ARM64;
	sHostCPUsubtype = CPU_SUBTYPE_ARM64E;
#elif __arm64__
	sHostCPU		= CPU_TYPE_ARM64;
	sHostCPUsubtype = CPU_SUBTYPE_ARM64_V8;
#else
	struct host_basic_info info;
	mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
	mach_port_t hostPort = mach_host_self();
	kern_return_t result = host_info(hostPort, HOST_BASIC_INFO, (host_info_t)&info, &count);
	if ( result != KERN_SUCCESS )
		throw "host_info() failed";
	sHostCPU		= info.cpu_type;
	sHostCPUsubtype = info.cpu_subtype;
	mach_port_deallocate(mach_task_self(), hostPort);
  #if __x86_64__
	  // host_info returns CPU_TYPE_I386 even for x86_64.  Override that here so that
	  // we don't need to mask the cpu type later.
	  sHostCPU = CPU_TYPE_X86_64;
	#if !TARGET_OS_SIMULATOR
	  sHaswell = (sHostCPUsubtype == CPU_SUBTYPE_X86_64_H);
	  // <rdar://problem/18528074> x86_64h: Fall back to the x86_64 slice if an app requires GC.
	  if ( sHaswell ) {
		if ( isGCProgram(mainExecutableMH, mainExecutableSlide) ) {
			// When running a GC program on a haswell machine, don't use and 'h slices
			sHostCPUsubtype = CPU_SUBTYPE_X86_64_ALL;
			sHaswell = false;
			gLinkContext.sharedRegionMode = ImageLoader::kDontUseSharedRegion;
		}
	  }
	#endif
  #endif
#endif
#endif
}

static void checkSharedRegionDisable(const dyld3::MachOLoaded* mainExecutableMH, uintptr_t mainExecutableSlide)
{
#if TARGET_OS_OSX
	// if main executable has segments that overlap the shared region,
	// then disable using the shared region
	if ( mainExecutableMH->intersectsRange(SHARED_REGION_BASE, SHARED_REGION_SIZE) ) {
		gLinkContext.sharedRegionMode = ImageLoader::kDontUseSharedRegion;
		if ( gLinkContext.verboseMapping )
			dyld::warn("disabling shared region because main executable overlaps\n");
	}
#if __i386__
	if ( !gLinkContext.allowEnvVarsPath ) {
		// <rdar://problem/15280847> use private or no shared region for suid processes
		gLinkContext.sharedRegionMode = ImageLoader::kUsePrivateSharedRegion;
	}
#endif
#endif
#if TARGET_OS_SIMULATOR
	gLinkContext.sharedRegionMode = ImageLoader::kUsePrivateSharedRegion;
#endif
	// iOS cannot run without shared region
}

bool validImage(const ImageLoader* possibleImage)
{
    const size_t imageCount = sAllImages.size();
    for(size_t i=0; i < imageCount; ++i) {
        if ( possibleImage == sAllImages[i] ) {
            return true;
        }
    }
	return false;
}

uint32_t getImageCount()
{
	return (uint32_t)sAllImages.size();
}

ImageLoader* getIndexedImage(unsigned int index)
{
	if ( index < sAllImages.size() )
		return sAllImages[index];
	return NULL;
}

ImageLoader* findImageByMachHeader(const struct mach_header* target)
{
	return findMappedRange((uintptr_t)target);
}


ImageLoader* findImageContainingAddress(const void* addr)
{
  #if SUPPORT_ACCELERATE_TABLES
	if ( sAllCacheImagesProxy != NULL ) {
		const mach_header* mh;
		const char* path;
		unsigned index;
		if ( sAllCacheImagesProxy->addressInCache(addr, &mh, &path, &index) )
			return sAllCacheImagesProxy;
	}
  #endif
	return findMappedRange((uintptr_t)addr);
}


ImageLoader* findImageContainingSymbol(const void* symbol)
{
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
		ImageLoader* anImage = *it;
		if ( anImage->containsSymbol(symbol) )
			return anImage;
	}
	return NULL;
}



void forEachImageDo( void (*callback)(ImageLoader*, void* userData), void* userData)
{
	const size_t imageCount = sAllImages.size();
	for(size_t i=0; i < imageCount; ++i) {
		ImageLoader* anImage = sAllImages[i];
		(*callback)(anImage, userData);
	}
}

ImageLoader* findLoadedImage(const struct stat& stat_buf)
{
	const size_t imageCount = sAllImages.size();
	for(size_t i=0; i < imageCount; ++i){
		ImageLoader* anImage = sAllImages[i];
		if ( anImage->statMatch(stat_buf) )
			return anImage;
	}
	return NULL;
}

// based on ANSI-C strstr()
static const char* strrstr(const char* str, const char* sub) 
{
	const size_t sublen = strlen(sub);
	for(const char* p = &str[strlen(str)]; p != str; --p) {
		if ( strncmp(p, sub, sublen) == 0 )
			return p;
	}
	return NULL;
}


//
// Find framework path
//
//  /path/foo.framework/foo								=>   foo.framework/foo	
//  /path/foo.framework/Versions/A/foo					=>   foo.framework/Versions/A/foo
//  /path/foo.framework/Frameworks/bar.framework/bar	=>   bar.framework/bar
//  /path/foo.framework/Libraries/bar.dylb				=>   NULL
//  /path/foo.framework/bar								=>   NULL
//
// Returns NULL if not a framework path
//
static const char* getFrameworkPartialPath(const char* path)
{
	const char* dirDot = strrstr(path, ".framework/");
	if ( dirDot != NULL ) {
		const char* dirStart = dirDot;
		for ( ; dirStart >= path; --dirStart) {
			if ( (*dirStart == '/') || (dirStart == path) ) {
				const char* frameworkStart = &dirStart[1];
				if ( dirStart == path )
					--frameworkStart;
				size_t len = dirDot - frameworkStart;
				char framework[len+1];
				strncpy(framework, frameworkStart, len);
				framework[len] = '\0';
				const char* leaf = strrchr(path, '/');
				if ( leaf != NULL ) {
					if ( strcmp(framework, &leaf[1]) == 0 ) {
						return frameworkStart;
					}
					if (  gLinkContext.imageSuffix != NULL ) {
						// some debug frameworks have install names that end in _debug
						if ( strncmp(framework, &leaf[1], len) == 0 ) {
							for (const char* const* suffix=gLinkContext.imageSuffix; *suffix != NULL; ++suffix) {
								if ( strcmp(*suffix, &leaf[len+1]) == 0 )
									return frameworkStart;
							}
						}
					}
				}
			}
		}
	}
	return NULL;
}


static const char* getLibraryLeafName(const char* path)
{
	const char* start = strrchr(path, '/');
	if ( start != NULL )
		return &start[1];
	else
		return path;
}


// only for architectures that use cpu-sub-types
#if CPU_SUBTYPES_SUPPORTED 

const cpu_subtype_t CPU_SUBTYPE_END_OF_LIST = -1;


//
//	A fat file may contain multiple sub-images for the same CPU type.
//	In that case, dyld picks which sub-image to use by scanning a table
//	of preferred cpu-sub-types for the running cpu.  
//	
//	There is one row in the table for each cpu-sub-type on which dyld might run.
//  The first entry in a row is that cpu-sub-type.  It is followed by all
//	cpu-sub-types that can run on that cpu, if preferred order.  Each row ends with 
//	a "SUBTYPE_ALL" (to denote that images written to run on any cpu-sub-type are usable), 
//  followed by one or more CPU_SUBTYPE_END_OF_LIST to pad out this row.
//


#if __arm__
//      
//     ARM sub-type lists
//
const int kARM_RowCount = 8;
static const cpu_subtype_t kARM[kARM_RowCount][9] = { 

	// armv7f can run: v7f, v7, v6, v5, and v4
	{  CPU_SUBTYPE_ARM_V7F, CPU_SUBTYPE_ARM_V7, CPU_SUBTYPE_ARM_V6, CPU_SUBTYPE_ARM_V5TEJ, CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST },

	// armv7k can run: v7k
	{  CPU_SUBTYPE_ARM_V7K, CPU_SUBTYPE_END_OF_LIST },

	// armv7s can run: v7s, v7, v7f, v7k, v6, v5, and v4
	{  CPU_SUBTYPE_ARM_V7S, CPU_SUBTYPE_ARM_V7, CPU_SUBTYPE_ARM_V7F, CPU_SUBTYPE_ARM_V6, CPU_SUBTYPE_ARM_V5TEJ, CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST },

	// armv7 can run: v7, v6, v5, and v4
	{  CPU_SUBTYPE_ARM_V7, CPU_SUBTYPE_ARM_V6, CPU_SUBTYPE_ARM_V5TEJ, CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST },
	
	// armv6 can run: v6, v5, and v4
	{  CPU_SUBTYPE_ARM_V6, CPU_SUBTYPE_ARM_V5TEJ, CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST },
	
	// xscale can run: xscale, v5, and v4
	{  CPU_SUBTYPE_ARM_XSCALE, CPU_SUBTYPE_ARM_V5TEJ, CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST },
	
	// armv5 can run: v5 and v4
	{  CPU_SUBTYPE_ARM_V5TEJ, CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST },

	// armv4 can run: v4
	{  CPU_SUBTYPE_ARM_V4T, CPU_SUBTYPE_ARM_ALL, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST, CPU_SUBTYPE_END_OF_LIST },
};
#endif

#if __ARM64_ARCH_8_32__
//
//    arm64_32 sub-type lists
//
	static const cpu_subtype_t kARM64_32[] = { CPU_SUBTYPE_ARM64_32_V8, CPU_SUBTYPE_END_OF_LIST };
#endif

#if __arm64__ && __LP64__
//
//     arm64[e] sub-type handing
//
	#if __arm64e__
		// arm64e with keys on
		static const cpu_subtype_t kARM64e[]        = { CPU_SUBTYPE_ARM64E, CPU_SUBTYPE_END_OF_LIST };
		// arm64 or arm64e with keys off
		static const cpu_subtype_t kARM64eKeysOff[] = { CPU_SUBTYPE_ARM64E, CPU_SUBTYPE_ARM64_V8, CPU_SUBTYPE_ARM64_ALL, CPU_SUBTYPE_END_OF_LIST };
	#else
		// arm64 main binary
		static const cpu_subtype_t kARM64[] = { CPU_SUBTYPE_ARM64_V8, CPU_SUBTYPE_ARM64_ALL, CPU_SUBTYPE_END_OF_LIST };
	#endif // __arm64e__
#endif


#if __x86_64__
//      
//     x86_64 sub-type lists
//
const int kX86_64_RowCount = 2;
static const cpu_subtype_t kX86_64[kX86_64_RowCount][5] = {

	// x86_64h can run: x86_64h, x86_64h(lib), x86_64(lib), and x86_64
	{ CPU_SUBTYPE_X86_64_H, (cpu_subtype_t)(CPU_SUBTYPE_LIB64|CPU_SUBTYPE_X86_64_H), (cpu_subtype_t)(CPU_SUBTYPE_LIB64|CPU_SUBTYPE_X86_64_ALL), CPU_SUBTYPE_X86_64_ALL,  CPU_SUBTYPE_END_OF_LIST },

	// x86_64 can run: x86_64(lib) and x86_64
	{ CPU_SUBTYPE_X86_64_ALL, (cpu_subtype_t)(CPU_SUBTYPE_LIB64|CPU_SUBTYPE_X86_64_ALL), CPU_SUBTYPE_END_OF_LIST },

};
#endif


// scan the tables above to find the cpu-sub-type-list for this machine
static const cpu_subtype_t* findCPUSubtypeList(cpu_type_t cpu, cpu_subtype_t subtype)
{
	switch (cpu) {
#if __arm__
		case CPU_TYPE_ARM:
			for (int i=0; i < kARM_RowCount ; ++i) {
				if ( kARM[i][0] == subtype )
					return kARM[i];
			}
			break;
#endif
#if __arm64__
	#if __LP64__
		case CPU_TYPE_ARM64:
		#if __arm64e__
			return ( sKeysDisabled ? kARM64eKeysOff : kARM64e);
		#else
			return kARM64;
		#endif
			break;
	#endif

	#if !__LP64__
		case CPU_TYPE_ARM64_32:
			return kARM64_32;
	#endif

#endif
#if __x86_64__
		case CPU_TYPE_X86_64:
			for (int i=0; i < kX86_64_RowCount ; ++i) {
				if ( kX86_64[i][0] == subtype )
					return kX86_64[i];
			}
			break;
#endif
	}
	return NULL;
}


// scan fat table-of-contents for best most preferred subtype
static bool fatFindBestFromOrderedList(cpu_type_t cpu, const cpu_subtype_t list[], const fat_header* fh, int fd, uint64_t* offset, uint64_t* len)
{
	const fat_arch* const archs = (fat_arch*)(((char*)fh)+sizeof(fat_header));
	for (uint32_t subTypeIndex=0; list[subTypeIndex] != CPU_SUBTYPE_END_OF_LIST; ++subTypeIndex) {
		for(uint32_t fatIndex=0; fatIndex < OSSwapBigToHostInt32(fh->nfat_arch); ++fatIndex) {
			cpu_type_t    sliceCpuType    = OSSwapBigToHostInt32(archs[fatIndex].cputype);
			cpu_subtype_t sliceCpuSubType = OSSwapBigToHostInt32(archs[fatIndex].cpusubtype) & ~CPU_SUBTYPE_MASK;
			uint64_t      sliceOffset     = OSSwapBigToHostInt32(archs[fatIndex].offset);
			uint64_t      sliceLen        = OSSwapBigToHostInt32(archs[fatIndex].size);
			if ( (sliceCpuType == cpu) && ((list[subTypeIndex] & ~CPU_SUBTYPE_MASK) == sliceCpuSubType) ) {
#if TARGET_OS_OSX && __has_feature(ptrauth_calls)
				if ( sOnlyPlatformArm64e && (sliceCpuType == CPU_TYPE_ARM64) && (sliceCpuSubType == CPU_SUBTYPE_ARM64E) ) {
					// if we can only load arm64e slices that are platform binaries, skip over slices that are not
					if ( !dyld3::MachOAnalyzer::sliceIsOSBinary(fd, sliceOffset, sliceLen) )
						continue;
				}
#endif
				*offset = sliceOffset;
				*len    = sliceLen;
				return true;
			}
		}
	}
	return false;
}

#if !TARGET_OS_OSX || !__has_feature(ptrauth_calls)
// scan fat table-of-contents for exact match of cpu and cpu-sub-type
static bool fatFindExactMatch(cpu_type_t cpu, cpu_subtype_t subtype, const fat_header* fh, uint64_t* offset, uint64_t* len)
{
	const fat_arch* archs = (fat_arch*)(((char*)fh)+sizeof(fat_header));
	for(uint32_t i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
		if ( ((cpu_type_t)OSSwapBigToHostInt32(archs[i].cputype) == cpu)
			&& ((cpu_subtype_t)OSSwapBigToHostInt32(archs[i].cpusubtype) == subtype) ) {
			*offset = OSSwapBigToHostInt32(archs[i].offset);
			*len = OSSwapBigToHostInt32(archs[i].size);
			return true;
		}
	}
	return false;
}
#endif

// scan fat table-of-contents for image with matching cpu-type and runs-on-all-sub-types
static bool fatFindRunsOnAllCPUs(cpu_type_t cpu, const fat_header* fh, uint64_t* offset, uint64_t* len)
{
	const fat_arch* archs = (fat_arch*)(((char*)fh)+sizeof(fat_header));
	for(uint32_t i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
		if ( (cpu_type_t)OSSwapBigToHostInt32(archs[i].cputype) == cpu) {
			switch (cpu) {
#if __arm__
				case CPU_TYPE_ARM:
					if ( (cpu_subtype_t)OSSwapBigToHostInt32(archs[i].cpusubtype) == CPU_SUBTYPE_ARM_ALL ) {
						*offset = OSSwapBigToHostInt32(archs[i].offset);
						*len = OSSwapBigToHostInt32(archs[i].size);
						return true;
					}
					break;
#endif
#if __arm64__
				case CPU_TYPE_ARM64:
					if ( (cpu_subtype_t)OSSwapBigToHostInt32(archs[i].cpusubtype) == CPU_SUBTYPE_ARM64_ALL ) {
						*offset = OSSwapBigToHostInt32(archs[i].offset);
						*len = OSSwapBigToHostInt32(archs[i].size);
						return true;
					}
					break;
#endif
#if __x86_64__
				case CPU_TYPE_X86_64:
					if ( (cpu_subtype_t)OSSwapBigToHostInt32(archs[i].cpusubtype) == CPU_SUBTYPE_X86_64_ALL ) {
						*offset = OSSwapBigToHostInt32(archs[i].offset);
						*len = OSSwapBigToHostInt32(archs[i].size);
						return true;
					}
					break;
#endif
			}
		}
	}
	return false;
}

#endif // CPU_SUBTYPES_SUPPORTED


//
// Validate the fat_header and fat_arch array:
//
// 1) arch count would not cause array to extend past 4096 byte read buffer
// 2) no slice overlaps the fat_header and arch array
// 3) arch list does not contain duplicate cputype/cpusubtype tuples
// 4) arch list does not have two overlapping slices.
//
static bool fatValidate(const fat_header* fh)
{
	if ( fh->magic != OSSwapBigToHostInt32(FAT_MAGIC) )
		return false;

	// since only first 4096 bytes of file read, we can only handle up to 204 slices.
	const uint32_t sliceCount = OSSwapBigToHostInt32(fh->nfat_arch);
	if ( sliceCount > 204 )
		return false;

	// compare all slices looking for conflicts
	const fat_arch* archs = (fat_arch*)(((char*)fh)+sizeof(fat_header));
	for (uint32_t i=0; i < sliceCount; ++i) {
		uint32_t i_offset     = OSSwapBigToHostInt32(archs[i].offset);
		uint32_t i_size       = OSSwapBigToHostInt32(archs[i].size);
		uint32_t i_cputype    = OSSwapBigToHostInt32(archs[i].cputype);
		uint32_t i_cpusubtype = OSSwapBigToHostInt32(archs[i].cpusubtype);
		uint32_t i_end        = i_offset + i_size;
		// slice cannot overlap with header
		if ( i_offset < 4096 )
			return false;
		// slice size cannot overflow
		if ( i_end < i_offset )
			return false;
		for (uint32_t j=i+1; j < sliceCount; ++j) {
			uint32_t j_offset     = OSSwapBigToHostInt32(archs[j].offset);
			uint32_t j_size       = OSSwapBigToHostInt32(archs[j].size);
			uint32_t j_cputype    = OSSwapBigToHostInt32(archs[j].cputype);
			uint32_t j_cpusubtype = OSSwapBigToHostInt32(archs[j].cpusubtype);
			uint32_t j_end        = j_offset + j_size;
			// duplicate slices types not allowed
			if ( (i_cputype == j_cputype) && (i_cpusubtype == j_cpusubtype) )
				return false;
			// slice size cannot overflow
			if ( j_end < j_offset )
				return false;
			// check for overlap of slices
			if ( i_offset <= j_offset ) {
				if ( j_offset < i_end )
					return false; //  j overlaps end of i
			}
			else {
				//  j overlaps end of i
				if ( i_offset < j_end )
					return false;  //  i overlaps end of j
			}
		}
	}
	return true;
}

//
// A fat file may contain multiple sub-images for the same cpu-type,
// each optimized for a different cpu-sub-type (e.g G3 or G5).
// This routine picks the optimal sub-image.
//
static bool fatFindBest(const fat_header* fh, uint64_t* offset, uint64_t* len, int fd=-1)
{
	if ( !fatValidate(fh) )
		return false;

#if CPU_SUBTYPES_SUPPORTED
	// assume all dylibs loaded must have same cpu type as main executable
	const cpu_type_t cpu = sMainExecutableMachHeader->cputype;

	// We only know the subtype to use if the main executable cpu type matches the host
	if ( cpu == sHostCPU ) {
		// get preference ordered list of subtypes
		const cpu_subtype_t* subTypePreferenceList = findCPUSubtypeList(cpu, sHostCPUsubtype);
	
		// use ordered list to find best sub-image in fat file
		if ( subTypePreferenceList != NULL ) {
			if ( fatFindBestFromOrderedList(cpu, subTypePreferenceList, fh, fd, offset, len) )
				return true;
		}
#if TARGET_OS_OSX && __has_feature(ptrauth_calls)
		// don't use fallbacks for macOS arm64e to ensure only compatible binaries are loaded
		return false;
#else
		// if running cpu is not in list, try for an exact match
		if ( fatFindExactMatch(cpu, sHostCPUsubtype, fh, offset, len) )
			return true;
#endif
	}
	
	// running on an uknown cpu, can only load generic code
	return fatFindRunsOnAllCPUs(cpu, fh, offset, len);
#else
	// just find first slice with matching architecture
	const fat_arch* archs = (fat_arch*)(((char*)fh)+sizeof(fat_header));
	for(uint32_t i=0; i < OSSwapBigToHostInt32(fh->nfat_arch); ++i) {
		if ( (cpu_type_t)OSSwapBigToHostInt32(archs[i].cputype) == sMainExecutableMachHeader->cputype) {
			*offset = OSSwapBigToHostInt32(archs[i].offset);
			*len = OSSwapBigToHostInt32(archs[i].size);
			return true;
		}
	}
	return false;
#endif
}

#if defined(__x86_64__) && !TARGET_OS_SIMULATOR
#ifndef kIsTranslated
   #define kIsTranslated  0x4000000000000000ULL
#endif
bool isTranslated()
{
	return ((*(uint64_t*)_COMM_PAGE_CPU_CAPABILITIES64) & kIsTranslated);
}
#endif


//
// This is used to validate if a non-fat (aka thin or raw) mach-o file can be used
// on the current processor. //
bool isCompatibleMachO(const uint8_t* firstPage, const char* path, int fd=-1, uint64_t sliceOffset=0, uint64_t sliceLen=-1)
{
#if CPU_SUBTYPES_SUPPORTED
	// It is deemed compatible if any of the following are true:
	//  1) mach_header subtype is in list of compatible subtypes for running processor
	//  2) mach_header subtype is same as running processor subtype
	//  3) mach_header subtype runs on all processor variants
	const mach_header* mh = (mach_header*)firstPage;
	if ( mh->magic == sMainExecutableMachHeader->magic ) {
		if ( mh->cputype == sMainExecutableMachHeader->cputype ) {
			if ( mh->cputype == sHostCPU ) {
				const cpu_subtype_t mhCPUSubtype = mh->cpusubtype & ~CPU_SUBTYPE_MASK;
				// get preference ordered list of subtypes that this machine can use
				const cpu_subtype_t* subTypePreferenceList = findCPUSubtypeList(mh->cputype, sHostCPUsubtype);
				if ( subTypePreferenceList != NULL ) {
					// if image's subtype is in the list, it is compatible
					for (const cpu_subtype_t* p = subTypePreferenceList; *p != CPU_SUBTYPE_END_OF_LIST; ++p) {
						if ( *p == mhCPUSubtype ) {
	#if TARGET_OS_OSX && __has_feature(ptrauth_calls)
							if ( mhCPUSubtype == CPU_SUBTYPE_ARM64E ) {
								if ( !sOnlyPlatformArm64e || dyld3::MachOAnalyzer::sliceIsOSBinary(fd, sliceOffset, sliceLen) )
									return true;
							}
							else
	#endif
							return true;
						}
					}
					// have list and not in list, so not compatible
					throwf("incompatible cpu-subtype: 0x%08X in %s", mhCPUSubtype, path);
				}
				// unknown cpu sub-type, but if exact match for current subtype then ok to use
				if ( mhCPUSubtype == sHostCPUsubtype )
					return true;
			}

			// cpu type has no ordered list of subtypes
			switch (mh->cputype) {
				case CPU_TYPE_I386:
				case CPU_TYPE_X86_64:
					// subtypes are not used or these architectures
					return true;
			}
		}
	}
#else
	// For architectures that don't support cpu-sub-types
	// this just check the cpu type.
	const mach_header* mh = (mach_header*)firstPage;
	if ( mh->magic == sMainExecutableMachHeader->magic ) {
		if ( mh->cputype == sMainExecutableMachHeader->cputype ) {
			return true;
		}
	}
#endif
	return false;
}




// The kernel maps in main executable before dyld gets control.  We need to 
// make an ImageLoader* for the already mapped in main executable.
static ImageLoaderMachO* instantiateFromLoadedImage(const macho_header* mh, uintptr_t slide, const char* path)
{
	// try mach-o loader
//	if ( isCompatibleMachO((const uint8_t*)mh, path) ) {
		ImageLoader* image = ImageLoaderMachO::instantiateMainExecutable(mh, slide, path, gLinkContext);
		addImage(image);
		return (ImageLoaderMachO*)image;
//	}
	
//	throw "main executable not a known format";
}

#if SUPPORT_ACCELERATE_TABLES
static bool dylibsCanOverrideCache()
{
	if ( !dyld3::internalInstall() )
		return false;
	return ( (sSharedCacheLoadInfo.loadAddress != nullptr) && (sSharedCacheLoadInfo.loadAddress->header.cacheType == kDyldSharedCacheTypeDevelopment) );
}
#endif

const void* imMemorySharedCacheHeader()
{
	return sSharedCacheLoadInfo.loadAddress;
}


const char* getStandardSharedCacheFilePath()
{
	if ( sSharedCacheLoadInfo.loadAddress != nullptr )
		return sSharedCacheLoadInfo.path;
	else
		return nullptr;
}

bool hasInsertedOrInterposingLibraries() {
	return (sInsertedDylibCount > 0) || ImageLoader::haveInterposingTuples();
}


#if SUPPORT_VERSIONED_PATHS
static bool findInSharedCacheImage(const char* path, bool searchByPath, const struct stat* stat_buf, const macho_header** mh, const char** pathInCache, long* slide)
{
	dyld3::SharedCacheFindDylibResults results;
	if ( dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, path, &results) ) {
		*mh			 = (macho_header*)results.mhInCache;
		*pathInCache = results.pathInCache;
		*slide	     = results.slideInCache;
		return true;
	}
	return false;
}
#endif

bool inSharedCache(const char* path)
{
	return dyld3::pathIsInSharedCacheImage(sSharedCacheLoadInfo, path);
}


static ImageLoader* checkandAddImage(ImageLoader* image, const LoadContext& context)
{
	// now sanity check that this loaded image does not have the same install path as any existing image
	const char* loadedImageInstallPath = image->getInstallPath();
	if ( image->isDylib() && (loadedImageInstallPath != NULL) && (loadedImageInstallPath[0] == '/') ) {
		for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
			ImageLoader* anImage = *it;
			const char* installPath = anImage->getInstallPath();
			if ( installPath != NULL) {
				if ( strcmp(loadedImageInstallPath, installPath) == 0 ) {
					//dyld::log("duplicate(%s) => %p\n", installPath, anImage);
					removeImage(image);
					ImageLoader::deleteImage(image);
					return anImage;
				}
			}
		}
	}

	// some API's restrict what they can load
	if ( context.mustBeBundle && !image->isBundle() )
		throw "not a bundle";
	if ( context.mustBeDylib && !image->isDylib() )
		throw "not a dylib";

	// regular main executables cannot be loaded 
	if ( image->isExecutable() ) {
		if ( !context.canBePIE || !image->isPositionIndependentExecutable() )
			throw "can't load a main executable";
	}
	
	// don't add bundles to global list, they can be loaded but not linked.  When linked it will be added to list
	if ( ! image->isBundle() ) 
		addImage(image);
	
	return image;
}

#if TARGET_OS_SIMULATOR	
static bool isSimulatorBinary(const uint8_t* firstPages, const char* path)
{
	const macho_header* mh = (macho_header*)firstPages;
	const uint32_t cmd_count = mh->ncmds;
	const load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const load_command* const cmdsEnd = (load_command*)((char*)cmds + mh->sizeofcmds);
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
	#if TARGET_OS_WATCH
			case LC_VERSION_MIN_WATCHOS:
				return true;
	#elif TARGET_OS_TV
			case LC_VERSION_MIN_TVOS:
				return true;
	#elif TARGET_OS_IOS
			case LC_VERSION_MIN_IPHONEOS:
				return true;
	#endif
			case LC_VERSION_MIN_MACOSX:
				// grandfather in a few libSystem dylibs
				if ((strcmp(path, "/usr/lib/system/libsystem_kernel.dylib") == 0) ||
				    (strcmp(path, "/usr/lib/system/libsystem_platform.dylib") == 0) ||
				    (strcmp(path, "/usr/lib/system/libsystem_pthread.dylib") == 0) ||
				    (strcmp(path, "/usr/lib/system/libsystem_platform_debug.dylib") == 0) ||
				    (strcmp(path, "/usr/lib/system/libsystem_pthread_debug.dylib") == 0) ||
				    (strcmp(path, "/sbin/launchd_sim_trampoline") == 0) ||
				    (strcmp(path, "/usr/sbin/iokitsimd") == 0) ||
					(strcmp(path, "/usr/lib/system/host/liblaunch_sim.dylib") == 0))
					return true;
				return false;
			case LC_BUILD_VERSION:
			{
				// Same logic as above, but for LC_BUILD_VERSION instead of legacy load commands
				const struct build_version_command* buildVersionCmd = (build_version_command*)cmd;
				switch(buildVersionCmd->platform) {
					case PLATFORM_IOSSIMULATOR:
					case PLATFORM_TVOSSIMULATOR:
					case PLATFORM_WATCHOSSIMULATOR:
					case PLATFORM_WATCHOS:
						return true;
					case PLATFORM_MACOS:
						if ((strcmp(path, "/usr/lib/system/libsystem_kernel.dylib") == 0) ||
							(strcmp(path, "/usr/lib/system/libsystem_platform.dylib") == 0) ||
							(strcmp(path, "/usr/lib/system/libsystem_pthread.dylib") == 0) ||
							(strcmp(path, "/usr/lib/system/libsystem_platform_debug.dylib") == 0) ||
							(strcmp(path, "/usr/lib/system/libsystem_pthread_debug.dylib") == 0) ||
							(strcmp(path, "/sbin/launchd_sim_trampoline") == 0) ||
							(strcmp(path, "/usr/sbin/iokitsimd") == 0) ||
							(strcmp(path, "/usr/lib/system/host/liblaunch_sim.dylib") == 0))
							return true;
				}
			}
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
		if ( cmd > cmdsEnd )
			return false;
	}
	return false;
}
#endif


// map in file and instantiate an ImageLoader
static ImageLoader* loadPhase6(int fd, const struct stat& stat_buf, const char* path, const LoadContext& context)
{
	//dyld::log("%s(%s)\n", __func__ , path);
	uint64_t fileOffset = 0;
	uint64_t fileLength = stat_buf.st_size;

	// validate it is a file (not directory)
	if ( (stat_buf.st_mode & S_IFMT) != S_IFREG ) 
		throw "not a file";

	uint8_t firstPages[MAX_MACH_O_HEADER_AND_LOAD_COMMANDS_SIZE];
	bool shortPage = false;
	
	// min mach-o file is 4K
	if ( fileLength < 4096 ) {
		if ( pread(fd, firstPages, (size_t)fileLength, 0) != (ssize_t)fileLength )
			throwf("pread of short file failed: %d", errno);
		shortPage = true;
	} 
	else {
		// optimistically read only first 4KB
		if ( pread(fd, firstPages, 4096, 0) != 4096 )
			throwf("pread of first 4K failed: %d", errno);
	}
	
	// if fat wrapper, find usable sub-file
	const fat_header* fileStartAsFat = (fat_header*)firstPages;
	if ( fileStartAsFat->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		if ( OSSwapBigToHostInt32(fileStartAsFat->nfat_arch) > ((4096 - sizeof(fat_header)) / sizeof(fat_arch)) )
			throwf("fat header too large: %u entries", OSSwapBigToHostInt32(fileStartAsFat->nfat_arch));
		if ( fatFindBest(fileStartAsFat, &fileOffset, &fileLength, fd) ) {
			if ( (fileOffset+fileLength) > (uint64_t)(stat_buf.st_size) )
				throwf("truncated fat file.  file length=%llu, but needed slice goes to %llu", stat_buf.st_size, fileOffset+fileLength);
			if (pread(fd, firstPages, 4096, fileOffset) != 4096)
				throwf("pread of fat file failed: %d", errno);
		}
		else {
			throw "no matching architecture in universal wrapper";
		}
	}
	
	// try mach-o loader
	if ( shortPage ) 
		throw "file too short";

	if ( isCompatibleMachO(firstPages, path, fd, fileOffset, fileLength) ) {

		// only MH_BUNDLE, MH_DYLIB, and some MH_EXECUTE can be dynamically loaded
		const mach_header* mh = (mach_header*)firstPages;
		switch ( mh->filetype ) {
			case MH_EXECUTE:
			case MH_DYLIB:
			case MH_BUNDLE:
				break;
			default:
				throw "mach-o, but wrong filetype";
		}

		uint32_t headerAndLoadCommandsSize = sizeof(macho_header) + mh->sizeofcmds;
		if ( headerAndLoadCommandsSize > fileLength )
			dyld::throwf("malformed mach-o: load commands size (%u) > mach-o file size (%llu)", headerAndLoadCommandsSize, fileLength);

		vm_address_t vmAllocatedFirstPages = 0;
		if ( headerAndLoadCommandsSize > MAX_MACH_O_HEADER_AND_LOAD_COMMANDS_SIZE ) {
			if ( ::vm_allocate(mach_task_self(), &vmAllocatedFirstPages, headerAndLoadCommandsSize, VM_FLAGS_ANYWHERE) == 0 ) {
				if ( ::pread(fd, (void*)vmAllocatedFirstPages, headerAndLoadCommandsSize, fileOffset) != headerAndLoadCommandsSize )
					throwf("pread of all load commands failed: %d", errno);
				mh = (mach_header*)vmAllocatedFirstPages;
			}
			else {
				throwf("malformed mach-o: load commands size (%u) > %u", headerAndLoadCommandsSize, MAX_MACH_O_HEADER_AND_LOAD_COMMANDS_SIZE);
			}
		}
		else if ( headerAndLoadCommandsSize > 4096 ) {
			// read more pages
			unsigned readAmount = headerAndLoadCommandsSize - 4096;
			if ( pread(fd, &firstPages[4096], readAmount, fileOffset+4096) != readAmount )
				throwf("pread of extra load commands past 4KB failed: %d", errno);
		}

		if ( !((dyld3::MachOFile*)mh)->loadableIntoProcess((dyld3::Platform)gProcessInfo->platform, path) ) {
			throwf("mach-o, but not built for platform %s", dyld3::MachOFile::platformName((dyld3::Platform)gProcessInfo->platform));
		}

#if __has_feature(ptrauth_calls)
		if ( !sKeysDisabled && ((sMainExecutableMachHeader->cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E) && ((mh->cpusubtype & ~CPU_SUBTYPE_MASK) != CPU_SUBTYPE_ARM64E) )
			throw "arm64 dylibs cannot be loaded into arm64e processes";
#endif
		ImageLoader* image = nullptr;
		{
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_MAP_IMAGE, path, 0, 0);
			image = ImageLoaderMachO::instantiateFromFile(path, fd, (uint8_t*)mh, headerAndLoadCommandsSize, fileOffset, fileLength, stat_buf, gLinkContext);
			timer.setData4((uint64_t)image->machHeader());
		}

		if ( vmAllocatedFirstPages != 0 )
			::vm_deallocate(mach_task_self(), (vm_address_t)vmAllocatedFirstPages, headerAndLoadCommandsSize);

		// validate
		return checkandAddImage(image, context);
	}
	
	// try other file formats here...
	
	
	// throw error about what was found
	switch (*(uint32_t*)firstPages) {
		case MH_MAGIC:
		case MH_CIGAM:
		case MH_MAGIC_64:
		case MH_CIGAM_64:
			throw "mach-o, but wrong architecture";
		default:
		throwf("unknown file type, first eight bytes: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", 
			firstPages[0], firstPages[1], firstPages[2], firstPages[3], firstPages[4], firstPages[5], firstPages[6],firstPages[7]);
	}
}


static ImageLoader* loadPhase5open(const char* path, const LoadContext& context, const struct stat& stat_buf, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);

	// open file (automagically closed when this function exits)
	FileOpener file(path);
		
	// just return NULL if file not found, but record any other errors
	if ( file.getFileDescriptor() == -1 ) {
		int err = errno;
		if ( err != ENOENT ) {
			const char* newMsg;
			if ( (err == EPERM) && sandboxBlockedOpen(path) )
				newMsg = dyld::mkstringf("file system sandbox blocked open() of '%s'", path);
			else
				newMsg = dyld::mkstringf("%s: open() failed with errno=%d", path, err);
			exceptions->push_back(newMsg);
		}
		return NULL;
	}

	try {
		return loadPhase6(file.getFileDescriptor(), stat_buf, path, context);
	}
	catch (const char* msg) {
		const char* newMsg = dyld::mkstringf("%s: %s", path, msg);
		exceptions->push_back(newMsg);
		free((void*)msg);
		return NULL;
	}
}

static bool isFileRelativePath(const char* path)
{
	if ( path[0] == '/' )
		return false;
	if ( path[0] != '.' )
		return true;
	if ( path[1] == '/' )
		return true;
	if ( (path[1] == '.') && (path[2] == '/') )
		return true;
	return false;
}

static ImageLoader* loadPhase5check(const char* path, const char* orgPath, const LoadContext& context);


// try to open file
static ImageLoader* loadPhase5load(const char* path, const char* orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);

	// <rdar://problem/47682983> don't allow file system relative paths in hardened programs
	if ( (exceptions != NULL) &&  !gLinkContext.allowEnvVarsPath && isFileRelativePath(path) ) {
		exceptions->push_back("file system relative paths not allowed in hardened programs");
		return NULL;
	}

#if SUPPORT_ACCELERATE_TABLES
	if ( sAllCacheImagesProxy != NULL ) {
		if ( sAllCacheImagesProxy->hasDylib(path, &cacheIndex) )
			return sAllCacheImagesProxy;
	}
#endif
	uint statErrNo;
	struct stat statBuf;
	bool didStat = false;
	bool existsOnDisk;
	__block dyld3::SharedCacheFindDylibResults shareCacheResults;
	shareCacheResults.image = nullptr;

#if TARGET_OS_SIMULATOR

	auto findSharedCacheImage = ^() {
		// in simulators, 'path' has DYLD_ROOT_PATH prepended, but cache index does not have the prefix, so use orgPath
		return dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, orgPath, &shareCacheResults);
	};

#else

	auto findSharedCacheImage = ^() {
		return dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, path, &shareCacheResults);
	};

#endif

	if ( findSharedCacheImage() ) {
		// see if this image in the cache was already loaded via a different path
		for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); ++it) {
			ImageLoader* anImage = *it;
			if ( (const mach_header*)anImage->machHeader() == shareCacheResults.mhInCache )
				return anImage;
		}
		// if RTLD_NOLOAD, do nothing if not already loaded
		if ( context.dontLoad ) {
			// <rdar://33412890> possible that there is an override of cache
			if ( dyld3::stat(path, &statBuf) == 0 ) {
				ImageLoader* imageLoader = findLoadedImage(statBuf);
				if ( imageLoader != NULL )
					return imageLoader;
			}
			return NULL;
		}
		bool useCache = false;
		if ( shareCacheResults.image == nullptr ) {
			// HACK to support old caches
			existsOnDisk = ( dyld3::stat(path, &statBuf) == 0 );
			didStat = true;
			statErrNo = errno;
			useCache = !existsOnDisk;
		}
		else {
			// <rdar://problem/7014995> zero out stat buffer so mtime, etc are zero for items from the shared cache
			bzero(&statBuf, sizeof(statBuf));
			if ( shareCacheResults.image->overridableDylib() ) {
				existsOnDisk = ( dyld3::stat(path, &statBuf) == 0 );
				statErrNo = errno;
				if ( sSharedCacheLoadInfo.loadAddress->header.dylibsExpectedOnDisk ) {
					// old style macOS with dylibs on disk
					uint64_t expectedINode;
					uint64_t expectedMtime;
					if ( shareCacheResults.image->hasFileModTimeAndInode(expectedINode, expectedMtime) ) {
						// if dylib found has same inode/mtime as one in cache, use one in cache
						if ( (expectedMtime == statBuf.st_mtime) && (expectedINode == statBuf.st_ino) )
							useCache = true;
					}
				}
				else {
					// MRM style where dylibs are not on disk
					if ( !existsOnDisk ) {
						// looking at path where dylib should be, and we expect it to not be there but rather in the cache
						// Its possible we are looking at a deleted symlink path.  For example, we are trying to open .../AppKit but
						// there's already a loaded root of .../Versions/C/AppKit.  That used to work when the symlink was on-disk as
						// we'd realpath to find the shared cache path.  Now we record the aliases in the cache and delete the symlinks.
						const char* pathInSharedCache = shareCacheResults.image->path();
						if ( strcmp(path, pathInSharedCache) != 0 ) {
							ImageLoader* imageLoader = loadPhase5check(pathInSharedCache, orgPath, context);
							if ( imageLoader != NULL )
								return imageLoader;
						}
						useCache = true;
					}
					else if ( !sRootsChecker.onDiskFileIsRoot(path, sSharedCacheLoadInfo.loadAddress,
															  shareCacheResults.image, nullptr, statBuf.st_ino, statBuf.st_mtime) ) {
						// we found a file on disk, at the same path as the dyld cache has a dylib and it is one of the magic three
						useCache = true;
					}
				}
			}
			else {
				// we are trying to override a dylib in the cache that does not allow overrides, ignore override and use cache
				useCache = true;
			}
		}
		if ( useCache ) {
			const dyld3::MachOFile* cacheDylibMH = (dyld3::MachOFile*)shareCacheResults.mhInCache;
			if ( !cacheDylibMH->loadableIntoProcess((dyld3::Platform)gProcessInfo->platform, path) )
				throwf("mach-o, but not built for platform %s", dyld3::MachOFile::platformName((dyld3::Platform)gProcessInfo->platform));

			ImageLoader* imageLoader = ImageLoaderMachO::instantiateFromCache((macho_header*)cacheDylibMH, shareCacheResults.pathInCache, shareCacheResults.slideInCache, statBuf, gLinkContext);
			return checkandAddImage(imageLoader, context);
		}
	}

	// not in cache or cache not usable
	if ( !didStat ) {
		existsOnDisk = ( dyld3::stat(path, &statBuf) == 0 );
		statErrNo = errno;
	}
	if ( existsOnDisk ) {
		// in case image was renamed or found via symlinks, check for inode match
		ImageLoader* imageLoader = findLoadedImage(statBuf);
		if ( imageLoader != NULL )
			return imageLoader;
		// do nothing if not already loaded and if RTLD_NOLOAD 
		if ( context.dontLoad )
			return NULL;
		// try opening file
		imageLoader = loadPhase5open(path, context, statBuf, exceptions);
		if ( imageLoader != NULL ) {
			if ( shareCacheResults.image != nullptr ) {
				// if image was found in cache, but is overridden by a newer file on disk, record what the image overrides
				imageLoader->setOverridesCachedDylib(shareCacheResults.image->imageNum());
			}
			return imageLoader;
		}
	}

	// just return NULL if file not found, but record any other errors
	if ( (statErrNo != ENOENT) && (statErrNo != 0) ) {
		if ( (statErrNo == EPERM) && sandboxBlockedStat(path) )
			exceptions->push_back(dyld::mkstringf("%s: file system sandbox blocked stat()", path));
		else
			exceptions->push_back(dyld::mkstringf("%s: stat() failed with errno=%d", path, statErrNo));
	}
	return NULL;
}

// look for path match with existing loaded images
static ImageLoader* loadPhase5check(const char* path, const char* orgPath, const LoadContext& context)
{
	//dyld::log("%s(%s, %s)\n", __func__ , path, orgPath);
	// search path against load-path and install-path of all already loaded images
	uint32_t hash = ImageLoader::hash(path);
	//dyld::log("check() hash=%d, path=%s\n", hash, path);
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
		ImageLoader* anImage = *it;
		// check hash first to cut down on strcmp calls
		//dyld::log("    check() hash=%d, path=%s\n", anImage->getPathHash(), anImage->getPath());
		if ( anImage->getPathHash() == hash ) {
			if ( strcmp(path, anImage->getPath()) == 0 ) {
				// if we are looking for a dylib don't return something else
				if ( !context.mustBeDylib || anImage->isDylib() )
					return anImage;
			}
		}
		if ( context.matchByInstallName || anImage->matchInstallPath() ) {
			const char* installPath = anImage->getInstallPath();
			if ( installPath != NULL) {
				if ( strcmp(path, installPath) == 0 ) {
					// if we are looking for a dylib don't return something else
					if ( !context.mustBeDylib || anImage->isDylib() )
						return anImage;
				}
			}
		}
		// an install name starting with @rpath should match by install name, not just real path
		if ( (orgPath[0] == '@') && (strncmp(orgPath, "@rpath/", 7) == 0) ) {
			const char* installPath = anImage->getInstallPath();
			if ( installPath != NULL) {
				if ( !context.mustBeDylib || anImage->isDylib() ) {
					if ( strcmp(orgPath, installPath) == 0 )
						return anImage;
				}
			}
		}
	}
	
	//dyld::log("%s(%s) => NULL\n", __func__,   path);
	return NULL;
}


// open or check existing
static ImageLoader* loadPhase5(const char* path, const char* orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);
	
	// check for specific dylib overrides
	for (std::vector<DylibOverride>::iterator it = sDylibOverrides.begin(); it != sDylibOverrides.end(); ++it) {
		if ( strcmp(it->installName, path) == 0 ) {
			path = it->override;
			break;
		}
	}
	
	if ( exceptions != NULL ) 
		return loadPhase5load(path, orgPath, context, cacheIndex, exceptions);
	else
		return loadPhase5check(path, orgPath, context);
}

// try with and without image suffix
static ImageLoader* loadPhase4(const char* path, const char* orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);
	ImageLoader* image = NULL;
	if ( gLinkContext.imageSuffix != NULL ) {
 		for (const char* const* suffix=gLinkContext.imageSuffix; *suffix != NULL; ++suffix) {
 			char pathWithSuffix[strlen(path)+strlen(*suffix)+2];
 			ImageLoader::addSuffix(path, *suffix, pathWithSuffix);
 			image = loadPhase5(pathWithSuffix, orgPath, context, cacheIndex, exceptions);
 			if ( image != NULL )
 				break;
 		}
		if ( image != NULL ) {
			// if original path is in the dyld cache, then mark this one found as an override
			dyld3::SharedCacheFindDylibResults shareCacheResults;
			if ( dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, path, &shareCacheResults) && (shareCacheResults.image != nullptr) )
				image->setOverridesCachedDylib(shareCacheResults.image->imageNum());
		}
	}
	if ( image == NULL )
		image = loadPhase5(path, orgPath, context, cacheIndex, exceptions);
	return image;
}

static ImageLoader* loadPhase2(const char* path, const char* orgPath, const LoadContext& context,
							   const char* const frameworkPaths[], const char* const libraryPaths[],
							   unsigned& cacheIndex, std::vector<const char*>* exceptions); // forward reference


// expand @ variables
static ImageLoader* loadPhase3(const char* path, const char* orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);
	ImageLoader* image = NULL;
	if ( strncmp(path, "@executable_path/", 17) == 0 ) {
		// executable_path cannot be in used in any binary in a setuid process rdar://problem/4589305
		if ( !gLinkContext.allowAtPaths )
			throwf("unsafe use of @executable_path in %s with restricted binary (Codesign main executable with Library Validation to allow @ paths)", context.origin);
		// handle @executable_path path prefix
		const char* executablePath = sExecPath;
		char newPath[strlen(executablePath) + strlen(path)];
		strcpy(newPath, executablePath);
		char* addPoint = strrchr(newPath,'/');
		if ( addPoint != NULL )
			strcpy(&addPoint[1], &path[17]);
		else
			strcpy(newPath, &path[17]);
		image = loadPhase4(newPath, orgPath, context, cacheIndex, exceptions);
		if ( image != NULL )
			return image;

		// perhaps main executable path is a sym link, find realpath and retry
		char resolvedPath[PATH_MAX];
		if ( realpath(sExecPath, resolvedPath) != NULL ) {
			char newRealPath[strlen(resolvedPath) + strlen(path)];
			strcpy(newRealPath, resolvedPath);
			addPoint = strrchr(newRealPath,'/');
			if ( addPoint != NULL )
				strcpy(&addPoint[1], &path[17]);
			else
				strcpy(newRealPath, &path[17]);
			image = loadPhase4(newRealPath, orgPath, context, cacheIndex, exceptions);
			if ( image != NULL )
				return image;
		}
	}
	else if ( (strncmp(path, "@loader_path/", 13) == 0) && (context.origin != NULL) ) {
		// @loader_path cannot be used from the main executable of a setuid process rdar://problem/4589305
		if ( !gLinkContext.allowAtPaths  && (strcmp(context.origin, sExecPath) == 0) )
			throwf("unsafe use of @loader_path in %s with restricted binary (Codesign main executable with Library Validation to allow @ paths)", context.origin);
		// handle @loader_path path prefix
		char newPath[strlen(context.origin) + strlen(path)];
		strcpy(newPath, context.origin);
		char* addPoint = strrchr(newPath,'/');
		if ( addPoint != NULL )
			strcpy(&addPoint[1], &path[13]);
		else
			strcpy(newPath, &path[13]);
		image = loadPhase4(newPath, orgPath, context, cacheIndex, exceptions);
		if ( image != NULL )
			return image;
		
		// perhaps loader path is a sym link, find realpath and retry
		char resolvedPath[PATH_MAX];
		if ( realpath(context.origin, resolvedPath) != NULL ) {
			char newRealPath[strlen(resolvedPath) + strlen(path)];
			strcpy(newRealPath, resolvedPath);
			addPoint = strrchr(newRealPath,'/');
			if ( addPoint != NULL )
				strcpy(&addPoint[1], &path[13]);
			else
				strcpy(newRealPath, &path[13]);
			image = loadPhase4(newRealPath, orgPath, context, cacheIndex, exceptions);
			if ( image != NULL )
				return image;
		}
	}
	else if ( context.implicitRPath || (strncmp(path, "@rpath/", 7) == 0) ) {
		const char* trailingPath = (strncmp(path, "@rpath/", 7) == 0) ? &path[7] : path;
		// substitute @rpath with all -rpath paths up the load chain
		for(const ImageLoader::RPathChain* rp=context.rpath; rp != NULL; rp=rp->next) {
			if (rp->paths != NULL ) {
				for(std::vector<const char*>::iterator it=rp->paths->begin(); it != rp->paths->end(); ++it) {
					const char* anRPath = *it;
					char newPath[strlen(anRPath) + strlen(trailingPath)+2];
					strcpy(newPath, anRPath);
					if ( newPath[strlen(newPath)-1] != '/' )
						strcat(newPath, "/");
					strcat(newPath, trailingPath); 
					image = loadPhase4(newPath, orgPath, context, cacheIndex, exceptions);
					if ( gLinkContext.verboseRPaths && (exceptions != NULL) ) {
						if ( image != NULL ) 
							dyld::log("RPATH successful expansion of %s to: %s\n", orgPath, newPath);
						else
							dyld::log("RPATH failed expanding     %s to: %s\n", orgPath, newPath);
					}
					if ( image != NULL ) 
						return image;
				}
			}
		}
		
		// substitute @rpath with LD_LIBRARY_PATH
		if ( sEnv.LD_LIBRARY_PATH != NULL ) {
			image = loadPhase2(trailingPath, orgPath, context, NULL, sEnv.LD_LIBRARY_PATH, cacheIndex, exceptions);
			if ( image != NULL )
				return image;
		}
		
		// if this is the "open" pass, don't try to open @rpath/... as a relative path
		if ( (exceptions != NULL) && (trailingPath != path) )
			return NULL;
	}
	else if ( !gLinkContext.allowEnvVarsPath && (path[0] != '/' ) ) {
		throwf("unsafe use of relative rpath %s in %s with restricted binary", path, context.origin);
	}

	return loadPhase4(path, orgPath, context, cacheIndex, exceptions);
}

static ImageLoader* loadPhase2cache(const char* path, const char *orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions) {
	ImageLoader* image = NULL;
#if !TARGET_OS_SIMULATOR
	if ( (exceptions != NULL) && (gLinkContext.allowEnvVarsPath || !isFileRelativePath(path)) && (path[0] != '@') ) {
		char resolvedPath[PATH_MAX];
		realpath(path, resolvedPath);
		int myerr = errno;
		// If realpath() resolves to a path which does not exist on disk, errno is set to ENOENT
		if ( (myerr == ENOENT) || (myerr == 0) )
		{
			image = loadPhase4(resolvedPath, orgPath, context, cacheIndex, exceptions);
		}
	}
#endif
	return image;
}


// try search paths
static ImageLoader* loadPhase2(const char* path, const char* orgPath, const LoadContext& context,
							   const char* const frameworkPaths[], const char* const libraryPaths[], 
							   unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);
	ImageLoader* image = NULL;
	const char* frameworkPartialPath = getFrameworkPartialPath(path);
	if ( frameworkPaths != NULL ) {
		if ( frameworkPartialPath != NULL ) {
			const size_t frameworkPartialPathLen = strlen(frameworkPartialPath);
			for(const char* const* fp = frameworkPaths; *fp != NULL; ++fp) {
				char npath[strlen(*fp)+frameworkPartialPathLen+8];
				strcpy(npath, *fp);
				strcat(npath, "/");
				strcat(npath, frameworkPartialPath);
				//dyld::log("dyld: fallback framework path used: %s() -> loadPhase4(\"%s\", ...)\n", __func__, npath);
				image = loadPhase4(npath, orgPath, context, cacheIndex, exceptions);
				// Look in the cache if appropriate
				if ( image == NULL)
					image = loadPhase2cache(npath, orgPath, context, cacheIndex, exceptions);
				if ( image != NULL ) {
					// if original path is in the dyld cache, then mark this one found as an override
					dyld3::SharedCacheFindDylibResults shareCacheResults;
					if ( dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, path, &shareCacheResults) && (shareCacheResults.image != nullptr) ) {
						image->setOverridesCachedDylib(shareCacheResults.image->imageNum());
					}
#if SUPPORT_ROOT_PATH
					else if ( (gLinkContext.rootPaths != nullptr)
							   && dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, orgPath, &shareCacheResults)
							   && (shareCacheResults.image != nullptr) ) {
						// DYLD_ROOT_PATH, ie, iOSMac, also needs to check if the original path is overridden
						// as the root prefix has been applied to 'path', but the framework path searches without a root path prefix
						image->setOverridesCachedDylib(shareCacheResults.image->imageNum());
					}
#endif
					return image;
				}
			}
		}
	}
	// <rdar://problem/12649639> An executable with the same name as a framework & DYLD_LIBRARY_PATH pointing to it gets loaded twice
	// <rdar://problem/14160846> Some apps depend on frameworks being found via library paths
	if ( (libraryPaths != NULL) && ((frameworkPartialPath == NULL) || sFrameworksFoundAsDylibs) ) {
		const char* libraryLeafName = getLibraryLeafName(path);
		const size_t libraryLeafNameLen = strlen(libraryLeafName);
		for(const char* const* lp = libraryPaths; *lp != NULL; ++lp) {
			char libpath[strlen(*lp)+libraryLeafNameLen+8];
			strcpy(libpath, *lp);
			strcat(libpath, "/");
			strcat(libpath, libraryLeafName);
			//dyld::log("dyld: fallback library path used: %s() -> loadPhase4(\"%s\", ...)\n", __func__, libpath);
			image = loadPhase4(libpath, orgPath, context, cacheIndex, exceptions);
			// Look in the cache if appropriate
			if ( image == NULL)
				image = loadPhase2cache(libpath, orgPath, context, cacheIndex, exceptions);
			if ( image != NULL ) {
				// if original path is in the dyld cache, then mark this one found as an override
			    dyld3::SharedCacheFindDylibResults shareCacheResults;
			    if ( dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, path, &shareCacheResults) && (shareCacheResults.image != nullptr) ) {
				    image->setOverridesCachedDylib(shareCacheResults.image->imageNum());
			    }
#if SUPPORT_ROOT_PATH
				else if ( (gLinkContext.rootPaths != nullptr)
						   && dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, orgPath, &shareCacheResults)
						   && (shareCacheResults.image != nullptr) ) {
				    // DYLD_ROOT_PATH, ie, iOSMac, also needs to check if the original path is overridden
				    // as the root prefix has been applied to 'path', but the library path searches without a root path prefix
				    image->setOverridesCachedDylib(shareCacheResults.image->imageNum());
			    }
#endif
			    return image;
		   }
		}
	}
	return NULL;
}

// try search overrides and fallbacks
static ImageLoader* loadPhase1(const char* path, const char* orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);
	ImageLoader* image = NULL;

	bool pathIsInDyldCacheWhichCannotBeOverridden = false;
	if ( sSharedCacheLoadInfo.loadAddress != nullptr ) {
		pathIsInDyldCacheWhichCannotBeOverridden = sSharedCacheLoadInfo.loadAddress->hasNonOverridablePath(path);
	}

	// <rdar://problem/48490116> dyld customer cache cannot be overridden
	if ( !pathIsInDyldCacheWhichCannotBeOverridden ) {
		// handle LD_LIBRARY_PATH environment variables that force searching
		if ( context.useLdLibraryPath && (sEnv.LD_LIBRARY_PATH != NULL) ) {
			image = loadPhase2(path, orgPath, context,  NULL, sEnv.LD_LIBRARY_PATH, cacheIndex,exceptions);
			if ( image != NULL )
				return image;
		}

		// handle DYLD_ environment variables that force searching
		if ( context.useSearchPaths && ((sEnv.DYLD_FRAMEWORK_PATH != NULL) || (sEnv.DYLD_LIBRARY_PATH != NULL)) ) {
			image = loadPhase2(path, orgPath, context, sEnv.DYLD_FRAMEWORK_PATH, sEnv.DYLD_LIBRARY_PATH, cacheIndex, exceptions);
			if ( image != NULL )
				return image;
		}
	}

	// try raw path
	image = loadPhase3(path, orgPath, context, cacheIndex, exceptions);
	if ( image != NULL )
		return image;
	
	// try fallback paths during second time (will open file)
	const char* const* fallbackLibraryPaths = sEnv.DYLD_FALLBACK_LIBRARY_PATH;
	if ( (fallbackLibraryPaths != NULL) && !context.useFallbackPaths )
		fallbackLibraryPaths = NULL;
	if ( !context.dontLoad  && (exceptions != NULL) && ((sEnv.DYLD_FALLBACK_FRAMEWORK_PATH != NULL) || (fallbackLibraryPaths != NULL)) ) {
		image = loadPhase2(path, orgPath, context, sEnv.DYLD_FALLBACK_FRAMEWORK_PATH, fallbackLibraryPaths, cacheIndex, exceptions);
		if ( image != NULL )
			return image;
	}

	// <rdar://problem/47682983> if hardened app calls dlopen() with a leaf path, dyld should only look in /usr/lib
	if ( context.useLdLibraryPath && (fallbackLibraryPaths == NULL) ) {
		const char* stdPaths[2] = { "/usr/lib", NULL };
		image = loadPhase2(path, orgPath, context, NULL, stdPaths, cacheIndex, exceptions);
		if ( image != NULL )
			return image;
	}

#if SUPPORT_VERSIONED_PATHS
    // <rdar://problem/53215116> DYLD_VERSIONED_FRAMEWORK_PATH fails to load a framework if it does not also exist at the system install path
    // Scan to see if the dylib appears in a versioned path. Don't worry if we find the newest, that will handled later
    if ( !context.dontLoad  && (exceptions != NULL) && ((sEnv.DYLD_VERSIONED_FRAMEWORK_PATH != NULL) || (sEnv.DYLD_VERSIONED_LIBRARY_PATH != NULL)) ) {
        image = loadPhase2(path, orgPath, context, sEnv.DYLD_VERSIONED_FRAMEWORK_PATH, sEnv.DYLD_VERSIONED_LIBRARY_PATH, cacheIndex, exceptions);
        if ( image != NULL )
            return image;
    }
#endif

	return NULL;
}

// try root substitutions
static ImageLoader* loadPhase0(const char* path, const char* orgPath, const LoadContext& context, unsigned& cacheIndex, std::vector<const char*>* exceptions)
{
	//dyld::log("%s(%s, %p)\n", __func__ , path, exceptions);

#if TARGET_OS_OSX
	// handle macOS dylibs dlopen()ing versioned path which needs to map to flat path in mazipan simulator
	if ( gLinkContext.iOSonMac && strstr(path, ".framework/Versions/")) {
		uintptr_t sourceOffset = 0;
		uintptr_t destOffset = 0;
		size_t sourceLangth = strlen(path);
		char flatPath[sourceLangth];
		flatPath[0] = 0;
		const char* frameworkBase = NULL;
		while ((frameworkBase = strstr(&path[sourceOffset], ".framework/Versions/"))) {
			uintptr_t foundLength = (frameworkBase - &path[sourceOffset]) + strlen(".framework/") ;
			strlcat(&flatPath[destOffset], &path[sourceOffset], foundLength);
			sourceOffset += foundLength + strlen("Versions/") + 1;
			destOffset += foundLength - 1;
		}
		strlcat(&flatPath[destOffset], &path[sourceOffset], sourceLangth);
		ImageLoader* image = loadPhase0(flatPath, orgPath, context, cacheIndex, exceptions);
		if ( image != NULL )
			return image;
	}
#endif
	
#if SUPPORT_ROOT_PATH
	// handle DYLD_ROOT_PATH which forces absolute paths to use a new root
	if ( (gLinkContext.rootPaths != NULL) && (path[0] == '/') ) {
		for(const char* const* rootPath = gLinkContext.rootPaths; *rootPath != NULL; ++rootPath) {
			size_t rootLen = strlen(*rootPath);
			if ( strncmp(path, *rootPath, rootLen) != 0 ) {
				char newPath[rootLen + strlen(path)+2];
				strcpy(newPath, *rootPath);
				strcat(newPath, path);
				ImageLoader* image = loadPhase1(newPath, orgPath, context, cacheIndex, exceptions);
				if ( image != NULL )
					return image;
			}
		}
	}
#endif

	// try raw path
	return loadPhase1(path, orgPath, context, cacheIndex, exceptions);
}

//
// Given all the DYLD_ environment variables, the general case for loading libraries
// is that any given path expands into a list of possible locations to load.  We
// also must take care to ensure two copies of the "same" library are never loaded.
//
// The algorithm used here is that there is a separate function for each "phase" of the
// path expansion.  Each phase function calls the next phase with each possible expansion
// of that phase.  The result is the last phase is called with all possible paths.  
//
// To catch duplicates the algorithm is run twice.  The first time, the last phase checks
// the path against all loaded images.  The second time, the last phase calls open() on 
// the path.  Either time, if an image is found, the phases all unwind without checking
// for other paths.
//
ImageLoader* load(const char* path, const LoadContext& context, unsigned& cacheIndex)
{
	CRSetCrashLogMessage2(path);
	const char* orgPath = path;
	cacheIndex = UINT32_MAX;
	
	//dyld::log("%s(%s)\n", __func__ , path);
	char realPath[PATH_MAX];
	// when DYLD_IMAGE_SUFFIX is in used, do a realpath(), otherwise a load of "Foo.framework/Foo" will not match
	if ( context.useSearchPaths && ( gLinkContext.imageSuffix != NULL && *gLinkContext.imageSuffix != NULL) ) {
		if ( realpath(path, realPath) != NULL )
			path = realPath;
	}
	
	// try all path permutations and check against existing loaded images

	ImageLoader* image = loadPhase0(path, orgPath, context, cacheIndex, NULL);
	if ( image != NULL ) {
		CRSetCrashLogMessage2(NULL);
		return image;
	}

	// try all path permutations and try open() until first success
	std::vector<const char*> exceptions;
	image = loadPhase0(path, orgPath, context, cacheIndex, &exceptions);
#if !TARGET_OS_SIMULATOR
	// <rdar://problem/16704628> support symlinks on disk to a path in dyld shared cache
	if ( image == NULL)
		image = loadPhase2cache(path, orgPath, context, cacheIndex, &exceptions);
#endif
    CRSetCrashLogMessage2(NULL);
	if ( image != NULL ) {
		// <rdar://problem/6916014> leak in dyld during dlopen when using DYLD_ variables
		for (std::vector<const char*>::iterator it = exceptions.begin(); it != exceptions.end(); ++it) {
			free((void*)(*it));
		}
		// if loaded image is not from cache, but original path is in cache
		// set gSharedCacheOverridden flag to disable some ObjC optimizations
		if ( !gSharedCacheOverridden && !image->inSharedCache() && image->isDylib() && dyld3::MachOFile::isSharedCacheEligiblePath(path) && inSharedCache(path) ) {
			gSharedCacheOverridden = true;
		}
		// <rdar://problem/59327556> if file loaded via symlink to a root of something in dyld cache, mark it as an override
		dyld3::SharedCacheFindDylibResults shareCacheResults;
		if ( !image->inSharedCache() && dyld3::findInSharedCacheImage(sSharedCacheLoadInfo, image->getRealPath(), &shareCacheResults) && (shareCacheResults.image != nullptr) )
			image->setOverridesCachedDylib(shareCacheResults.image->imageNum());
		
		return image;
	}
	else if ( exceptions.size() == 0 ) {
		if ( context.dontLoad ) {
			return NULL;
		}
		else
			throw "image not found";
	}
	else {
		const char* msgStart = "no suitable image found.  Did find:";
		const char* delim = "\n\t";
		size_t allsizes = strlen(msgStart)+8;
		for (size_t i=0; i < exceptions.size(); ++i) 
			allsizes += (strlen(exceptions[i]) + strlen(delim));
		char* fullMsg = new char[allsizes];
		strcpy(fullMsg, msgStart);
		for (size_t i=0; i < exceptions.size(); ++i) {
			strcat(fullMsg, delim);
			strcat(fullMsg, exceptions[i]);
			free((void*)exceptions[i]);
		}
		throw (const char*)fullMsg;
	}
}





static void mapSharedCache(uintptr_t mainExecutableSlide)
{
	dyld3::SharedCacheOptions opts;
	opts.cacheDirOverride	= sSharedCacheOverrideDir;
	opts.forcePrivate		= (gLinkContext.sharedRegionMode == ImageLoader::kUsePrivateSharedRegion);
#if __x86_64__ && !TARGET_OS_SIMULATOR
	opts.useHaswell			= sHaswell;
#else
	opts.useHaswell			= false;
#endif
	opts.verbose			= gLinkContext.verboseMapping;
    // <rdar://problem/32031197> respect -disable_aslr boot-arg
    // <rdar://problem/56299169> kern.bootargs is now blocked
	opts.disableASLR		= (mainExecutableSlide == 0) && dyld3::internalInstall(); // infer ASLR is off if main executable is not slid
	loadDyldCache(opts, &sSharedCacheLoadInfo);

	// update global state
	if ( sSharedCacheLoadInfo.loadAddress != nullptr ) {
		gLinkContext.dyldCache 								= sSharedCacheLoadInfo.loadAddress;
		dyld::gProcessInfo->processDetachedFromSharedRegion = opts.forcePrivate;
		dyld::gProcessInfo->sharedCacheSlide                = sSharedCacheLoadInfo.slide;
		dyld::gProcessInfo->sharedCacheBaseAddress          = (unsigned long)sSharedCacheLoadInfo.loadAddress;
		sSharedCacheLoadInfo.loadAddress->getUUID(dyld::gProcessInfo->sharedCacheUUID);
		dyld3::kdebug_trace_dyld_image(DBG_DYLD_UUID_SHARED_CACHE_A, sSharedCacheLoadInfo.path, (const uuid_t *)&dyld::gProcessInfo->sharedCacheUUID[0], {0,0}, {{ 0, 0 }}, (const mach_header *)sSharedCacheLoadInfo.loadAddress);
	}

//#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
// RAM disk booting does not have shared cache yet
// Don't make lack of a shared cache fatal in that case
//	if ( sSharedCacheLoadInfo.loadAddress == nullptr ) {
//		if ( sSharedCacheLoadInfo.errorMessage != nullptr )
//			halt(sSharedCacheLoadInfo.errorMessage);
//		else
//			halt("error loading dyld shared cache");
//	}
//#endif
}



// create when NSLinkModule is called for a second time on a bundle
ImageLoader* cloneImage(ImageLoader* image)
{
	// open file (automagically closed when this function exits)
	FileOpener file(image->getPath());
	
	struct stat stat_buf;
	if ( fstat(file.getFileDescriptor(), &stat_buf) == -1)
		throw "stat error";
	
	dyld::LoadContext context;
	context.useSearchPaths		= false;
	context.useFallbackPaths	= false;
	context.useLdLibraryPath	= false;
	context.implicitRPath		= false;
	context.matchByInstallName	= false;
	context.dontLoad			= false;
	context.mustBeBundle		= true;
	context.mustBeDylib			= false;
	context.canBePIE			= false;
	context.origin				= NULL;
	context.rpath				= NULL;
	return loadPhase6(file.getFileDescriptor(), stat_buf, image->getPath(), context);
}


ImageLoader* loadFromMemory(const uint8_t* mem, uint64_t len, const char* moduleName)
{
	// if fat wrapper, find usable sub-file
	const fat_header* memStartAsFat = (fat_header*)mem;
	uint64_t fileOffset = 0;
	uint64_t fileLength = len;
	if ( memStartAsFat->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		if ( fatFindBest(memStartAsFat, &fileOffset, &fileLength) ) {
			mem = &mem[fileOffset];
			len = fileLength;
		}
		else {
			throw "no matching architecture in universal wrapper";
		}
	}

	// try each loader
	if ( isCompatibleMachO(mem, moduleName) ) {
		ImageLoader* image = ImageLoaderMachO::instantiateFromMemory(moduleName, (macho_header*)mem, len, gLinkContext);
		// don't add bundles to global list, they can be loaded but not linked.  When linked it will be added to list
		if ( ! image->isBundle() ) 
			addImage(image);
		return image;
	}
	
	// try other file formats here...
	
	// throw error about what was found
	switch (*(uint32_t*)mem) {
		case MH_MAGIC:
		case MH_CIGAM:
		case MH_MAGIC_64:
		case MH_CIGAM_64:
			throw "mach-o, but wrong architecture";
		default:
		throwf("unknown file type, first eight bytes: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X", 
			mem[0], mem[1], mem[2], mem[3], mem[4], mem[5], mem[6],mem[7]);
	}
}


void registerAddCallback(ImageCallback func)
{
	// now add to list to get notified when any more images are added
	sAddImageCallbacks.push_back(func);
	
	// call callback with all existing images
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
		ImageLoader* image = *it;
		if ( image->getState() >= dyld_image_state_bound && image->getState() < dyld_image_state_terminated ) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)image->machHeader(), (uint64_t)(*func), 0);
			(*func)(image->machHeader(), image->getSlide());
		}
	}
#if SUPPORT_ACCELERATE_TABLES
	if ( sAllCacheImagesProxy != NULL ) {
		dyld_image_info	infos[allImagesCount()+1];
		unsigned cacheCount = sAllCacheImagesProxy->appendImagesToNotify(dyld_image_state_bound, true, infos);
		for (unsigned i=0; i < cacheCount; ++i) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)infos[i].imageLoadAddress, (uint64_t)(*func), 0);
			(*func)(infos[i].imageLoadAddress, sSharedCacheLoadInfo.slide);
		}
	}
#endif
}

void registerLoadCallback(LoadImageCallback func)
{
	// now add to list to get notified when any more images are added
	sAddLoadImageCallbacks.push_back(func);

	// call callback with all existing images
	for (ImageLoader* image : sAllImages) {
		if ( image->getState() >= dyld_image_state_bound && image->getState() < dyld_image_state_terminated ) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)image->machHeader(), (uint64_t)(*func), 0);
			(*func)(image->machHeader(), image->getPath(), !image->neverUnload());
		}
	}
#if SUPPORT_ACCELERATE_TABLES
	if ( sAllCacheImagesProxy != NULL ) {
		dyld_image_info	infos[allImagesCount()+1];
		unsigned cacheCount = sAllCacheImagesProxy->appendImagesToNotify(dyld_image_state_bound, true, infos);
		for (unsigned i=0; i < cacheCount; ++i) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)infos[i].imageLoadAddress, (uint64_t)(*func), 0);
			(*func)(infos[i].imageLoadAddress, infos[i].imageFilePath, false);
		}
	}
#endif
}

void registerBulkLoadCallback(LoadImageBulkCallback func)
{
	// call callback with all existing images
	unsigned count = dyld::gProcessInfo->infoArrayCount;
	const dyld_image_info* infoArray = dyld::gProcessInfo->infoArray;
	if ( infoArray != NULL ) {
		const mach_header* mhs[count];
		const char*        paths[count];
		for (unsigned i=0; i < count; ++i) {
			mhs[i]   = infoArray[i].imageLoadAddress;
			paths[i] = infoArray[i].imageFilePath;
		}
		dyld3::ScopedTimer timer(DBG_DYLD_TIMING_FUNC_FOR_ADD_IMAGE, (uint64_t)mhs[0], (uint64_t)func, 0);
		func(count, mhs, paths);
	}

	// now add to list to get notified when any more images are added
	sAddBulkLoadImageCallbacks.push_back(func);
}

void registerRemoveCallback(ImageCallback func)
{
	// <rdar://problem/15025198> ignore calls to register a notification during a notification
	if ( sRemoveImageCallbacksInUse )
		return;
	sRemoveImageCallbacks.push_back(func);
}

void clearErrorMessage()
{
	error_string[0] = '\0';
}

void setErrorMessage(const char* message)
{
	// save off error message in global buffer for CrashReporter to find
	strlcpy(error_string, message, sizeof(error_string));
}

const char* getErrorMessage()
{
	return error_string;
}

void halt(const char* message)
{
	if ( sSharedCacheLoadInfo.errorMessage != nullptr ) {
		// <rdar://problem/45957449> if dyld fails with a missing dylib and there is no shared cache, display the shared cache load error message
		dyld::log("dyld: dyld cache load error: %s\n", sSharedCacheLoadInfo.errorMessage);
		dyld::log("dyld: %s\n", message);
		strlcpy(error_string, "dyld cache load error: ", sizeof(error_string));
		strlcat(error_string, sSharedCacheLoadInfo.errorMessage, sizeof(error_string));
		strlcat(error_string, "\n", sizeof(error_string));
		strlcat(error_string, message, sizeof(error_string));
	} else if ( dyld::gProcessInfo->errorKind == DYLD_EXIT_REASON_DYLIB_MISSING ) {
		// If a dylib is missing, but we have the cache, print the cache UUID to make it easier
		// to see what might have gone wrong
		if ( sSharedCacheLoadInfo.loadAddress == nullptr ) {
			strlcpy(error_string, "dyld: No shared cache present\n", sizeof(error_string));
		} else {
			uuid_t cacheUUID;
			sSharedCacheLoadInfo.loadAddress->getUUID(cacheUUID);
			uuid_string_t uuidStr;
			uuid_unparse_upper(cacheUUID, uuidStr);

			strlcpy(error_string, "dyld: Using shared cache: ", sizeof(error_string));
			strlcat(error_string, uuidStr, sizeof(error_string));
			strlcat(error_string, "\n", sizeof(error_string));
		}

		dyld::log("dyld: %s\n", message);
		strlcat(error_string, message, sizeof(error_string));
	}
	else {
		dyld::log("dyld: %s\n", message);
		strlcpy(error_string, message, sizeof(error_string));
	}

	dyld::gProcessInfo->errorMessage = error_string;
	if ( !gLinkContext.startedInitializingMainExecutable )
		dyld::gProcessInfo->terminationFlags = 1;
	else
		dyld::gProcessInfo->terminationFlags = 0;

	char payloadBuffer[EXIT_REASON_PAYLOAD_MAX_LEN];
	dyld_abort_payload* payload = (dyld_abort_payload*)payloadBuffer;
	payload->version               = 1;
	payload->flags                 = gLinkContext.startedInitializingMainExecutable ? 0 : 1;
	payload->targetDylibPathOffset = 0;
	payload->clientPathOffset      = 0;
	payload->symbolOffset          = 0;
	int payloadSize = sizeof(dyld_abort_payload);

	if ( dyld::gProcessInfo->errorTargetDylibPath != NULL ) {
		payload->targetDylibPathOffset = payloadSize;
		payloadSize += strlcpy(&payloadBuffer[payloadSize], dyld::gProcessInfo->errorTargetDylibPath, sizeof(payloadBuffer)-payloadSize) + 1;
	}
	if ( dyld::gProcessInfo->errorClientOfDylibPath != NULL ) {
		payload->clientPathOffset = payloadSize;
		payloadSize += strlcpy(&payloadBuffer[payloadSize], dyld::gProcessInfo->errorClientOfDylibPath, sizeof(payloadBuffer)-payloadSize) + 1;
	}
	if ( dyld::gProcessInfo->errorSymbol != NULL ) {
		payload->symbolOffset = payloadSize;
		payloadSize += strlcpy(&payloadBuffer[payloadSize], dyld::gProcessInfo->errorSymbol, sizeof(payloadBuffer)-payloadSize) + 1;
	}
	char truncMessage[EXIT_REASON_USER_DESC_MAX_LEN];
	strlcpy(truncMessage, error_string, EXIT_REASON_USER_DESC_MAX_LEN);
	abort_with_payload(OS_REASON_DYLD, dyld::gProcessInfo->errorKind ? dyld::gProcessInfo->errorKind : DYLD_EXIT_REASON_OTHER, payloadBuffer, payloadSize, truncMessage, 0);
}

static void setErrorStrings(unsigned errorCode, const char* errorClientOfDylibPath,
								const char* errorTargetDylibPath, const char* errorSymbol)
{
	dyld::gProcessInfo->errorKind = errorCode;
	dyld::gProcessInfo->errorClientOfDylibPath = errorClientOfDylibPath;
	dyld::gProcessInfo->errorTargetDylibPath = errorTargetDylibPath;
	dyld::gProcessInfo->errorSymbol = errorSymbol;
}


uintptr_t bindLazySymbol(const mach_header* mh, uintptr_t* lazyPointer)
{
	uintptr_t result = 0;
	// acquire read-lock on dyld's data structures
#if 0 // rdar://problem/3811777 turn off locking until deadlock is resolved
	if ( gLibSystemHelpers != NULL ) 
		(*gLibSystemHelpers->lockForReading)();
#endif
	// lookup and bind lazy pointer and get target address
	try {
		ImageLoader* target;
	#if __i386__
		// fast stubs pass NULL for mh and image is instead found via the location of stub (aka lazyPointer)
		if ( mh == NULL )
			target = dyld::findImageContainingAddress(lazyPointer);
		else
			target = dyld::findImageByMachHeader(mh);
	#else
		// note, target should always be mach-o, because only mach-o lazy handler wired up to this
		target = dyld::findImageByMachHeader(mh);
	#endif
		if ( target == NULL )
			throwf("image not found for lazy pointer at %p", lazyPointer);
		DyldSharedCache::DataConstLazyScopedWriter patcher(gLinkContext.dyldCache, mach_task_self(), gLinkContext.verboseMapping ? &dyld::log : nullptr);
		result = target->doBindLazySymbol(lazyPointer, gLinkContext, patcher);
	}
	catch (const char* message) {
		dyld::log("dyld: lazy symbol binding failed: %s\n", message);
		halt(message);
	}
	// release read-lock on dyld's data structures
#if 0
	if ( gLibSystemHelpers != NULL ) 
		(*gLibSystemHelpers->unlockForReading)();
#endif
	// return target address to glue which jumps to it with real parameters restored
	return result;
}


uintptr_t fastBindLazySymbol(ImageLoader** imageLoaderCache, uintptr_t lazyBindingInfoOffset)
{
	uintptr_t result = 0;
	// get image 
	if ( *imageLoaderCache == NULL ) {
		// save in cache
		*imageLoaderCache = dyld::findMappedRange((uintptr_t)imageLoaderCache);
		if ( *imageLoaderCache == NULL ) {
#if SUPPORT_ACCELERATE_TABLES
		if ( sAllCacheImagesProxy != NULL ) {
			const mach_header* mh;
			const char* path;
			unsigned index;
			if ( sAllCacheImagesProxy->addressInCache(imageLoaderCache, &mh, &path, &index) ) {
				result = sAllCacheImagesProxy->bindLazy(lazyBindingInfoOffset, gLinkContext, mh, index);
				if ( result == 0 ) {
					halt("dyld: lazy symbol binding failed for image in dyld shared\n");
				}
				return result;
			}
		}
#endif
			const char* message = "fast lazy binding from unknown image";
			dyld::log("dyld: %s\n", message);
			halt(message);
		}
	}
	
	// bind lazy pointer and return it
	try {
		result = (*imageLoaderCache)->doBindFastLazySymbol((uint32_t)lazyBindingInfoOffset, gLinkContext, 
								(dyld::gLibSystemHelpers != NULL) ? dyld::gLibSystemHelpers->acquireGlobalDyldLock : NULL,
								(dyld::gLibSystemHelpers != NULL) ? dyld::gLibSystemHelpers->releaseGlobalDyldLock : NULL);
	}
	catch (const char* message) {
		dyld::log("dyld: lazy symbol binding failed: %s\n", message);
		halt(message);
	}

	// return target address to glue which jumps to it with real parameters restored
	return result;
}



void registerUndefinedHandler(UndefinedHandler handler)
{
	sUndefinedHandler = handler;
}

static void undefinedHandler(const char* symboName)
{
	if ( sUndefinedHandler != NULL ) {
		(*sUndefinedHandler)(symboName);
	}
}

static bool findExportedSymbol(const char* name, bool onlyInCoalesced, const ImageLoader::Symbol** sym, const ImageLoader** image, ImageLoader::CoalesceNotifier notifier=NULL)
{
	// search all images in order
	const ImageLoader* firstWeakImage = NULL;
	const ImageLoader::Symbol* firstWeakSym = NULL;
	const ImageLoader* firstNonWeakImage = NULL;
	const ImageLoader::Symbol* firstNonWeakSym = NULL;
	const size_t imageCount = sAllImages.size();
	for(size_t i=0; i < imageCount; ++i) {
		ImageLoader* anImage = sAllImages[i];
		// the use of inserted libraries alters search order
		// so that inserted libraries are found before the main executable
		if ( sInsertedDylibCount > 0 ) {
			if ( i < sInsertedDylibCount )
				anImage = sAllImages[i+1];
			else if ( i == sInsertedDylibCount )
				anImage = sAllImages[0];
		}
		//dyld::log("findExportedSymbol(%s) looking at %s\n", name, anImage->getPath());
		if ( ! anImage->hasHiddenExports() && (!onlyInCoalesced || anImage->hasCoalescedExports()) ) {
			const ImageLoader* foundInImage;
			*sym = anImage->findExportedSymbol(name, false, &foundInImage);
			//dyld::log("findExportedSymbol(%s) found: sym=%p, anImage=%p, foundInImage=%p\n", name, *sym, anImage, foundInImage /*, (foundInImage ? foundInImage->getPath() : "")*/);
			if ( *sym != NULL ) {
				if ( notifier && (foundInImage == anImage) )
					notifier(*sym, foundInImage, foundInImage->machHeader());
				// if weak definition found, record first one found
				if ( (foundInImage->getExportedSymbolInfo(*sym) & ImageLoader::kWeakDefinition) != 0 ) {
					if ( firstWeakImage == NULL ) {
						firstWeakImage = foundInImage;
						firstWeakSym = *sym;
					}
				}
				else {
					// found non-weak
					if ( !onlyInCoalesced ) {
						// for flat lookups, return first found
						*image = foundInImage;
						return true;
					}
					if ( firstNonWeakImage == NULL ) {
						firstNonWeakImage = foundInImage;
						firstNonWeakSym = *sym;
					}
				}
			}
		}
	}
	if ( firstNonWeakImage != NULL ) {
		// found a weak definition, but no non-weak, so return first weak found
		*sym = firstNonWeakSym;
		*image = firstNonWeakImage;
		return true;
	}
	if ( firstWeakSym != NULL ) {
		// found a weak definition, but no non-weak, so return first weak found
		*sym = firstWeakSym;
		*image = firstWeakImage;
		return true;
	}
#if SUPPORT_ACCELERATE_TABLES
	if ( sAllCacheImagesProxy != NULL ) {
		if ( sAllCacheImagesProxy->flatFindSymbol(name, onlyInCoalesced, sym, image, notifier) )
			return true;
	}
#endif

	return false;
}

bool flatFindExportedSymbol(const char* name, const ImageLoader::Symbol** sym, const ImageLoader** image)
{
	return findExportedSymbol(name, false, sym, image);
}

bool findCoalescedExportedSymbol(const char* name, const ImageLoader::Symbol** sym, const ImageLoader** image, ImageLoader::CoalesceNotifier notifier)
{
	return findExportedSymbol(name, true, sym, image, notifier);
}


bool flatFindExportedSymbolWithHint(const char* name, const char* librarySubstring, const ImageLoader::Symbol** sym, const ImageLoader** image)
{
	// search all images in order
	const size_t imageCount = sAllImages.size();
	for(size_t i=0; i < imageCount; ++i){
		ImageLoader* anImage = sAllImages[i];
		// only look at images whose paths contain the hint string (NULL hint string is wildcard)
		if ( ! anImage->isBundle() && ((librarySubstring==NULL) || (strstr(anImage->getPath(), librarySubstring) != NULL)) ) {
			*sym = anImage->findExportedSymbol(name, false, image);
			if ( *sym != NULL ) {
				return true;
			}
		}
	}
	return false;
}


unsigned int getCoalescedImages(ImageLoader* images[], unsigned imageIndex[])
{
	unsigned int count = 0;
	const size_t imageCount = sAllImages.size();
	for(size_t i=0; i < imageCount; ++i) {
		ImageLoader* anImage = sAllImages[i];
		// the use of inserted libraries alters search order
		// so that inserted libraries are found before the main executable
		if ( sInsertedDylibCount > 0 ) {
			if ( i < sInsertedDylibCount )
				anImage = sAllImages[i+1];
			else if ( i == sInsertedDylibCount )
				anImage = sAllImages[0];
		}
		if ( anImage->participatesInCoalescing() ) {
			images[count] = anImage;
			imageIndex[count] = 0;
			++count;
		}
	}
#if SUPPORT_ACCELERATE_TABLES
	if ( sAllCacheImagesProxy != NULL ) {
		sAllCacheImagesProxy->appendImagesNeedingCoalescing(images, imageIndex, count);
	}
#endif
	return count;
}


static ImageLoader::MappedRegion* getMappedRegions(ImageLoader::MappedRegion* regions)
{
	ImageLoader::MappedRegion* end = regions;
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
		(*it)->getMappedRegions(end);
	}
	return end;
}

void registerImageStateSingleChangeHandler(dyld_image_states state, dyld_image_state_change_handler handler)
{
	// mark the image that the handler is in as never-unload because dyld has a reference into it
	ImageLoader* handlerImage = findImageContainingAddress((void*)handler);
	if ( handlerImage != NULL )
		handlerImage->setNeverUnload();

	// add to list of handlers
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(state, sSingleHandlers);
	if ( handlers != NULL ) {
        // <rdar://problem/10332417> need updateAllImages() to be last in dyld_image_state_mapped list
        // so that if ObjC adds a handler that prevents a load, it happens before the gdb list is updated
        if ( state == dyld_image_state_mapped )
            handlers->insert(handlers->begin(), handler);
        else
            handlers->push_back(handler);

		// call callback with all existing images
		for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
			ImageLoader* image = *it;
			dyld_image_info	 info;
			info.imageLoadAddress	= image->machHeader();
			info.imageFilePath		= image->getRealPath();
			info.imageFileModDate	= image->lastModified();
			// should only call handler if state == image->state
			if ( image->getState() == state )
				(*handler)(state, 1, &info);
			// ignore returned string, too late to do anything
		}
	}
}

void registerImageStateBatchChangeHandler(dyld_image_states state, dyld_image_state_change_handler handler)
{
	// mark the image that the handler is in as never-unload because dyld has a reference into it
	ImageLoader* handlerImage = findImageContainingAddress((void*)handler);
	if ( handlerImage != NULL )
		handlerImage->setNeverUnload();

	// add to list of handlers
	std::vector<dyld_image_state_change_handler>* handlers = stateToHandlers(state, sBatchHandlers);
	if ( handlers != NULL ) {
		// insert at front, so that gdb handler is always last
		handlers->insert(handlers->begin(), handler);
		
		// call callback with all existing images
		try {
			notifyBatchPartial(state, true, handler, false, false);
		}
		catch (const char* msg) {
			// ignore request to abort during registration
		}
	}
}


void registerObjCNotifiers(_dyld_objc_notify_mapped mapped, _dyld_objc_notify_init init, _dyld_objc_notify_unmapped unmapped)
{
	// record functions to call
	sNotifyObjCMapped	= mapped;
	sNotifyObjCInit		= init;
	sNotifyObjCUnmapped = unmapped;

	// call 'mapped' function with all images mapped so far
	try {
		notifyBatchPartial(dyld_image_state_bound, true, NULL, false, true);
	}
	catch (const char* msg) {
		// ignore request to abort during registration
	}

	// <rdar://problem/32209809> call 'init' function on all images already init'ed (below libSystem)
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
		ImageLoader* image = *it;
		if ( (image->getState() == dyld_image_state_initialized) && image->notifyObjC() ) {
			dyld3::ScopedTimer timer(DBG_DYLD_TIMING_OBJC_INIT, (uint64_t)image->machHeader(), 0, 0);
			(*sNotifyObjCInit)(image->getRealPath(), image->machHeader());
		}
	}
}

bool sharedCacheUUID(uuid_t uuid)
{
	if ( sSharedCacheLoadInfo.loadAddress == nullptr )
		return false;

	sSharedCacheLoadInfo.loadAddress->getUUID(uuid);
	return true;
}

#if SUPPORT_ACCELERATE_TABLES

bool dlopenFromCache(const char* path, int mode, void** handle)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	char fallbackPath[PATH_MAX];
	bool result = sAllCacheImagesProxy->dlopenFromCache(gLinkContext, path, mode, handle);
	if ( !result && (strchr(path, '/') == NULL) ) {
		// POSIX says you can call dlopen() with a leaf name (e.g. dlopen("libz.dylb"))
		strcpy(fallbackPath, "/usr/lib/");
		strlcat(fallbackPath, path, PATH_MAX);
		result = sAllCacheImagesProxy->dlopenFromCache(gLinkContext, fallbackPath, mode, handle);
		if ( !result )
			path = fallbackPath;
	}
	if ( !result ) {
		// leaf name could be a symlink
		char resolvedPath[PATH_MAX];
		realpath(path, resolvedPath);
		int realpathErrno = errno;
		// If realpath() resolves to a path which does not exist on disk, errno is set to ENOENT
		if ( (realpathErrno == ENOENT) || (realpathErrno == 0) ) {
			result = sAllCacheImagesProxy->dlopenFromCache(gLinkContext, resolvedPath, mode, handle);
		}
	}

	return result;
}

bool makeCacheHandle(ImageLoader* image, unsigned cacheIndex, int mode, void** result)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	return sAllCacheImagesProxy->makeCacheHandle(gLinkContext, cacheIndex, mode, result);
}

bool isCacheHandle(void* handle)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	return sAllCacheImagesProxy->isCacheHandle(handle, NULL, NULL);
}

bool isPathInCache(const char* path)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	unsigned index;
	return sAllCacheImagesProxy->hasDylib(path, &index);
}

const char* getPathFromIndex(unsigned cacheIndex)
{
	if ( sAllCacheImagesProxy == NULL )
		return NULL;
	return sAllCacheImagesProxy->getIndexedPath(cacheIndex);
}

void* dlsymFromCache(void* handle, const char* symName, unsigned index)
{
	if ( sAllCacheImagesProxy == NULL )
		return NULL;
	return sAllCacheImagesProxy->dlsymFromCache(gLinkContext, handle, symName, index);
}

bool addressInCache(const void* address, const mach_header** mh, const char** path, unsigned* index)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	unsigned ignore;
	return sAllCacheImagesProxy->addressInCache(address, mh, path, index ? index : &ignore);
}

bool findUnwindSections(const void* addr, dyld_unwind_sections* info)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	return sAllCacheImagesProxy->findUnwindSections(addr, info);
}

bool dladdrFromCache(const void* address, Dl_info* info)
{
	if ( sAllCacheImagesProxy == NULL )
		return false;
	return sAllCacheImagesProxy->dladdrFromCache(address, info);
}
#endif

static ImageLoader* libraryLocator(const char* libraryName, bool search, const char* origin, const ImageLoader::RPathChain* rpaths, unsigned& cacheIndex)
{
	dyld::LoadContext context;
	context.useSearchPaths		= search;
	context.useFallbackPaths	= search;
	context.useLdLibraryPath	= false;
	context.implicitRPath		= false;
	context.matchByInstallName	= false;
	context.dontLoad			= false;
	context.mustBeBundle		= false;
	context.mustBeDylib			= true;
	context.canBePIE			= false;
	context.origin				= origin;
	context.rpath				= rpaths;
	return load(libraryName, context, cacheIndex);
}

static const char* basename(const char* path)
{
    const char* last = path;
    for (const char* s = path; *s != '\0'; s++) {
        if (*s == '/') 
			last = s+1;
    }
    return last;
}

static void setContext(const macho_header* mainExecutableMH, int argc, const char* argv[], const char* envp[], const char* apple[])
{
	gLinkContext.loadLibrary			= &libraryLocator;
	gLinkContext.terminationRecorder	= &terminationRecorder;
	gLinkContext.flatExportFinder		= &flatFindExportedSymbol;
	gLinkContext.coalescedExportFinder	= &findCoalescedExportedSymbol;
	gLinkContext.getCoalescedImages		= &getCoalescedImages;
	gLinkContext.undefinedHandler		= &undefinedHandler;
	gLinkContext.getAllMappedRegions	= &getMappedRegions;
	gLinkContext.bindingHandler			= NULL;
	gLinkContext.notifySingle			= &notifySingle;
	gLinkContext.notifyBatch			= &notifyBatch;
	gLinkContext.removeImage			= &removeImage;
	gLinkContext.registerDOFs			= dyld3::Loader::dtraceUserProbesEnabled() ? &registerDOFs : NULL;
	gLinkContext.clearAllDepths			= &clearAllDepths;
	gLinkContext.printAllDepths			= &printAllDepths;
	gLinkContext.imageCount				= &imageCount;
	gLinkContext.setNewProgramVars		= &setNewProgramVars;
	gLinkContext.inSharedCache			= &inSharedCache;
	gLinkContext.setErrorStrings		= &setErrorStrings;
#if SUPPORT_OLD_CRT_INITIALIZATION
	gLinkContext.setRunInitialzersOldWay= &setRunInitialzersOldWay;
#endif
	gLinkContext.findImageContainingAddress	= &findImageContainingAddress;
	gLinkContext.addDynamicReference	= &addDynamicReference;
#if SUPPORT_ACCELERATE_TABLES
	gLinkContext.notifySingleFromCache	= &notifySingleFromCache;
	gLinkContext.getPreInitNotifyHandler= &getPreInitNotifyHandler;
	gLinkContext.getBoundBatchHandler   = &getBoundBatchHandler;
#endif
	gLinkContext.bindingOptions			= ImageLoader::kBindingNone;
	gLinkContext.argc					= argc;
	gLinkContext.argv					= argv;
	gLinkContext.envp					= envp;
	gLinkContext.apple					= apple;
	gLinkContext.progname				= (argv[0] != NULL) ? basename(argv[0]) : "";
	gLinkContext.programVars.mh			= mainExecutableMH;
	gLinkContext.programVars.NXArgcPtr	= &gLinkContext.argc;
	gLinkContext.programVars.NXArgvPtr	= &gLinkContext.argv;
	gLinkContext.programVars.environPtr	= &gLinkContext.envp;
	gLinkContext.programVars.__prognamePtr=&gLinkContext.progname;
	gLinkContext.mainExecutable			= NULL;
	gLinkContext.imageSuffix			= NULL;
	gLinkContext.dynamicInterposeArray	= NULL;
	gLinkContext.dynamicInterposeCount	= 0;
	gLinkContext.prebindUsage			= ImageLoader::kUseAllPrebinding;
	gLinkContext.sharedRegionMode		= ImageLoader::kUseSharedRegion;
}



//
// Look for a special segment in the mach header. 
// Its presences means that the binary wants to have DYLD ignore
// DYLD_ environment variables.
//
#if TARGET_OS_OSX
static bool hasRestrictedSegment(const macho_header* mh)
{
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_SEGMENT_COMMAND:
			{
				const struct macho_segment_command* seg = (struct macho_segment_command*)cmd;
				
				//dyld::log("seg name: %s\n", seg->segname);
				if (strcmp(seg->segname, "__RESTRICT") == 0) {
					const struct macho_section* const sectionsStart = (struct macho_section*)((char*)seg + sizeof(struct macho_segment_command));
					const struct macho_section* const sectionsEnd = &sectionsStart[seg->nsects];
					for (const struct macho_section* sect=sectionsStart; sect < sectionsEnd; ++sect) {
						if (strcmp(sect->sectname, "__restrict") == 0) 
							return true;
					}
				}
			}
			break;
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
		
	return false;
}
#endif

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
static bool isFairPlayEncrypted(const macho_header* mh)
{
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		if ( cmd->cmd == LC_ENCRYPT_COMMAND ) {
			const encryption_info_command* enc = (encryption_info_command*)cmd;
			return (enc->cryptid != 0);
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}

	return false;
}
#endif

#if SUPPORT_VERSIONED_PATHS

#define FIRST_PAGE_BUFFER_SIZE	16384

static bool readFirstPage(const char* dylibPath, uint8_t firstPage[FIRST_PAGE_BUFFER_SIZE])
{
	firstPage[0] = 0;
	// open file (automagically closed when this function exits)
	FileOpener file(dylibPath);

	if ( file.getFileDescriptor() == -1 ) 
		return false;
	
	if ( pread(file.getFileDescriptor(), firstPage, FIRST_PAGE_BUFFER_SIZE, 0) != FIRST_PAGE_BUFFER_SIZE )
		return false;

	// if fat wrapper, find usable sub-file
	const fat_header* fileStartAsFat = (fat_header*)firstPage;
	if ( fileStartAsFat->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		uint64_t fileOffset;
		uint64_t fileLength;
		if ( fatFindBest(fileStartAsFat, &fileOffset, &fileLength) ) {
			if ( pread(file.getFileDescriptor(), firstPage, FIRST_PAGE_BUFFER_SIZE, fileOffset) != FIRST_PAGE_BUFFER_SIZE )
				return false;
		}
		else {
			return false;
		}
	}
	
	return true;
}

//
// Peeks at a dylib file and returns its current_version and install_name.
// Returns false on error.
//
static bool getDylibVersionAndInstallname(const char* dylibPath, uint32_t* version, char* installName)
{
	uint8_t firstPage[FIRST_PAGE_BUFFER_SIZE];
	const macho_header* mh = (macho_header*)firstPage;
	if ( !readFirstPage(dylibPath, firstPage) ) {
		// If file cannot be read, check to see if path is in shared cache
		const macho_header* mhInCache;
		const char*			pathInCache;
		long				slideInCache;
		if ( !findInSharedCacheImage(dylibPath, true, NULL, &mhInCache, &pathInCache, &slideInCache) )
			return false;
		mh = mhInCache;
	}

	// check mach-o header
	if ( mh->magic != sMainExecutableMachHeader->magic ) 
		return false;
	if ( mh->cputype != sMainExecutableMachHeader->cputype )
		return false;

	// scan load commands for LC_ID_DYLIB
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const struct load_command* const cmdsReadEnd = (struct load_command*)(((char*)mh)+FIRST_PAGE_BUFFER_SIZE);
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_ID_DYLIB:
			{
				const struct dylib_command* id = (struct dylib_command*)cmd;
				*version = id->dylib.current_version;
				if ( installName != NULL )
					strlcpy(installName, (char *)id + id->dylib.name.offset, PATH_MAX);
				return true;
			}
			break;
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
		if ( cmd > cmdsReadEnd )
			return false;
	}
	
	return false;
}
#endif // SUPPORT_VERSIONED_PATHS
						

#if 0
static void printAllImages()
{
	dyld::log("printAllImages()\n");
	for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
		ImageLoader* image = *it;
		dyld_image_states imageState = image->getState();
		dyld::log("  state=%d, dlopen-count=%d, never-unload=%d, in-use=%d, name=%s\n",
				  imageState, image->dlopenCount(), image->neverUnload(), image->isMarkedInUse(), image->getShortName());
	}
}
#endif

void link(ImageLoader* image, bool forceLazysBound, bool neverUnload, const ImageLoader::RPathChain& loaderRPaths, unsigned cacheIndex)
{
	// add to list of known images.  This did not happen at creation time for bundles
	if ( image->isBundle() && !image->isLinked() )
		addImage(image);

	// we detect root images as those not linked in yet 
	if ( !image->isLinked() )
		addRootImage(image);
	
	// process images
	try {
		const char* path = image->getPath();
#if SUPPORT_ACCELERATE_TABLES
		if ( image == sAllCacheImagesProxy )
			path = sAllCacheImagesProxy->getIndexedPath(cacheIndex);
#endif
		image->link(gLinkContext, forceLazysBound, false, neverUnload, loaderRPaths, path);
	}
	catch (const char* msg) {
		garbageCollectImages();
		throw;
	}
}


void runInitializers(ImageLoader* image)
{
	// do bottom up initialization
	ImageLoader::InitializerTimingList initializerTimes[allImagesCount()];
	initializerTimes[0].count = 0;
	image->runInitializers(gLinkContext, initializerTimes[0]);
}

// This function is called at the end of dlclose() when the reference count goes to zero.
// The dylib being unloaded may have brought in other dependent dylibs when it was loaded.
// Those dependent dylibs need to be unloaded, but only if they are not referenced by
// something else.  We use a standard mark and sweep garbage collection.
//
// The tricky part is that when a dylib is unloaded it may have a termination function that
// can run and itself call dlclose() on yet another dylib.  The problem is that this
// sort of gabage collection is not re-entrant.  Instead a terminator's call to dlclose()
// which calls garbageCollectImages() will just set a flag to re-do the garbage collection
// when the current pass is done.
//
// Also note that this is done within the dyld global lock, so it is always single threaded.
//
void garbageCollectImages()
{
	static bool sDoingGC = false;
	static bool sRedo = false;

	if ( sDoingGC ) {
		// GC is currently being run, just set a flag to have it run again.
		sRedo = true;
		return;
	}
	
	sDoingGC = true;
	do {
		sRedo = false;
		
		// mark phase: mark all images not-in-use
		for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
			ImageLoader* image = *it;
			//dyld::log("gc: neverUnload=%d name=%s\n", image->neverUnload(), image->getShortName());
			image->markNotUsed();
		}
		
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		// sweep phase: mark as in-use, images reachable from never-unload or in-use image
		for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
			ImageLoader* image = *it;
			if ( (image->dlopenCount() != 0) || (image->neverUnload() && (image->getState() >= dyld_image_state_bound)) || (image == sMainExecutable) ) {
				OSSpinLockLock(&sDynamicReferencesLock);
					image->markedUsedRecursive(sDynamicReferences);
				OSSpinLockUnlock(&sDynamicReferencesLock);
			}
		}
#pragma clang diagnostic pop

		// collect phase: build array of images not marked in-use
		ImageLoader* deadImages[sAllImages.size()];
		unsigned deadCount = 0;
		int maxRangeCount = 0;
		for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
			ImageLoader* image = *it;
			if ( ! image->isMarkedInUse() ) {
				deadImages[deadCount++] = image;
				if (gLogAPIs) dyld::log("dlclose(), found unused image %p %s\n", image, image->getShortName());
				maxRangeCount += image->segmentCount();
			}
		}

		// collect phase: run termination routines for images not marked in-use
		if ( maxRangeCount != 0 ) {
			__cxa_range_t ranges[maxRangeCount];
			int rangeCount = 0;
			for (unsigned i=0; i < deadCount; ++i) {
				ImageLoader* image = deadImages[i];
				for (unsigned int j=0; j < image->segmentCount(); ++j) {
					if ( !image->segExecutable(j) )
						continue;
					if ( rangeCount < maxRangeCount ) {
						ranges[rangeCount].addr = (const void*)image->segActualLoadAddress(j);
						ranges[rangeCount].length = image->segSize(j);
						++rangeCount;
					}
				}
				try {
					runImageStaticTerminators(image);
				}
				catch (const char* msg) {
					dyld::warn("problem running terminators for image: %s\n", msg);
				}
			}

			// <rdar://problem/14718598> dyld should call __cxa_finalize_ranges()
			if ( (rangeCount > 0) && (gLibSystemHelpers != NULL) && (gLibSystemHelpers->version >= 13) )
				(*gLibSystemHelpers->cxa_finalize_ranges)(ranges, rangeCount);
		}

		// collect phase: delete all images which are not marked in-use
		bool mightBeMore;
		do {
			mightBeMore = false;
			for (std::vector<ImageLoader*>::iterator it=sAllImages.begin(); it != sAllImages.end(); it++) {
				ImageLoader* image = *it;
				if ( ! image->isMarkedInUse() ) {
					try {
						if (gLogAPIs) dyld::log("dlclose(), deleting %p %s\n", image, image->getShortName());
						removeImage(image);
						ImageLoader::deleteImage(image);
						mightBeMore = true;
						break;  // interator in invalidated by this removal
					}
					catch (const char* msg) {
						dyld::warn("problem deleting image: %s\n", msg);
					}
				}
			}
		} while ( mightBeMore );
	} while (sRedo);
	sDoingGC = false;

	//printAllImages();

}


static void preflight_finally(ImageLoader* image)
{
	if ( image->isBundle() ) {
		removeImageFromAllImages(image->machHeader());
		ImageLoader::deleteImage(image);
	}
	sBundleBeingLoaded = NULL;
	dyld::garbageCollectImages();
}


void preflight(ImageLoader* image, const ImageLoader::RPathChain& loaderRPaths, unsigned cacheIndex)
{
	try {
		if ( image->isBundle() ) 
			sBundleBeingLoaded = image;	// hack
		const char* path = image->getPath();
#if SUPPORT_ACCELERATE_TABLES
		if ( image == sAllCacheImagesProxy )
			path = sAllCacheImagesProxy->getIndexedPath(cacheIndex);
#endif
		image->link(gLinkContext, false, true, false, loaderRPaths, path);
	}
	catch (const char* msg) {	
		preflight_finally(image);
		throw;
	}
	preflight_finally(image);
}

static void loadInsertedDylib(const char* path)
{
	unsigned cacheIndex;
	try {
		LoadContext context;
		context.useSearchPaths		= false;
		context.useFallbackPaths	= false;
		context.useLdLibraryPath	= false;
		context.implicitRPath		= false;
		context.matchByInstallName	= false;
		context.dontLoad			= false;
		context.mustBeBundle		= false;
		context.mustBeDylib			= true;
		context.canBePIE			= false;
		context.origin				= NULL;	// can't use @loader_path with DYLD_INSERT_LIBRARIES
		context.rpath				= NULL;
		load(path, context, cacheIndex);
	}
	catch (const char* msg) {
		if ( gLinkContext.allowInsertFailures )
			dyld::log("dyld: warning: could not load inserted library '%s' into hardened process because %s\n", path, msg);
		else
			halt(dyld::mkstringf("could not load inserted library '%s' because %s\n", path, msg));
	}
	catch (...) {
		halt(dyld::mkstringf("could not load inserted library '%s'\n", path));
	}
}


static void configureProcessRestrictions(const macho_header* mainExecutableMH, const char* envp[])
{
	uint64_t amfiInputFlags = 0;
#if TARGET_OS_SIMULATOR
	amfiInputFlags |= AMFI_DYLD_INPUT_PROC_IN_SIMULATOR;
#elif TARGET_OS_OSX
	if ( hasRestrictedSegment(mainExecutableMH) )
		amfiInputFlags |= AMFI_DYLD_INPUT_PROC_HAS_RESTRICT_SEG;
#elif TARGET_OS_IPHONE
	if ( isFairPlayEncrypted(mainExecutableMH) )
		amfiInputFlags |= AMFI_DYLD_INPUT_PROC_IS_ENCRYPTED;
#endif
	uint64_t amfiOutputFlags = 0;
	const char* amfiFake = nullptr;
	if constexpr(BUILD_FOR_TESTING == 1) {
		amfiFake = _simple_getenv(envp, "DYLD_AMFI_FAKE");
	} else if ( dyld3::internalInstall() && dyld3::BootArgs::enableDyldTestMode() ) {
		amfiFake = _simple_getenv(envp, "DYLD_AMFI_FAKE");
	}

	if ( amfiFake != nullptr ) {
		amfiOutputFlags = hexToUInt64(amfiFake, nullptr);
	}
	if ( (amfiFake != nullptr) || (amfi_check_dyld_policy_self(amfiInputFlags, &amfiOutputFlags) == 0) ) {
		gLinkContext.allowAtPaths 				= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_AT_PATH);
		gLinkContext.allowEnvVarsPrint			= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_PRINT_VARS);
		gLinkContext.allowEnvVarsPath			= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_PATH_VARS);
		gLinkContext.allowEnvVarsSharedCache	= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_CUSTOM_SHARED_CACHE);
		gLinkContext.allowClassicFallbackPaths	= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_FALLBACK_PATHS);
		gLinkContext.allowInsertFailures    	= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_FAILED_LIBRARY_INSERTION);
#ifdef AMFI_RETURNS_INTERPOSING_FLAG
		gLinkContext.allowInterposing	    	= (amfiOutputFlags & AMFI_DYLD_OUTPUT_ALLOW_LIBRARY_INTERPOSING);
#else
		gLinkContext.allowInterposing	    	= true;
#endif
	}
	else {
#if TARGET_OS_OSX
		// support chrooting from old kernel
		bool isRestricted = false;
		bool libraryValidation = false;
		// any processes with setuid or setgid bit set or with __RESTRICT segment is restricted
		if ( issetugid() || hasRestrictedSegment(mainExecutableMH) ) {
			isRestricted = true;
		}
		bool usingSIP = (csr_check(CSR_ALLOW_TASK_FOR_PID) != 0);
		uint32_t flags;
		if ( csops(0, CS_OPS_STATUS, &flags, sizeof(flags)) != -1 ) {
			// On OS X CS_RESTRICT means the program was signed with entitlements
			if ( ((flags & CS_RESTRICT) == CS_RESTRICT) && usingSIP ) {
				isRestricted = true;
			}
			// Library Validation loosens searching but requires everything to be code signed
			if ( flags & CS_REQUIRE_LV ) {
				isRestricted = false;
				libraryValidation = true;
			}
		}
		gLinkContext.allowAtPaths                = !isRestricted;
		gLinkContext.allowEnvVarsPrint           = !isRestricted;
		gLinkContext.allowEnvVarsPath            = !isRestricted;
		gLinkContext.allowEnvVarsSharedCache     = !libraryValidation || !usingSIP;
		gLinkContext.allowClassicFallbackPaths   = !isRestricted;
		gLinkContext.allowInsertFailures         = false;
		gLinkContext.allowInterposing         	 = true;
#else
		halt("amfi_check_dyld_policy_self() failed\n");
#endif
	}
}

// called by _dyld_register_driverkit_main()
void setMainEntry(void (*main)())
{
	if ( sEntryOverride == nullptr )
		sEntryOverride = main;
	else
		halt("_dyld_register_driverkit_main() may only be called once");
}

bool processIsRestricted()
{
#if TARGET_OS_OSX
	return !gLinkContext.allowEnvVarsPath;
#else
	return false;
#endif
}


// <rdar://problem/10583252> Add dyld to uuidArray to enable symbolication of stackshots
static void addDyldImageToUUIDList()
{
	const struct macho_header* mh = (macho_header*)&__dso_handle;
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)((char*)mh + sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_UUID: {
				uuid_command* uc = (uuid_command*)cmd;
				dyld_uuid_info info;
				info.imageLoadAddress = (mach_header*)mh;
				memcpy(info.imageUUID, uc->uuid, 16);
				addNonSharedCacheImageUUID(info);
				return;
			}
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
}

void notifyKernelAboutImage(const struct macho_header* mh, const char* fileInfo)
{
	const char *endptr = nullptr;
	uint64_t tmp = hexToUInt64(fileInfo, &endptr);
	fsid_t fsid = *reinterpret_cast<fsid_t *>(&tmp);
	uint64_t fsobj_id_scalar = 0;
	fsobj_id_t fsobj_id = {0};
	if (endptr != nullptr) {
		fsobj_id_scalar = hexToUInt64(endptr+1, &endptr);
		fsobj_id = *reinterpret_cast<fsobj_id_t *>(&fsobj_id_scalar);
	}
	const uint32_t cmd_count = mh->ncmds;
	const struct load_command* const cmds = (struct load_command*)((char*)mh + sizeof(macho_header));
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_UUID: {
				// Add dyld to the kernel image info
				uuid_command* uc = (uuid_command*)cmd;
				char path[MAXPATHLEN];
				if (fsgetpath(path, MAXPATHLEN, &fsid, fsobj_id_scalar) < 0) {
					path[0] = 0;
				}
				dyld3::kdebug_trace_dyld_image(DBG_DYLD_UUID_MAP_A, path, (const uuid_t *)&uc->uuid[0], fsobj_id, fsid, (const mach_header *)mh);
				return;
			}
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
}

#if TARGET_OS_OSX
static void* getProcessInfo() { return dyld::gProcessInfo; }
static const SyscallHelpers sSysCalls = {
		14,
		// added in version 1
		&open,
		&close, 
		&pread, 
		&write, 
		&mmap, 
		&munmap, 
		&madvise,
		&stat, 
		&fcntl,
		&ioctl, 
		&issetugid, 
		&getcwd, 
		&realpath, 
		&vm_allocate, 
		&vm_deallocate,
		&vm_protect,
		&vlog, 
		&vwarn, 
		&pthread_mutex_lock, 
		&pthread_mutex_unlock,
		&mach_thread_self, 
		&mach_port_deallocate, 
		&task_self_trap,
		&mach_timebase_info,
		&OSAtomicCompareAndSwapPtrBarrier, 
		&OSMemoryBarrier,
		&getProcessInfo,
		&__error,
		&mach_absolute_time,
		// added in version 2
		&thread_switch,
		// added in version 3
		&opendir,
		&readdir_r,
		&closedir,
		// added in version 4
		&coresymbolication_load_notifier,
		&coresymbolication_unload_notifier,
		// Added in version 5
		&proc_regionfilename,
		&getpid,
		&mach_port_insert_right,
		&mach_port_allocate,
		&mach_msg_sim_interposed,
		// Added in version 6
		&abort_with_payload,
		// Added in version 7
		&legacy_task_register_dyld_image_infos,
		&legacy_task_unregister_dyld_image_infos,
		&legacy_task_get_dyld_image_infos,
		&legacy_task_register_dyld_shared_cache_image_info,
		&legacy_task_register_dyld_set_dyld_state,
		&legacy_task_register_dyld_get_process_state,
		// Added in version 8
		&task_info,
		&thread_info,
		&kdebug_is_enabled,
		&kdebug_trace,
		// Added in version 9
		&kdebug_trace_string,
		// Added in version 10
		&amfi_check_dyld_policy_self,
		// Added in version 11
		&notifyMonitoringDyldMain,
		&notifyMonitoringDyld,
		// Add in version 12
		&mach_msg_destroy,
		&mach_port_construct,
		&mach_port_destruct,
		// Added in version 13
		&fstat,
		&vm_copy,
		// Added in version 14
		&task_dyld_process_info_notify_get
};

__attribute__((noinline))
static const char* useSimulatorDyld(int fd, const macho_header* mainExecutableMH, const char* dyldPath,
								int argc, const char* argv[], const char* envp[], const char* apple[],
								uintptr_t* startGlue, uintptr_t* mainAddr)
{
	*startGlue = 0;
	*mainAddr = 0;

	// <rdar://problem/25311921> simulator does not support restricted processes
	uint32_t flags;
	if ( csops(0, CS_OPS_STATUS, &flags, sizeof(flags)) == -1 )
		return "csops() failed";
	if ( (flags & CS_RESTRICT) == CS_RESTRICT )
		return "dyld_sim cannot be loaded in a restricted process";
	if ( issetugid() )
		return "dyld_sim cannot be loaded in a setuid process";
	if ( hasRestrictedSegment(mainExecutableMH) )
		return "dyld_sim cannot be loaded in a restricted process";

	// get file size of dyld_sim
	struct stat sb;
	if ( fstat(fd, &sb) == -1 )
		return "stat(dyld_sim) failed";

	// read first page of dyld_sim file
	uint8_t firstPage[4096];
	if ( pread(fd, firstPage, 4096, 0) != 4096 )
		return "pread(dyld_sim) failed";

	// if fat file, pick matching slice
	uint64_t fileOffset = 0;
	uint64_t fileLength = sb.st_size;
	const fat_header* fileStartAsFat = (fat_header*)firstPage;
	if ( fileStartAsFat->magic == OSSwapBigToHostInt32(FAT_MAGIC) ) {
		if ( !fatFindBest(fileStartAsFat, &fileOffset, &fileLength) ) 
			return "no matching arch in dyld_sim";
		// re-read buffer from start of mach-o slice in fat file
		if ( pread(fd, firstPage, 4096, fileOffset) != 4096 )
			return "pread(dyld_sim) failed";
	}
	else if ( !isCompatibleMachO(firstPage, dyldPath, fd, fileOffset, fileLength) ) {
		return "dyld_sim is not compatible with the loaded process, likely due to architecture mismatch";
	}
	
	// calculate total size of dyld segments
	const macho_header* mh = (const macho_header*)firstPage;
	struct macho_segment_command* lastSeg = NULL;
	struct macho_segment_command* firstSeg = NULL;
	uintptr_t mappingSize = 0;
	uintptr_t preferredLoadAddress = 0;
	const uint32_t cmd_count = mh->ncmds;
	if ( mh->sizeofcmds > 4096 )
		return "dyld_sim load commands to large";
	if ( (sizeof(macho_header) + mh->sizeofcmds) > 4096 )
		return "dyld_sim load commands to large";
	struct linkedit_data_command* codeSigCmd = NULL;
	const struct load_command* const cmds = (struct load_command*)(((char*)mh)+sizeof(macho_header));
	const struct load_command* const endCmds = (struct load_command*)(((char*)mh) + sizeof(macho_header) + mh->sizeofcmds);
	const struct load_command* cmd = cmds;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		uint32_t cmdLength = cmd->cmdsize;
		if ( cmdLength < 8 )
			return "dyld_sim load command too small";
		const struct load_command* const nextCmd = (const struct load_command*)(((char*)cmd)+cmdLength);
		if ( (nextCmd > endCmds) || (nextCmd < cmd) )
			return "dyld_sim load command too large";
		switch (cmd->cmd) {
			case LC_SEGMENT_COMMAND:
				{
					struct macho_segment_command* seg = (struct macho_segment_command*)cmd;
					if ( seg->vmaddr + seg->vmsize < seg->vmaddr )
						return "dyld_sim seg wraps address space";
					if ( seg->vmsize < seg->filesize )
						return "dyld_sim seg vmsize too small";
					if ( (seg->fileoff + seg->filesize) < seg->fileoff )
						return "dyld_sim seg size wraps address space";
					if ( lastSeg == NULL ) {
						// first segment must be __TEXT and start at beginning of file/slice
						firstSeg = seg;
						if ( strcmp(seg->segname, "__TEXT") != 0 )
							return "dyld_sim first segment not __TEXT";
						if ( seg->fileoff != 0 )
							return "dyld_sim first segment not at file offset zero";
						if ( seg->filesize < (sizeof(macho_header) + mh->sizeofcmds) )
							return "dyld_sim first segment smaller than load commands";
						preferredLoadAddress = seg->vmaddr;
					}
					else {
						// other sements must be continguous with previous segment and not executable
						if ( lastSeg->fileoff + lastSeg->filesize != seg->fileoff )
							return "dyld_sim segments not contiguous";
						if ( lastSeg->vmaddr + lastSeg->vmsize != seg->vmaddr )
							return "dyld_sim segments not address contiguous";
						if ( (seg->initprot & VM_PROT_EXECUTE) != 0 )
							return "dyld_sim non-first segment is executable";
					}
					mappingSize += seg->vmsize;
					lastSeg = seg;
				}
				break;
			case LC_SEGMENT_COMMAND_WRONG:
				return "dyld_sim wrong load segment load command";
			case LC_CODE_SIGNATURE:
				codeSigCmd = (struct linkedit_data_command*)cmd;
				break;
		}
		cmd = nextCmd;
	}
	// last segment must be named __LINKEDIT and not writable
	if ( lastSeg == NULL )
		return "dyld_sim has no segments";
	if ( strcmp(lastSeg->segname, "__LINKEDIT") != 0 )
		return "dyld_sim last segment not __LINKEDIT";
	if ( lastSeg->initprot & VM_PROT_WRITE )
		return "dyld_sim __LINKEDIT segment writable";

	// must have code signature which is contained within LINKEDIT segment
	if ( codeSigCmd == NULL )
		return "dyld_sim not code signed";
	if ( codeSigCmd->dataoff < lastSeg->fileoff )
		return "dyld_sim code signature not in __LINKEDIT";
	if ( (codeSigCmd->dataoff + codeSigCmd->datasize) <  codeSigCmd->dataoff )
		return "dyld_sim code signature size wraps";
	if ( (codeSigCmd->dataoff + codeSigCmd->datasize) > (lastSeg->fileoff + lastSeg->filesize) )
		return "dyld_sim code signature extends beyond __LINKEDIT";

	// register code signature with kernel before mmap()ing segments
	fsignatures_t siginfo;
	siginfo.fs_file_start=fileOffset;							// start of mach-o slice in fat file
	siginfo.fs_blob_start=(void*)(long)(codeSigCmd->dataoff);	// start of code-signature in mach-o file
	siginfo.fs_blob_size=codeSigCmd->datasize;					// size of code-signature
	int result = fcntl(fd, F_ADDFILESIGS_FOR_DYLD_SIM, &siginfo);
	if ( result == -1 ) {
		return mkstringf("dyld_sim fcntl(F_ADDFILESIGS_FOR_DYLD_SIM) failed with errno=%d", errno);
	}
	// file range covered by code signature must extend up to code signature itself
	if ( siginfo.fs_file_start < codeSigCmd->dataoff )
		return mkstringf("dyld_sim code signature does not cover all of dyld_sim. Signature covers up to 0x%08lX. Signature starts at 0x%08X", (unsigned long)siginfo.fs_file_start, codeSigCmd->dataoff);

	// reserve space, then mmap each segment
	vm_address_t loadAddress = 0;
	if ( ::vm_allocate(mach_task_self(), &loadAddress, mappingSize, VM_FLAGS_ANYWHERE) != 0 )
		return "dyld_sim cannot allocate space";
	cmd = cmds;
	struct source_version_command*	dyldVersionCmd = NULL;
	struct uuid_command*			uuidCmd = NULL;
	for (uint32_t i = 0; i < cmd_count; ++i) {
		switch (cmd->cmd) {
			case LC_SEGMENT_COMMAND:
				{
					struct macho_segment_command* seg = (struct macho_segment_command*)cmd;
					uintptr_t requestedLoadAddress = seg->vmaddr - preferredLoadAddress + loadAddress;
					void* segAddress = ::mmap((void*)requestedLoadAddress, seg->filesize, seg->initprot, MAP_FIXED | MAP_PRIVATE, fd, fileOffset + seg->fileoff);
					//dyld::log("dyld_sim %s mapped at %p\n", seg->segname, segAddress);
					if ( segAddress == (void*)(-1) )
						return "dyld_sim mmap() of segment failed";
					if ( ((uintptr_t)segAddress < loadAddress) || ((uintptr_t)segAddress+seg->filesize > loadAddress+mappingSize) )
						return "dyld_sim mmap() to wrong location";
				}
				break;
			case LC_SOURCE_VERSION:
				dyldVersionCmd = (struct source_version_command*)cmd;
				break;
			case LC_UUID: {
				uuidCmd = (uuid_command*)cmd;
				break;
			}
		}
		cmd = (const struct load_command*)(((char*)cmd)+cmd->cmdsize);
	}
	close(fd);

	// Walk newly mapped dyld_sim load commands to find entry point
	uintptr_t entry = 0;
	bool unusedUsesCRT = false;
	uint64_t entryOffset = 0;
	if ( !((dyld3::MachOAnalyzer*)loadAddress)->getEntry(entryOffset, unusedUsesCRT) ) {
		return "dyld_sim entry not found";
	}

	// Translate the load address by the entry offset in order to get the runtime address.
	entry = (uintptr_t)loadAddress;
	entry += entryOffset;

#if __arm64e__
	// It's necessary to sign the entry pointer.
	entry = (uint64_t)__builtin_ptrauth_sign_unauthenticated((void*)entry, ptrauth_key_asia, 0);
#endif

	// notify debugger that dyld_sim is loaded
	dyld_image_info info;
	info.imageLoadAddress = (mach_header*)loadAddress;
	info.imageFilePath	  = strdup(dyldPath);
	info.imageFileModDate = sb.st_mtime;
	addImagesToAllImages(1, &info);
	dyld::gProcessInfo->notification(dyld_image_adding, 1, &info);

	fsid_t fsid = {{0, 0}};
	fsobj_id_t fsobj = {0};
	ino_t inode = sb.st_ino;
	fsobj.fid_objno = (uint32_t)inode;
	fsobj.fid_generation = (uint32_t)(inode>>32);
	fsid.val[0] = sb.st_dev;
	dyld3::kdebug_trace_dyld_image(DBG_DYLD_UUID_MAP_A, dyldPath, (const uuid_t *)&uuidCmd->uuid[0], fsobj, fsid, (const mach_header *)loadAddress);

	const char** appleParams = apple;

	// <rdar://problem/5077374> have host dyld detach macOS shared cache from process before jumping into dyld_sim
	dyld3::deallocateExistingSharedCache();

	// jump into new simulator dyld
	typedef uintptr_t (*sim_entry_proc_t)(int argc, const char* argv[], const char* envp[], const char* apple[],
								const macho_header* mainExecutableMH, const macho_header* dyldMH, uintptr_t dyldSlide,
								const dyld::SyscallHelpers* vtable, uintptr_t* startGlue);
	sim_entry_proc_t newDyld = (sim_entry_proc_t)entry;
	*mainAddr = (*newDyld)(argc, argv, envp, appleParams, mainExecutableMH, (macho_header*)loadAddress,
					 loadAddress - preferredLoadAddress, 
					 &sSysCalls, startGlue);
	return NULL;
}
#endif

// 
// If the DYLD_SKIP_MAIN environment is set to 1, dyld will return the 
// address of this function instead of main() in the target program which 
// __dyld_start jumps to. Useful for qualifying dyld itself.
//
int
fake_main()
{
	return 0;
}



#if !TARGET_OS_SIMULATOR

static bool envVarMatches(const dyld3::closure::LaunchClosure* mainClosure, const char* envp[], const char* varName)
{
	__block const char* valueFromClosure = nullptr;
	mainClosure->forEachEnvVar(^(const char* keyEqualValue, bool& stop) {
		size_t keyLen = strlen(varName);
		if ( (strncmp(varName, keyEqualValue, keyLen) == 0) && (keyEqualValue[keyLen] == '=') ) {
			valueFromClosure = &keyEqualValue[keyLen+1];
			stop = true;
		}
	});

	const char* valueFromEnv = _simple_getenv(envp, varName);

	bool inClosure = (valueFromClosure != nullptr);
	bool inEnv     = (valueFromEnv != nullptr);
	if ( inClosure != inEnv )
		return false;
	if ( !inClosure && !inEnv )
		return true;
	return ( strcmp(valueFromClosure, valueFromEnv) == 0 );
}

static const char* const sEnvVarsToCheck[] = {
	"DYLD_LIBRARY_PATH",
	"DYLD_FRAMEWORK_PATH",
	"DYLD_FALLBACK_LIBRARY_PATH",
	"DYLD_FALLBACK_FRAMEWORK_PATH",
	"DYLD_INSERT_LIBRARIES",
	"DYLD_IMAGE_SUFFIX",
	"DYLD_VERSIONED_FRAMEWORK_PATH",
	"DYLD_VERSIONED_LIBRARY_PATH",
	"DYLD_ROOT_PATH"
};

static bool envVarsMatch(const dyld3::closure::LaunchClosure* mainClosure, const char* envp[])
{
	for (const char* envVar : sEnvVarsToCheck) {
		if ( !envVarMatches(mainClosure, envp, envVar) ) {
			if ( gLinkContext.verboseWarnings )
				dyld::log("dyld: closure %p not used because %s changed\n", mainClosure, envVar);
			return false;
		}
	}

	// FIXME: dyld3 doesn't support versioned paths so we need to fall back to dyld2 if we have them.
	// <rdar://problem/37004660> dyld3: support DYLD_VERSIONED_*_PATHs ?
	if ( sEnv.DYLD_VERSIONED_LIBRARY_PATH != nullptr ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because DYLD_VERSIONED_LIBRARY_PATH used\n", mainClosure);
		return false;
	}
	if ( sEnv.DYLD_VERSIONED_FRAMEWORK_PATH != nullptr ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because DYLD_VERSIONED_FRAMEWORK_PATH used\n", mainClosure);
		return false;
	}

	return true;
}

static bool closureValid(const dyld3::closure::LaunchClosure* mainClosure, const dyld3::closure::LoadedFileInfo& mainFileInfo,
						 const uint8_t* mainExecutableCDHash, bool closureInCache, const char* envp[])
{
	if ( closureInCache ) {
		// We can only use the cache closure if the cache version is the same as dyld
		if (sSharedCacheLoadInfo.loadAddress->header.formatVersion != dyld3::closure::kFormatVersion) {
			if ( gLinkContext.verboseWarnings )
				dyld::log("dyld: dyld closure version 0x%08X does not match dyld cache version 0x%08X\n",
						  dyld3::closure::kFormatVersion, sSharedCacheLoadInfo.loadAddress->header.formatVersion);
			return false;
		}
		if (sForceInvalidSharedCacheClosureFormat) {
			if ( gLinkContext.verboseWarnings )
				dyld::log("dyld: closure %p dyld cache version forced invalid\n", mainClosure);
			return false;
		}
	} else {
		// verify current dyld cache is same as expected
		uuid_t expectedCacheUUID;
		if ( mainClosure->builtAgainstDyldCache(expectedCacheUUID) ) {
			if ( sSharedCacheLoadInfo.loadAddress == nullptr ) {
				if ( gLinkContext.verboseWarnings )
					dyld::log("dyld: closure %p dyld cache not loaded\n", mainClosure);
				return false;
			}
			else {
				uuid_t actualCacheUUID;
				sSharedCacheLoadInfo.loadAddress->getUUID(actualCacheUUID);
				if ( memcmp(expectedCacheUUID, actualCacheUUID, sizeof(uuid_t)) != 0 ) {
					if ( gLinkContext.verboseWarnings )
						dyld::log("dyld: closure %p not used because built against different dyld cache\n", mainClosure);
					return false;
				}
			}
		}
		else {
			// closure built assume there is no dyld cache
			if ( sSharedCacheLoadInfo.loadAddress != nullptr ) {
				if ( gLinkContext.verboseWarnings )
					dyld::log("dyld: closure %p built expecting no dyld cache\n", mainClosure);
				return false;
			}
		}
	}

	// verify all mach-o files have not changed since closure was built
	__block bool foundFileThatInvalidatesClosure = false;
	mainClosure->images()->forEachImage(^(const dyld3::closure::Image* image, bool& stop) {
		__block uint64_t expectedInode;
		__block uint64_t expectedMtime;
		if ( image->hasFileModTimeAndInode(expectedInode, expectedMtime) ) {
			struct stat statBuf;
			if ( dyld3::stat(image->path(), &statBuf) == 0 ) {
				if ( (statBuf.st_mtime != expectedMtime) || (statBuf.st_ino != expectedInode) ) {
					if ( gLinkContext.verboseWarnings )
						dyld::log("dyld: closure %p not used because mtime/inode for '%s' has changed since closure was built\n", mainClosure, image->path());
					foundFileThatInvalidatesClosure = true;
					stop = true;
				}
			}
			else {
				if ( gLinkContext.verboseWarnings )
					dyld::log("dyld: closure %p not used because '%s' is needed by closure but is missing\n", mainClosure, image->path());
				foundFileThatInvalidatesClosure = true;
				stop = true;
			}
		}
	});
	if ( foundFileThatInvalidatesClosure )
		return false;

	// verify cdHash of main executable is same as recorded in closure
	const dyld3::closure::Image* mainImage = mainClosure->topImage();

	__block bool foundCDHash = false;
	__block bool foundValidCDHash = false;
	mainImage->forEachCDHash(^(const uint8_t *expectedHash, bool& stop) {
		if ( mainExecutableCDHash == nullptr ) {
			if ( gLinkContext.verboseWarnings )
				dyld::log("dyld: closure %p not used because main executable is not code signed but was expected to be\n", mainClosure);
			stop = true;
			return;
		}
		foundCDHash = true;
		if ( memcmp(mainExecutableCDHash, expectedHash, 20) == 0 ) {
			// found a match, so lets use this one.
			foundValidCDHash = true;
			stop = true;
			return;
		}
	});

	// If we found cd hashes, but they were all invalid, then print them out
	if ( foundCDHash && !foundValidCDHash ) {
		auto getCDHashString = [](const uint8_t cdHash[20], char* cdHashBuffer) {
			for (int i=0; i < 20; ++i) {
				uint8_t byte = cdHash[i];
				uint8_t nibbleL = byte & 0x0F;
				uint8_t nibbleH = byte >> 4;
				if ( nibbleH < 10 ) {
					*cdHashBuffer = '0' + nibbleH;
					++cdHashBuffer;
				} else {
					*cdHashBuffer = 'a' + (nibbleH-10);
					++cdHashBuffer;
				}
				if ( nibbleL < 10 ) {
					*cdHashBuffer = '0' + nibbleL;
					++cdHashBuffer;
				} else {
					*cdHashBuffer = 'a' + (nibbleL-10);
					++cdHashBuffer;
				}
			}
		};
		if ( gLinkContext.verboseWarnings ) {
			mainImage->forEachCDHash(^(const uint8_t *expectedHash, bool &stop) {
				char mainExecutableCDHashBuffer[128] = { '\0' };
				char expectedCDHashBuffer[128] = { '\0' };

				getCDHashString(mainExecutableCDHash, mainExecutableCDHashBuffer);
				getCDHashString(expectedHash, expectedCDHashBuffer);

				dyld::log("dyld: closure %p not used because main executable cd-hash (%s) changed since closure was built with (%s)\n",
						  mainClosure, mainExecutableCDHashBuffer, expectedCDHashBuffer);
			});
		}

		return false;
	}
	
	// verify UUID of main executable is same as recorded in closure
	uuid_t expectedUUID;
	bool hasExpect = mainImage->getUuid(expectedUUID);
	uuid_t actualUUID;
	const dyld3::MachOLoaded* mainExecutableMH = (const dyld3::MachOLoaded*)mainFileInfo.fileContent;
	bool hasActual = mainExecutableMH->getUuid(actualUUID);
	if ( hasExpect != hasActual ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because UUID of executable changed since closure was built\n", mainClosure);
		return false;
	}
	if ( hasExpect && hasActual && memcmp(actualUUID, expectedUUID, sizeof(uuid_t)) != 0 ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because UUID of executable changed since closure was built\n", mainClosure);
		return false;
	}

	// verify DYLD_* env vars are same as when closure was built
	if ( !envVarsMatch(mainClosure, envp) ) {
		return false;
	}

	// verify files that are supposed to be missing actually are missing
	mainClosure->forEachMustBeMissingFile(^(const char* path, bool& stop) {
		struct stat statBuf;
		if ( dyld3::stat(path, &statBuf) == 0 ) {
			stop = true;
			foundFileThatInvalidatesClosure = true;
			if ( gLinkContext.verboseWarnings )
				dyld::log("dyld: closure %p not used because found unexpected file '%s'\n", mainClosure, path);
		}
	});

	// verify files that are supposed to exist are there with the
	mainClosure->forEachSkipIfExistsFile(^(const dyld3::closure::LaunchClosure::SkippedFile &file, bool &stop) {
		struct stat statBuf;
		if ( dyld3::stat(file.path, &statBuf) == 0 ) {
			if ( (statBuf.st_mtime != file.mtime) || (statBuf.st_ino != file.inode) ) {
				if ( gLinkContext.verboseWarnings )
					dyld::log("dyld: closure %p not used because mtime/inode for '%s' has changed since closure was built\n", mainClosure, file.path);
				foundFileThatInvalidatesClosure = true;
				stop = true;
			}
		}
	});

	// verify closure did not require anything unavailable
	if ( mainClosure->usedAtPaths() && !gLinkContext.allowAtPaths ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because is used @paths, but process does not allow that\n", mainClosure);
		return false;
	}
	if ( mainClosure->usedFallbackPaths() && !gLinkContext.allowClassicFallbackPaths ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because is used default fallback paths, but process does not allow that\n", mainClosure);
		return false;
	}
	if ( mainClosure->usedInterposing() && !gLinkContext.allowInterposing ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: closure %p not used because is uses interposing, but process does not allow that\n", mainClosure);
		return false;
	}
	return !foundFileThatInvalidatesClosure;
}

static bool nolog(const char* format, ...)
{
	return false;
}

static bool dolog(const char* format, ...)
{
	va_list	list;
	va_start(list, format);
	vlog(format, list);
	va_end(list);
	return true;
}

static bool launchWithClosure(const dyld3::closure::LaunchClosure* mainClosure,
							  const DyldSharedCache* dyldCache,
							  const dyld3::MachOLoaded* mainExecutableMH, uintptr_t mainExecutableSlide,
							  int argc, const char* argv[], const char* envp[], const char* apple[], Diagnostics& diag,
							  uintptr_t* entry, uintptr_t* startGlue, bool* closureOutOfDate, bool* recoverable)
{
	*closureOutOfDate = false;
	*recoverable      = true;

	// build list of all known ImageArrays (at most three: cached dylibs, other OS dylibs, and main prog)
	STACK_ALLOC_ARRAY(const dyld3::closure::ImageArray*, imagesArrays, 3);
	const dyld3::closure::ImageArray* mainClosureImages = mainClosure->images();
	if ( dyldCache != nullptr ) {
		imagesArrays.push_back(dyldCache->cachedDylibsImageArray());
		if ( auto others = dyldCache->otherOSImageArray() )
			imagesArrays.push_back(others);
	}
	imagesArrays.push_back(mainClosureImages);

	// allocate space for Array<LoadedImage>
	STACK_ALLOC_ARRAY(dyld3::LoadedImage, allImages, mainClosure->initialLoadCount());
	STACK_ALLOC_ARRAY(dyld3::LoadedImage, noImages, 1);

	// Get the pre-optimized Objective-C so that we can bind the selectors
	const dyld3::closure::ObjCSelectorOpt* 					selectorOpt = nullptr;
	dyld3::Array<dyld3::closure::Image::ObjCSelectorImage> 	selectorImages;
	mainClosure->selectorHashTable(selectorImages, selectorOpt);

	__block dyld3::Loader loader(noImages, allImages, dyldCache, imagesArrays,
								 selectorOpt, selectorImages, sRootsChecker,
								 (dyld3::Platform)gProcessInfo->platform,
								 (gLinkContext.verboseLoading ? &dolog : &nolog),
								 (gLinkContext.verboseMapping ? &dolog : &nolog),
								 (gLinkContext.verboseBind    ? &dolog : &nolog),
								 (gLinkContext.verboseDOF     ? &dolog : &nolog),
								 (sClosureKind == ClosureKind::minimal),
								 (dyld3::LaunchErrorInfo*)&gProcessInfo->errorKind);
	dyld3::closure::ImageNum mainImageNum = mainClosure->topImageNum();
	mainClosureImages->forEachImage(^(const dyld3::closure::Image* image, bool& stop) {
		if ( image->imageNum() == mainImageNum ) {
			// add main executable (which is already mapped by kernel) to list
			dyld3::LoadedImage mainLoadedImage = dyld3::LoadedImage::make(image, mainExecutableMH);
			mainLoadedImage.setState(dyld3::LoadedImage::State::mapped);
			mainLoadedImage.markLeaveMapped();
			loader.addImage(mainLoadedImage);
			stop = true;
		}
		else {
			// add inserted library to initial list
			loader.addImage(dyld3::LoadedImage::make(image));
		}
	});

	// recursively load all dependents and fill in allImages array
	bool someCacheImageOverridden = false;
	loader.completeAllDependents(diag, someCacheImageOverridden);
	if ( diag.noError() )
		loader.mapAndFixupAllImages(diag, dyld3::Loader::dtraceUserProbesEnabled(), false, closureOutOfDate, recoverable);
	if ( diag.hasError() ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: %s\n", diag.errorMessage());
		if ( !*recoverable ) {
			// we won't make it to libDyldEntry, so the image list will never be set up
			// hack together an image list here so crash reports show the binaries involved
			__block unsigned loadImageCount = 0;
			loader.forEachImage(^(const dyld3::LoadedImage& li, bool& stop) {
				++loadImageCount;
			});
			dyld_image_info* tempArray = new dyld_image_info[loadImageCount];
			__block unsigned i = 0;
			loader.forEachImage(^(const dyld3::LoadedImage& li, bool& stop) {
				tempArray[i].imageFilePath    = li.image()->path();
				tempArray[i].imageLoadAddress = li.loadedAddress();
				tempArray[i].imageFileModDate = 0;
				++i;
			});
			dyld::gProcessInfo->infoArray        = tempArray;
			dyld::gProcessInfo->infoArrayCount   = loadImageCount;
			dyld::gProcessInfo->initialImageCount= loadImageCount;
			dyld::gProcessInfo->infoArrayChangeTimestamp = mach_absolute_time();
		}
		return false;
	}

	//dyld::log("loaded image list:\n");
	//for (const dyld3::LoadedImage& info : allImages) {
	//	dyld::log("mh=%p, path=%s\n", info.loadedAddress(), info.image()->path());
	//}

	// find libdyld entry
	dyld3::closure::Image::ResolvedSymbolTarget dyldEntry;
	mainClosure->libDyldEntry(dyldEntry);
	const dyld3::LibDyldEntryVector* libDyldEntry = (dyld3::LibDyldEntryVector*)loader.resolveTarget(dyldEntry);

	// Set the logging function first so that libdyld can log from inside all other entry vector functions
#if !TARGET_OS_SIMULATOR
	if ( libDyldEntry->vectorVersion > 3 )
		libDyldEntry->setLogFunction(&dyld::vlog);
#endif

	// send info on all images to libdyld.dylb
	libDyldEntry->setVars(mainExecutableMH, argc, argv, envp, apple, sKeysDisabled, sOnlyPlatformArm64e, gEnableSharedCacheDataConst);
#if TARGET_OS_OSX
	uint32_t progVarsOffset;
	if ( mainClosure->hasProgramVars(progVarsOffset) ) {
		if ( libDyldEntry->vectorVersion >= 8 ) {
			// main executable contains globals to hold argc, argv, envp, and progname, but they need to be filled in
			ProgramVars* vars = (ProgramVars*)((uint8_t*)mainExecutableMH + progVarsOffset);
			*vars->NXArgcPtr 	= argc;
			*vars->NXArgvPtr 	= argv;
			*vars->environPtr 	= envp;
			*vars->__prognamePtr = (argv[0] != NULL) ? basename(argv[0]) : "";
			// set up so libSystem gets ProgramVars struct embedded in main executable
			libDyldEntry->setProgramVars(vars);
		}
	}
#endif
	if ( libDyldEntry->vectorVersion > 4 )
		libDyldEntry->setRestrictions(gLinkContext.allowAtPaths, gLinkContext.allowEnvVarsPath, gLinkContext.allowClassicFallbackPaths);
	libDyldEntry->setHaltFunction(&halt);
	if ( libDyldEntry->vectorVersion > 5 ) {
		libDyldEntry->setNotifyMonitoringDyldMain(&notifyMonitoringDyldMain);
		libDyldEntry->setNotifyMonitoringDyld(&notifyMonitoringDyld);
	}

	if ( libDyldEntry->vectorVersion > 6 )
		libDyldEntry->setHasCacheOverrides(someCacheImageOverridden);

	if ( libDyldEntry->vectorVersion > 2 )
		libDyldEntry->setChildForkFunction(&_dyld_fork_child);
	if ( libDyldEntry->vectorVersion >= 9 )
		libDyldEntry->setLaunchMode(sLaunchModeUsed);


	libDyldEntry->setOldAllImageInfo(gProcessInfo);
	dyld3::LoadedImage* libSys = loader.findImage(mainClosure->libSystemImageNum());
	libDyldEntry->setInitialImageList(mainClosure, dyldCache, sSharedCacheLoadInfo.path, allImages, *libSys,
									  mach_task_self());
	// run initializers
	CRSetCrashLogMessage("dyld3: launch, running initializers");
	libDyldEntry->runInitialzersBottomUp((mach_header*)mainExecutableMH);
	//dyld::log("returned from runInitialzersBottomUp()\n");

	if (dyld3::kdebug_trace_dyld_enabled(DBG_DYLD_TIMING_LAUNCH_EXECUTABLE)) {
		dyld3::kdebug_trace_dyld_duration_end(launchTraceID, DBG_DYLD_TIMING_LAUNCH_EXECUTABLE, 0, 0, 3);
	}
#if TARGET_OS_OSX
	if ( gLinkContext.driverKit ) {
		if (libDyldEntry->vectorVersion >= 10)
			*entry = (uintptr_t)libDyldEntry->getDriverkitMain();
		if ( *entry == 0 )
			halt("no entry point registered");
		if ( sClosureKind != ClosureKind::minimal )
			halt("driverkit process should run with minimal closures");
		*startGlue = (uintptr_t)(libDyldEntry->startFunc);
	}
	else
#endif
	{
		dyld3::closure::Image::ResolvedSymbolTarget progEntry;
		if ( mainClosure->mainEntry(progEntry) ) {
			// modern app with LC_MAIN
			// set startGlue to "start" function in libdyld.dylib
			// set entry to "main" function in program
			*startGlue = (uintptr_t)(libDyldEntry->startFunc);
			*entry     = loader.resolveTarget(progEntry);
#if __has_feature(ptrauth_calls)
			// start() calls the result pointer as a function pointer so we need to sign it.
			*entry = (uintptr_t)__builtin_ptrauth_sign_unauthenticated((void*)*entry, 0, 0);
#endif
		}
		else if ( mainClosure->startEntry(progEntry) ) {
			// old style app linked with crt1.o
			// entry is "start" function in program
			*startGlue = 0;
			*entry     = loader.resolveTarget(progEntry);
		}
		else {
			assert(0);
		}
	}
	CRSetCrashLogMessage("dyld3 mode");
	return true;
}


static const dyld3::closure::LaunchClosure* mapClosureFile(const char* closurePath)
{
	struct stat statbuf;
	if ( dyld3::stat(closurePath, &statbuf) == -1 )
		return nullptr;

	// check for tombstone file
	if ( statbuf.st_size == 0 )
		return nullptr;

	int fd = dyld3::open(closurePath, O_RDONLY, 0);
	if ( fd < 0 )
		return nullptr;

	const dyld3::closure::LaunchClosure* closure = (dyld3::closure::LaunchClosure*)::mmap(NULL, (size_t)statbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	::close(fd);

	if ( closure == MAP_FAILED )
		return nullptr;

	return closure;
}

static bool needsDyld2ErrorMessage(const char* msg)
{
	if ( strcmp(msg, "lazy bind opcodes missing binds") == 0 )
		return true;
	return false;
}

// Note: buildLaunchClosure calls halt() if there is an error building the closure
static const dyld3::closure::LaunchClosure* buildLaunchClosure(const uint8_t* mainExecutableCDHash,
															   const dyld3::closure::LoadedFileInfo& mainFileInfo,
															   const char* envp[],
															   const dyld3::Array<uint8_t>& bootToken)
{
	const dyld3::MachOLoaded* mainExecutableMH = (const dyld3::MachOLoaded*)mainFileInfo.fileContent;
	dyld3::closure::PathOverrides pathOverrides;
	pathOverrides.setFallbackPathHandling(gLinkContext.allowClassicFallbackPaths ? dyld3::closure::PathOverrides::FallbackPathMode::classic : dyld3::closure::PathOverrides::FallbackPathMode::restricted);
	pathOverrides.setEnvVars(envp, mainExecutableMH, mainFileInfo.path);
	STACK_ALLOC_ARRAY(const dyld3::closure::ImageArray*,  imagesArrays, 3);
	if ( sSharedCacheLoadInfo.loadAddress != nullptr ) {
		imagesArrays.push_back(sSharedCacheLoadInfo.loadAddress->cachedDylibsImageArray());
		if ( auto others = sSharedCacheLoadInfo.loadAddress->otherOSImageArray() )
			imagesArrays.push_back(others);
	}

	char closurePath[PATH_MAX];
	bool canSaveClosureToDisk = !bootToken.empty() && dyld3::closure::LaunchClosure::buildClosureCachePath(mainFileInfo.path, envp, true, closurePath);
	dyld3::LaunchErrorInfo* errorInfo = (dyld3::LaunchErrorInfo*)&gProcessInfo->errorKind;
	const dyld3::GradedArchs& archs = dyld3::GradedArchs::forCurrentOS(sKeysDisabled, sOnlyPlatformArm64e);
	dyld3::closure::FileSystemPhysical fileSystem;
	dyld3::closure::ClosureBuilder::AtPath atPathHanding = (gLinkContext.allowAtPaths ? dyld3::closure::ClosureBuilder::AtPath::all : dyld3::closure::ClosureBuilder::AtPath::none);
	dyld3::closure::ClosureBuilder builder(dyld3::closure::kFirstLaunchClosureImageNum, fileSystem, sRootsChecker, sSharedCacheLoadInfo.loadAddress, true,
										   archs, pathOverrides, atPathHanding, gLinkContext.allowEnvVarsPath, errorInfo, (dyld3::Platform)gProcessInfo->platform);
	if (sForceInvalidSharedCacheClosureFormat)
		builder.setDyldCacheInvalidFormatVersion();
	if (sClosureKind == ClosureKind::minimal)
		builder.makeMinimalClosures();
	else if ( canSaveClosureToDisk )
		builder.setCanSkipEncodingRebases(); // <rdar://problem/56172089> large iOS apps with massive number of rebases can overflow 16MB closure file limit
	if ( !gLinkContext.allowInterposing )
		builder.disableInterposing();

	const dyld3::closure::LaunchClosure* result = builder.makeLaunchClosure(mainFileInfo, gLinkContext.allowInsertFailures);
	if ( builder.diagnostics().hasError() ) {
		const char* errMsg = builder.diagnostics().errorMessage();
		// let apps with this error fallback to dyld2 mode
		if ( needsDyld2ErrorMessage(errMsg) ) {
			if ( canSaveClosureToDisk ) {
				// create empty file as a tombstone to not keep trying
				int fd = dyld3::open(closurePath, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
				if ( fd != -1 ) {
					::fchmod(fd, S_IRUSR);
					::close(fd);
					if ( gLinkContext.verboseWarnings )
						dyld::log("dyld: just built tombstone closure for %s\n", sExecPath);
					// We only care about closure failures that do not also cause dyld2 to fail, so defer logging
					// until after dyld2 has tried to launch the binary
					sLogClosureFailure = true;
				}
			}
			return nullptr;
		}
		// terminate process
		halt(errMsg);
	}

	if ( result == nullptr )
		return nullptr;

	if ( !closureValid(result, mainFileInfo, mainExecutableCDHash, false, envp) ) {
		// some how the freshly generated closure is invalid...
		result->deallocate();
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: somehow just built closure is invalid\n");
		return nullptr;
	}

	// write closure file but only if we have boot-token
	if ( canSaveClosureToDisk ) {
		if ( const dyld3::closure::LaunchClosure* existingClosure = mapClosureFile(closurePath) ) {
			if ( (existingClosure->size() == result->size()) && (memcmp(existingClosure, result, result->size()) == 0) ) {
				// closure file already exists and has same content, so re-use file by altering boot-token
				::chmod(closurePath, S_IRUSR|S_IWUSR); // file has to be writable to alter attributes
				// handle both attribute size change and missing attribute
				if ( ::setxattr(closurePath, DYLD_CLOSURE_XATTR_NAME, bootToken.begin(), bootToken.count(), 0, XATTR_REPLACE) != 0 )
					::setxattr(closurePath, DYLD_CLOSURE_XATTR_NAME, bootToken.begin(), bootToken.count(), 0, 0);
				::chmod(closurePath, S_IRUSR);
				result->deallocate();
				if ( gLinkContext.verboseWarnings )
					dyld::log("dyld: reusing previous boot %s closure %p (size=%lu) for %s\n", existingClosure->topImage()->variantString(), existingClosure, existingClosure->size(), sExecPath);
				return existingClosure;
			}
		}
		// make new file
		char closurePathTemp[PATH_MAX];
		strlcpy(closurePathTemp, closurePath, PATH_MAX);
		int mypid = getpid();
		char pidBuf[16];
		char* s = pidBuf;
		*s++ = '.';
		putHexByte(mypid >> 24, s);
		putHexByte(mypid >> 16, s);
		putHexByte(mypid >> 8, s);
		putHexByte(mypid, s);
		*s = '\0';
		strlcat(closurePathTemp, pidBuf, PATH_MAX);
#if TARGET_OS_OSX
		int fd = dyld3::open(closurePathTemp, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR);
#else
		int fd = ::open_dprotected_np(closurePathTemp, O_WRONLY|O_CREAT, PROTECTION_CLASS_D, 0, S_IRUSR|S_IWUSR);
#endif
		if ( fd != -1 ) {
			::ftruncate(fd, result->size());
			::write(fd, result, result->size());
			::fsetxattr(fd, DYLD_CLOSURE_XATTR_NAME, bootToken.begin(), bootToken.count(), 0, 0);
			::fchmod(fd, S_IRUSR);
			::close(fd);
			::rename(closurePathTemp, closurePath);
			// free built closure and mmap file() to reduce dirty memory
			result->deallocate();
			result = mapClosureFile(closurePath);
			sLaunchModeUsed |= DYLD_LAUNCH_MODE_CLOSURE_SAVED_TO_FILE;
		}
		else if ( gLinkContext.verboseWarnings ) {
			dyld::log("could not save closure (errno=%d) to: %s\n", errno, closurePathTemp);
		}
	}

	if ( gLinkContext.verboseWarnings )
		dyld::log("dyld: just built %s closure %p (size=%lu) for %s\n", result->topImage()->variantString(), result, result->size(), sExecPath);

	return result;
}

static const dyld3::closure::LaunchClosure* findCachedLaunchClosure(const uint8_t* mainExecutableCDHash,
																    const dyld3::closure::LoadedFileInfo& mainFileInfo,
																	const char* envp[],
																	const dyld3::Array<uint8_t>& bootToken)
{
	// get path to where closure file will be store for this program
	char closurePath[PATH_MAX];
	if ( !dyld3::closure::LaunchClosure::buildClosureCachePath(mainFileInfo.path, envp, false, closurePath) ) {
		// if cannot construct path to use/store closure file, then use minimal closures
		if ( sClosureKind == ClosureKind::unset )
			sClosureKind = ClosureKind::minimal;
		return nullptr;
	}

	// if file exists, but extended attribute is wrong, ignore file (might be re-used later)
	if ( bootToken.empty() )
		return nullptr;
	uint8_t filesBootToken[bootToken.count()];
	ssize_t attrSize = ::getxattr(closurePath, DYLD_CLOSURE_XATTR_NAME, filesBootToken, bootToken.count(), 0, 0);
	if ( attrSize != bootToken.count() )
		return nullptr;
	if ( memcmp(bootToken.begin(), filesBootToken, bootToken.count()) != 0 )
		return nullptr;

	const dyld3::closure::LaunchClosure* closure = mapClosureFile(closurePath);
	if ( closure == nullptr )
		return nullptr;

	if ( !closureValid(closure, mainFileInfo, mainExecutableCDHash, false, envp) ) {
		::munmap((void*)closure, closure->size());
		return nullptr;
	}

	if ( gLinkContext.verboseWarnings )
		dyld::log("dyld: used cached %s closure %p (size=%lu) for %s\n", closure->topImage()->variantString(), closure, closure->size(), sExecPath);

	return closure;
}

#endif // !TARGET_OS_SIMULATOR

	
static ClosureMode getPlatformDefaultClosureMode() {
#if TARGET_OS_OSX
#if __i386__
	// rdar://problem/32701418: Don't use dyld3 for i386 for now.
	return ClosureMode::Off;
#else
	// x86_64 defaults to using the shared cache closures
	return ClosureMode::PreBuiltOnly;
#endif // __i386__
	
#else
	// <rdar://problem/33171968> enable dyld3 mode for all OS programs when using customer dyld cache (no roots)
	if ( (sSharedCacheLoadInfo.loadAddress != nullptr) && (sSharedCacheLoadInfo.loadAddress->header.cacheType == kDyldSharedCacheTypeProduction) )
		return ClosureMode::On;
	else
		return ClosureMode::Off;
#endif // TARGET_OS_OSX
}

//
// Entry point for dyld.  The kernel loads dyld and jumps to __dyld_start which
// sets up some registers and call this function.
//
// Returns address of main() in target program which __dyld_start jumps to
//
uintptr_t
_main(const macho_header* mainExecutableMH, uintptr_t mainExecutableSlide, 
		int argc, const char* argv[], const char* envp[], const char* apple[], 
		uintptr_t* startGlue)
{
	if (dyld3::kdebug_trace_dyld_enabled(DBG_DYLD_TIMING_LAUNCH_EXECUTABLE)) {
		launchTraceID = dyld3::kdebug_trace_dyld_duration_start(DBG_DYLD_TIMING_LAUNCH_EXECUTABLE, (uint64_t)mainExecutableMH, 0, 0);
	}

	//Check and see if there are any kernel flags
	dyld3::BootArgs::setFlags(hexToUInt64(_simple_getenv(apple, "dyld_flags"), nullptr));

#if __has_feature(ptrauth_calls)
	// Check and see if kernel disabled JOP pointer signing (which lets us load plain arm64 binaries)
	if ( const char* disableStr = _simple_getenv(apple, "ptrauth_disabled") ) {
		if ( strcmp(disableStr, "1") == 0 )
			sKeysDisabled = true;
	}
	else {
		// needed until kernel passes ptrauth_disabled for arm64 main executables
		if ( (mainExecutableMH->cpusubtype == CPU_SUBTYPE_ARM64_V8) || (mainExecutableMH->cpusubtype == CPU_SUBTYPE_ARM64_ALL) )
			sKeysDisabled = true;
	}
#endif

    // Grab the cdHash of the main executable from the environment
	uint8_t mainExecutableCDHashBuffer[20];
	const uint8_t* mainExecutableCDHash = nullptr;
	if ( const char* mainExeCdHashStr = _simple_getenv(apple, "executable_cdhash") ) {
		unsigned bufferLenUsed;
		if ( hexStringToBytes(mainExeCdHashStr, mainExecutableCDHashBuffer, sizeof(mainExecutableCDHashBuffer), bufferLenUsed) )
			mainExecutableCDHash = mainExecutableCDHashBuffer;
	}

	getHostInfo(mainExecutableMH, mainExecutableSlide);

#if !TARGET_OS_SIMULATOR
	// Trace dyld's load
	notifyKernelAboutImage((macho_header*)&__dso_handle, _simple_getenv(apple, "dyld_file"));
	// Trace the main executable's load
	notifyKernelAboutImage(mainExecutableMH, _simple_getenv(apple, "executable_file"));
#endif

	uintptr_t result = 0;
	sMainExecutableMachHeader = mainExecutableMH;
	sMainExecutableSlide = mainExecutableSlide;


	// Set the platform ID in the all image infos so debuggers can tell the process type
	// FIXME: This can all be removed once we make the kernel handle it in rdar://43369446
	// The host may not have the platform field in its struct, but there's space for it in the padding, so always set it
	{
		__block bool platformFound = false;
		((dyld3::MachOFile*)mainExecutableMH)->forEachSupportedPlatform(^(dyld3::Platform platform, uint32_t minOS, uint32_t sdk) {
			if (platformFound) {
				halt("MH_EXECUTE binaries may only specify one platform");
			}
			gProcessInfo->platform = (uint32_t)platform;
			platformFound = true;
		});
		if (gProcessInfo->platform == (uint32_t)dyld3::Platform::unknown) {
			// There were no platforms found in the binary. This may occur on macOS for alternate toolchains and old binaries.
			// It should never occur on any of our embedded platforms.
#if TARGET_OS_OSX
			gProcessInfo->platform = (uint32_t)dyld3::Platform::macOS;
#else
			halt("MH_EXECUTE binaries must specify a minimum supported OS version");
#endif
		}
	}

#if TARGET_OS_OSX
	// Check to see if we need to override the platform
	const char* forcedPlatform = _simple_getenv(envp, "DYLD_FORCE_PLATFORM");
	if (forcedPlatform) {
		dyld_platform_t forcedPlatformType = 0;
		if (strncmp(forcedPlatform, "6", 1) == 0) {
			forcedPlatformType = PLATFORM_MACCATALYST;
		} else if (strncmp(forcedPlatform, "2", 1) == 0) {
			forcedPlatformType = PLATFORM_IOS;
		} else  {
			halt("DYLD_FORCE_PLATFORM is only supported for platform 2 or 6.");
		}
		const dyld3::MachOFile* mf = (dyld3::MachOFile*)sMainExecutableMachHeader;
		if (mf->allowsAlternatePlatform()) {
			gProcessInfo->platform = forcedPlatformType;
		}
	}

	// if this is host dyld, check to see if iOS simulator is being run
	const char* rootPath = _simple_getenv(envp, "DYLD_ROOT_PATH");
	if ( (rootPath != NULL) ) {
		// look to see if simulator has its own dyld
		char simDyldPath[PATH_MAX]; 
		strlcpy(simDyldPath, rootPath, PATH_MAX);
		strlcat(simDyldPath, "/usr/lib/dyld_sim", PATH_MAX);
		int fd = dyld3::open(simDyldPath, O_RDONLY, 0);
		if ( fd != -1 ) {
			const char* errMessage = useSimulatorDyld(fd, mainExecutableMH, simDyldPath, argc, argv, envp, apple, startGlue, &result);
			if ( errMessage != NULL )
				halt(errMessage);
			return result;
		}
	}
	else {
		((dyld3::MachOFile*)mainExecutableMH)->forEachSupportedPlatform(^(dyld3::Platform platform, uint32_t minOS, uint32_t sdk) {
			if ( dyld3::MachOFile::isSimulatorPlatform(platform) )
				halt("attempt to run simulator program outside simulator (DYLD_ROOT_PATH not set)");
		});
	}
#endif

	CRSetCrashLogMessage("dyld: launch started");

	setContext(mainExecutableMH, argc, argv, envp, apple);

	// Pickup the pointer to the exec path.
	sExecPath = _simple_getenv(apple, "executable_path");

	// <rdar://problem/13868260> Remove interim apple[0] transition code from dyld
	if (!sExecPath) sExecPath = apple[0];

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
	// <rdar://54095622> kernel is not passing a real path for main executable
	if ( strncmp(sExecPath, "/var/containers/Bundle/Application/", 35) == 0 ) {
		if ( char* newPath = (char*)malloc(strlen(sExecPath)+10) ) {
			strcpy(newPath, "/private");
			strcat(newPath, sExecPath);
			sExecPath = newPath;
		}
	}
#endif

	if ( sExecPath[0] != '/' ) {
		// have relative path, use cwd to make absolute
		char cwdbuff[MAXPATHLEN];
	    if ( getcwd(cwdbuff, MAXPATHLEN) != NULL ) {
			// maybe use static buffer to avoid calling malloc so early...
			char* s = new char[strlen(cwdbuff) + strlen(sExecPath) + 2];
			strcpy(s, cwdbuff);
			strcat(s, "/");
			strcat(s, sExecPath);
			sExecPath = s;
		}
	}

	// Remember short name of process for later logging
	sExecShortName = ::strrchr(sExecPath, '/');
	if ( sExecShortName != NULL )
		++sExecShortName;
	else
		sExecShortName = sExecPath;

#if TARGET_OS_OSX && __has_feature(ptrauth_calls)
	// on Apple Silicon macOS, only Apple signed ("platform binary") arm64e can be loaded
	sOnlyPlatformArm64e = true;

	// internal builds, or if boot-arg is set, then non-platform-binary arm64e slices can be run
	if ( const char* abiMode = _simple_getenv(apple, "arm64e_abi") ) {
		if ( strcmp(abiMode, "all") == 0 )
			sOnlyPlatformArm64e = false;
	}
#endif

    configureProcessRestrictions(mainExecutableMH, envp);

	// Check if we should force dyld3.  Note we have to do this outside of the regular env parsing due to AMFI
	if ( dyld3::internalInstall() ) {
		if (const char* useClosures = _simple_getenv(envp, "DYLD_USE_CLOSURES")) {
			if ( strcmp(useClosures, "0") == 0 ) {
				sClosureMode = ClosureMode::Off;
			} else if ( strcmp(useClosures, "1") == 0 ) {
	#if !__i386__ // don't support dyld3 for 32-bit macOS
				sClosureMode = ClosureMode::On;
				sClosureKind = ClosureKind::full;
	#endif
			} else if ( strcmp(useClosures, "2") == 0 ) {
				sClosureMode = ClosureMode::On;
				sClosureKind = ClosureKind::minimal;
			} else {
				dyld::warn("unknown option to DYLD_USE_CLOSURES.  Valid options are: 0 and 1\n");
			}

		}
	}

	// Check if we should force the shared cache __DATA_CONST to read-only or read-write
	if ( dyld3::BootArgs::forceReadWriteDataConst() ) {
		gEnableSharedCacheDataConst = false;
	} else if ( dyld3::BootArgs::forceReadOnlyDataConst() ) {
		gEnableSharedCacheDataConst = true;
	} else {
		// __DATA_CONST is enabled by default for arm64(e) for now
#if __arm64__ && __LP64__
		gEnableSharedCacheDataConst = true;
#else
		gEnableSharedCacheDataConst = false;
#endif
	}
	bool sharedCacheDataConstIsEnabled = gEnableSharedCacheDataConst;

	if ( dyld3::internalInstall() ) {
		if (const char* dataConst = _simple_getenv(envp, "DYLD_SHARED_REGION_DATA_CONST")) {
			if ( strcmp(dataConst, "RW") == 0 ) {
				gEnableSharedCacheDataConst = false;
			} else if ( strcmp(dataConst, "RO") == 0 ) {
				gEnableSharedCacheDataConst = true;
			} else {
				dyld::warn("unknown option to DYLD_SHARED_REGION_DATA_CONST.  Valid options are: RW and RO\n");
			}

		}
	}


#if TARGET_OS_OSX
    if ( !gLinkContext.allowEnvVarsPrint && !gLinkContext.allowEnvVarsPath && !gLinkContext.allowEnvVarsSharedCache ) {
		pruneEnvironmentVariables(envp, &apple);
		// set again because envp and apple may have changed or moved
		setContext(mainExecutableMH, argc, argv, envp, apple);
	}
	else
#endif
	{
		checkEnvironmentVariables(envp);
		defaultUninitializedFallbackPaths(envp);
	}
#if TARGET_OS_OSX
	switch (gProcessInfo->platform) {
#if (TARGET_OS_OSX && TARGET_CPU_ARM64)
		case PLATFORM_IOS:
			sClosureMode = ClosureMode::On; // <rdar://problem/56792308> Run iOS apps on macOS in dyld3 mode
			[[clang::fallthrough]];
#endif
		case PLATFORM_MACCATALYST:
			gLinkContext.rootPaths = parseColonList("/System/iOSSupport", NULL);
			gLinkContext.iOSonMac = true;
			if ( sEnv.DYLD_FALLBACK_LIBRARY_PATH == sLibraryFallbackPaths )
				sEnv.DYLD_FALLBACK_LIBRARY_PATH = sRestrictedLibraryFallbackPaths;
			if ( sEnv.DYLD_FALLBACK_FRAMEWORK_PATH == sFrameworkFallbackPaths )
				sEnv.DYLD_FALLBACK_FRAMEWORK_PATH = sRestrictedFrameworkFallbackPaths;
			break;
		case PLATFORM_DRIVERKIT:
			gLinkContext.driverKit = true;
			gLinkContext.sharedRegionMode = ImageLoader::kDontUseSharedRegion;
			break;
	}
#endif
	if ( sEnv.DYLD_PRINT_OPTS )
		printOptions(argv);
	if ( sEnv.DYLD_PRINT_ENV ) 
		printEnvironmentVariables(envp);

	// Parse this envirionment variable outside of the regular logic as we want to accept
	// this on binaries without an entitelment
#if !TARGET_OS_SIMULATOR
	if ( _simple_getenv(envp, "DYLD_JUST_BUILD_CLOSURE") != nullptr ) {
#if TARGET_OS_IPHONE
		char tempClosurePath[PATH_MAX];
		if ( dyld3::closure::LaunchClosure::buildClosureCachePath(sExecPath, envp, false, tempClosurePath) )
			sJustBuildClosure = true;
#endif
		// If the env vars for the data contain look wrong, don't want to launch the app as that would bring up the UI
		if (!sJustBuildClosure) {
			_exit(EXIT_SUCCESS);
		}
	}
#endif

	if ( sJustBuildClosure )
		sClosureMode = ClosureMode::On;

	// load shared cache
	checkSharedRegionDisable((dyld3::MachOLoaded*)mainExecutableMH, mainExecutableSlide);
	if ( gLinkContext.sharedRegionMode != ImageLoader::kDontUseSharedRegion ) {
#if TARGET_OS_SIMULATOR
		if ( sSharedCacheOverrideDir)
			mapSharedCache(mainExecutableSlide);
#else
		mapSharedCache(mainExecutableSlide);
#endif

		// If this process wants a different __DATA_CONST state from the shared region, then override that now
		if ( (sSharedCacheLoadInfo.loadAddress != nullptr) && (gEnableSharedCacheDataConst != sharedCacheDataConstIsEnabled) ) {
			uint32_t permissions = gEnableSharedCacheDataConst ? VM_PROT_READ : (VM_PROT_READ | VM_PROT_WRITE);
			sSharedCacheLoadInfo.loadAddress->changeDataConstPermissions(mach_task_self(), permissions,
																		 (gLinkContext.verboseMapping ? &dyld::log : nullptr));
		}
	}

#if !TARGET_OS_SIMULATOR
	if ( getpid() == 1 ) {
		// Get the value as set by the boot-args
		uint64_t commPageValue = 0;
		size_t commPageValueSize = sizeof(commPageValue);
		if ( sysctlbyname("kern.dyld_flags", &commPageValue, &commPageValueSize, nullptr, 0) != 0 ) {
			// Try again with the old name
			// TODO: Remove this when we are always on new enough kernels
			sysctlbyname("kern.dyld_system_flags", &commPageValue, &commPageValueSize, nullptr, 0);
		}

		commPageValue &= CommPageBootArgMask;
		// logToConsole("dyld: got comm page flags 0x%llx\n", commPageValue);

		// If we are PID 1 (launchd) and on macOS, then we should check if the simulator support dylibs
		// are roots or not.
		// If they are not roots at launchd time, and the file system is read-only, then we know for sure
		// they will not be roots later
#if DYLD_SIMULATOR_ROOTS_SUPPORT
		bool fileSystemIsWritable = true;

		// logToConsole("dyld: in launchd\n");
		struct statfs statBuffer;
		int statResult = statfs("/", &statBuffer);
		if ( statResult == 0 ) {
			if ( !strcmp(statBuffer.f_fstypename, "apfs") ) {
				if ( (statBuffer.f_flags & (MNT_RDONLY | MNT_SNAPSHOT)) == (MNT_RDONLY | MNT_SNAPSHOT) ) {
					// logToConsole("dyld: got statfs flags 0x%llx\n", statBuffer.f_flags);
					fileSystemIsWritable = false;
				}
			}
		} else {
			int error = errno;
			logToConsole("dyld: could not stat '/', errno = %d\n", error);
		}

		// If the file system is read-only, then we can check now whether any of the simulator support
		// dylibs are roots
		bool libsystemKernelIsRoot 		= false;
		bool libsystemPlatformIsRoot 	= false;
		bool libsystemPThreadIsRoot 	= false;
		if ( !fileSystemIsWritable && (sSharedCacheLoadInfo.loadAddress != nullptr)) {
			dyld3::closure::FileSystemPhysical fileSystem;
			libsystemKernelIsRoot 	= !dyld3::RootsChecker::uuidMatchesSharedCache("/usr/lib/system/libsystem_kernel.dylib",
																				   &fileSystem, sSharedCacheLoadInfo.loadAddress);
			libsystemPlatformIsRoot = !dyld3::RootsChecker::uuidMatchesSharedCache("/usr/lib/system/libsystem_platform.dylib",
																				   &fileSystem, sSharedCacheLoadInfo.loadAddress);
			libsystemPThreadIsRoot 	= !dyld3::RootsChecker::uuidMatchesSharedCache("/usr/lib/system/libsystem_pthread.dylib",
																				   &fileSystem, sSharedCacheLoadInfo.loadAddress);
		}
		commPageValue |= (fileSystemIsWritable ? CommPageFlags::fileSystemCanBeModified : CommPageFlags::None);
		commPageValue |= (libsystemKernelIsRoot ? CommPageFlags::libsystemKernelIsRoot : CommPageFlags::None);
		commPageValue |= (libsystemPlatformIsRoot ? CommPageFlags::libsystemPlatformIsRoot : CommPageFlags::None);
		commPageValue |= (libsystemPThreadIsRoot ? CommPageFlags::libsystemPThreadIsRoot : CommPageFlags::None);
#endif // DYLD_SIMULATOR_ROOTS_SUPPORT

		logToConsole("dyld: setting comm page to 0x%llx\n", commPageValue);
		if ( sysctlbyname("kern.dyld_flags", nullptr, 0, &commPageValue, sizeof(commPageValue)) != 0 ) {
			// Try again with the old name
			// TODO: Remove this when we are always on new enough kernels
			sysctlbyname("kern.dyld_system_flags", nullptr, 0, &commPageValue, sizeof(commPageValue));
		}
	}

#if DYLD_SIMULATOR_ROOTS_SUPPORT
	// Set the roots checker to the state from the comm page
	{
		uint64_t dyldFlags = *((uint64_t*)_COMM_PAGE_DYLD_SYSTEM_FLAGS);
		bool fileSystemCanBeModified = dyldFlags & CommPageFlags::fileSystemCanBeModified;
		bool libsystemKernelIsRoot = dyldFlags & CommPageFlags::libsystemKernelIsRoot;
		bool libsystemPlatformIsRoot = dyldFlags & CommPageFlags::libsystemPlatformIsRoot;
		bool libsystemPThreadIsRoot = dyldFlags & CommPageFlags::libsystemPThreadIsRoot;
		sRootsChecker.setFileSystemCanBeModified(fileSystemCanBeModified);
		sRootsChecker.setLibsystemKernelIsRoot(libsystemKernelIsRoot);
		sRootsChecker.setLibsystemPlatformIsRoot(libsystemPlatformIsRoot);
		sRootsChecker.setLibsystemPThreadIsRoot(libsystemPThreadIsRoot);
	}
#endif // DYLD_SIMULATOR_ROOTS_SUPPORT

#endif // !TARGET_OS_SIMULATOR

	// If we haven't got a closure mode yet, then check the environment and cache type
	if ( sClosureMode == ClosureMode::Unset ) {
		// First test to see if we forced in dyld2 via a kernel boot-arg
		if ( dyld3::BootArgs::forceDyld2() ) {
			sClosureMode = ClosureMode::Off;
		} else if ( inDenyList(sExecPath) ) {
			sClosureMode = ClosureMode::Off;
		} else if ( sEnv.hasOverride ) {
			sClosureMode = ClosureMode::Off;
		} else if ( dyld3::BootArgs::forceDyld3() ) {
			sClosureMode = ClosureMode::On;
		} else {
			sClosureMode = getPlatformDefaultClosureMode();
		}
	}

#if !TARGET_OS_SIMULATOR
	if ( sClosureMode == ClosureMode::Off ) {
		if ( gLinkContext.verboseWarnings )
			dyld::log("dyld: not using closures\n");
	} else {
		sLaunchModeUsed = DYLD_LAUNCH_MODE_USING_CLOSURE;
		const dyld3::closure::LaunchClosure* mainClosure = nullptr;
		dyld3::closure::LoadedFileInfo mainFileInfo;
		mainFileInfo.fileContent = mainExecutableMH;
		mainFileInfo.path = sExecPath;
		// FIXME: If we are saving this closure, this slice offset/length is probably wrong in the case of FAT files.
		mainFileInfo.sliceOffset = 0;
		mainFileInfo.sliceLen = -1;
		struct stat mainExeStatBuf;
		if ( dyld3::stat(sExecPath, &mainExeStatBuf) == 0 ) {
			mainFileInfo.inode = mainExeStatBuf.st_ino;
			mainFileInfo.mtime = mainExeStatBuf.st_mtime;
		}
		// check for closure in cache first
		if ( sSharedCacheLoadInfo.loadAddress != nullptr ) {
			mainClosure = sSharedCacheLoadInfo.loadAddress->findClosure(sExecPath);
			if ( gLinkContext.verboseWarnings && (mainClosure != nullptr) )
				dyld::log("dyld: found closure %p (size=%lu) in dyld shared cache\n", mainClosure, mainClosure->size());
			if ( mainClosure != nullptr )
				sLaunchModeUsed |= DYLD_LAUNCH_MODE_CLOSURE_FROM_OS;
		}

		// We only want to try build a closure at runtime if its an iOS third party binary, or a macOS binary from the shared cache
		bool allowClosureRebuilds = false;
		if ( sClosureMode == ClosureMode::On ) {
			allowClosureRebuilds = true;
		} else if ( (sClosureMode == ClosureMode::PreBuiltOnly) && (mainClosure != nullptr) ) {
			allowClosureRebuilds = true;
		}

		if ( (mainClosure != nullptr) && !closureValid(mainClosure, mainFileInfo, mainExecutableCDHash, true, envp) ) {
			mainClosure = nullptr;
			sLaunchModeUsed &= ~DYLD_LAUNCH_MODE_CLOSURE_FROM_OS;
		}

		// <rdar://60333505> bootToken is a concat of boot-hash kernel passes down for app and dyld's uuid
		uint8_t bootTokenBufer[128];
		unsigned bootTokenBufferLen = 0;
		if ( const char* bootHashStr = _simple_getenv(apple, "executable_boothash") ) {
			if ( hexStringToBytes(bootHashStr, bootTokenBufer, sizeof(bootTokenBufer), bootTokenBufferLen) ) {
				if ( ((dyld3::MachOFile*)&__dso_handle)->getUuid(&bootTokenBufer[bootTokenBufferLen]) )
					bootTokenBufferLen += sizeof(uuid_t);
			}
		}
		dyld3::Array<uint8_t> bootToken(bootTokenBufer, bootTokenBufferLen, bootTokenBufferLen);

		// If we didn't find a valid cache closure then try build a new one
		if ( (mainClosure == nullptr) && allowClosureRebuilds ) {
			// if forcing closures, and no closure in cache, or it is invalid, check for cached closure
			if ( !sForceInvalidSharedCacheClosureFormat )
				mainClosure = findCachedLaunchClosure(mainExecutableCDHash, mainFileInfo, envp, bootToken);
			if ( mainClosure == nullptr ) {
				// if  no cached closure found, build new one
				mainClosure = buildLaunchClosure(mainExecutableCDHash, mainFileInfo, envp, bootToken);
				if ( mainClosure != nullptr )
					sLaunchModeUsed |= DYLD_LAUNCH_MODE_BUILT_CLOSURE_AT_LAUNCH;
			}
		}

		// exit dyld after closure is built, without running program
		if ( sJustBuildClosure )
			_exit(EXIT_SUCCESS);

		// try using launch closure
		if ( mainClosure != nullptr ) {
			CRSetCrashLogMessage("dyld3: launch started");
			if ( mainClosure->topImage()->fixupsNotEncoded() )
				sLaunchModeUsed |= DYLD_LAUNCH_MODE_MINIMAL_CLOSURE;
			Diagnostics diag;
			bool closureOutOfDate;
			bool recoverable;
			bool launched = launchWithClosure(mainClosure, sSharedCacheLoadInfo.loadAddress, (dyld3::MachOLoaded*)mainExecutableMH,
											  mainExecutableSlide, argc, argv, envp, apple, diag, &result, startGlue, &closureOutOfDate, &recoverable);
			if ( !launched && closureOutOfDate && allowClosureRebuilds ) {
				// closure is out of date, build new one
				mainClosure = buildLaunchClosure(mainExecutableCDHash, mainFileInfo, envp, bootToken);
				if ( mainClosure != nullptr ) {
					diag.clearError();
					sLaunchModeUsed |= DYLD_LAUNCH_MODE_BUILT_CLOSURE_AT_LAUNCH;
					if ( mainClosure->topImage()->fixupsNotEncoded() )
						sLaunchModeUsed |= DYLD_LAUNCH_MODE_MINIMAL_CLOSURE;
					else
						sLaunchModeUsed &= ~DYLD_LAUNCH_MODE_MINIMAL_CLOSURE;
					launched = launchWithClosure(mainClosure, sSharedCacheLoadInfo.loadAddress, (dyld3::MachOLoaded*)mainExecutableMH,
												 mainExecutableSlide, argc, argv, envp, apple, diag, &result, startGlue, &closureOutOfDate, &recoverable);
				}
			}
			if ( launched ) {
				gLinkContext.startedInitializingMainExecutable = true;
				if (sSkipMain)
					result = (uintptr_t)&fake_main;
				return result;
			}
			else {
				if ( gLinkContext.verboseWarnings ) {
					dyld::log("dyld: unable to use closure %p\n", mainClosure);
				}
				if ( !recoverable )
					halt(diag.errorMessage());
			}
		}
	}
#endif // TARGET_OS_SIMULATOR
	// could not use closure info, launch old way
	sLaunchModeUsed = 0;


	// install gdb notifier
	stateToHandlers(dyld_image_state_dependents_mapped, sBatchHandlers)->push_back(notifyGDB);
	stateToHandlers(dyld_image_state_mapped, sSingleHandlers)->push_back(updateAllImages);
	// make initial allocations large enough that it is unlikely to need to be re-alloced
	sImageRoots.reserve(16);
	sAddImageCallbacks.reserve(4);
	sRemoveImageCallbacks.reserve(4);
	sAddLoadImageCallbacks.reserve(4);
	sImageFilesNeedingTermination.reserve(16);
	sImageFilesNeedingDOFUnregistration.reserve(8);

#if !TARGET_OS_SIMULATOR
#ifdef WAIT_FOR_SYSTEM_ORDER_HANDSHAKE
	// <rdar://problem/6849505> Add gating mechanism to dyld support system order file generation process
	WAIT_FOR_SYSTEM_ORDER_HANDSHAKE(dyld::gProcessInfo->systemOrderFlag);
#endif
#endif


	try {
		// add dyld itself to UUID list
		addDyldImageToUUIDList();

#if SUPPORT_ACCELERATE_TABLES
#if __arm64e__
		// Disable accelerator tables when we have threaded rebase/bind, which is arm64e executables only for now.
		if ((sMainExecutableMachHeader->cpusubtype & ~CPU_SUBTYPE_MASK) == CPU_SUBTYPE_ARM64E)
			sDisableAcceleratorTables = true;
#endif
		bool mainExcutableAlreadyRebased = false;
		if ( (sSharedCacheLoadInfo.loadAddress != nullptr) && !dylibsCanOverrideCache() && !sDisableAcceleratorTables && (sSharedCacheLoadInfo.loadAddress->header.accelerateInfoAddr != 0) ) {
			struct stat statBuf;
			if ( dyld3::stat(IPHONE_DYLD_SHARED_CACHE_DIR "no-dyld2-accelerator-tables", &statBuf) != 0 )
				sAllCacheImagesProxy = ImageLoaderMegaDylib::makeImageLoaderMegaDylib(&sSharedCacheLoadInfo.loadAddress->header, sSharedCacheLoadInfo.slide, mainExecutableMH, gLinkContext);
		}

reloadAllImages:
#endif


#if TARGET_OS_OSX
		gLinkContext.strictMachORequired = false;
        // <rdar://problem/22805519> be less strict about old macOS mach-o binaries
        ((dyld3::MachOFile*)mainExecutableMH)->forEachSupportedPlatform(^(dyld3::Platform platform, uint32_t minOS, uint32_t sdk) {
            if ( (platform == dyld3::Platform::macOS) && (sdk >= DYLD_PACKED_VERSION(10,15,0)) ) {
            	gLinkContext.strictMachORequired = true;
			}
        });
	    if ( gLinkContext.iOSonMac )
		    gLinkContext.strictMachORequired = true;
	#else
		// simulators, iOS, tvOS, watchOS, are always strict
		gLinkContext.strictMachORequired = true;
	#endif


		CRSetCrashLogMessage(sLoadingCrashMessage);
		// instantiate ImageLoader for main executable
		sMainExecutable = instantiateFromLoadedImage(mainExecutableMH, mainExecutableSlide, sExecPath);
		gLinkContext.mainExecutable = sMainExecutable;
		gLinkContext.mainExecutableCodeSigned = hasCodeSignatureLoadCommand(mainExecutableMH);

#if TARGET_OS_SIMULATOR
		// check main executable is not too new for this OS
		{
			if ( ! isSimulatorBinary((uint8_t*)mainExecutableMH, sExecPath) ) {
				throwf("program was built for a platform that is not supported by this runtime");
			}
			uint32_t mainMinOS = sMainExecutable->minOSVersion();

			// dyld is always built for the current OS, so we can get the current OS version
			// from the load command in dyld itself.
			uint32_t dyldMinOS = ImageLoaderMachO::minOSVersion((const mach_header*)&__dso_handle);
			if ( mainMinOS > dyldMinOS ) {
	#if TARGET_OS_WATCH
				throwf("app was built for watchOS %d.%d which is newer than this simulator %d.%d",
						mainMinOS >> 16, ((mainMinOS >> 8) & 0xFF),
						dyldMinOS >> 16, ((dyldMinOS >> 8) & 0xFF));
	#elif TARGET_OS_TV
				throwf("app was built for tvOS %d.%d which is newer than this simulator %d.%d",
						mainMinOS >> 16, ((mainMinOS >> 8) & 0xFF),
						dyldMinOS >> 16, ((dyldMinOS >> 8) & 0xFF));
	#else
				throwf("app was built for iOS %d.%d which is newer than this simulator %d.%d",
						mainMinOS >> 16, ((mainMinOS >> 8) & 0xFF),
						dyldMinOS >> 16, ((dyldMinOS >> 8) & 0xFF));
	#endif
			}
		}
#endif


	#if SUPPORT_ACCELERATE_TABLES
		sAllImages.reserve((sAllCacheImagesProxy != NULL) ? 16 : INITIAL_IMAGE_COUNT);
	#else
		sAllImages.reserve(INITIAL_IMAGE_COUNT);
	#endif

#if defined(__x86_64__) && !TARGET_OS_SIMULATOR
		if (dyld::isTranslated()) {
			struct dyld_all_runtime_info {
				uint32_t image_count;
				dyld_image_info* images;
				uint32_t uuid_count;
				dyld_uuid_info* uuids;
				uint32_t aot_image_count;
				dyld_aot_image_info* aots;
				dyld_aot_shared_cache_info aot_cache_info;
			};

			dyld_all_runtime_info* runtime_info;
			int ret = syscall(0x7000004, &runtime_info);
			if (ret == 0) {
				for (int i = 0; i < runtime_info->uuid_count; i++) {
					dyld_image_info image_info = runtime_info->images[i];
					dyld_uuid_info uuid_info = runtime_info->uuids[i];

					// add the arm64 cambria runtime to uuid info
					addNonSharedCacheImageUUID(uuid_info);

					struct stat sb;
					if (stat(image_info.imageFilePath, &sb) == 0) {
						fsid_t fsid = {{0, 0}};
						fsobj_id_t fsobj = {0};
						ino_t inode = sb.st_ino;
						fsobj.fid_objno = (uint32_t)inode;
						fsobj.fid_generation = (uint32_t)(inode>>32);
						fsid.val[0] = sb.st_dev;

						dyld3::kdebug_trace_dyld_image(DBG_DYLD_UUID_MAP_A, image_info.imageFilePath, &(uuid_info.imageUUID), fsobj, fsid, image_info.imageLoadAddress);
					}
				}

				// add aot images to dyld_all_image_info
				addAotImagesToAllAotImages(runtime_info->aot_image_count, runtime_info->aots);

				// add the arm64 cambria runtime to dyld_all_image_info
				addImagesToAllImages(runtime_info->image_count, runtime_info->images);

				// set the aot shared cache info in dyld_all_image_info
				dyld::gProcessInfo->aotSharedCacheBaseAddress = runtime_info->aot_cache_info.cacheBaseAddress;
				memcpy(dyld::gProcessInfo->aotSharedCacheUUID, runtime_info->aot_cache_info.cacheUUID, sizeof(uuid_t));
			}
		}
#endif

		// Now that shared cache is loaded, setup an versioned dylib overrides
	#if SUPPORT_VERSIONED_PATHS
		checkVersionedPaths();
	#endif


		// dyld_all_image_infos image list does not contain dyld
		// add it as dyldPath field in dyld_all_image_infos
		// for simulator, dyld_sim is in image list, need host dyld added
#if TARGET_OS_SIMULATOR
		// get path of host dyld from table of syscall vectors in host dyld
		void* addressInDyld = gSyscallHelpers;
#else
		// get path of dyld itself
		void*  addressInDyld = (void*)&__dso_handle;
#endif
		char dyldPathBuffer[MAXPATHLEN+1];
		int len = proc_regionfilename(getpid(), (uint64_t)(long)addressInDyld, dyldPathBuffer, MAXPATHLEN);
		if ( len > 0 ) {
			dyldPathBuffer[len] = '\0'; // proc_regionfilename() does not zero terminate returned string
			if ( strcmp(dyldPathBuffer, gProcessInfo->dyldPath) != 0 )
				gProcessInfo->dyldPath = strdup(dyldPathBuffer);
		}

		// load any inserted libraries
		if	( sEnv.DYLD_INSERT_LIBRARIES != NULL ) {
			for (const char* const* lib = sEnv.DYLD_INSERT_LIBRARIES; *lib != NULL; ++lib) 
				loadInsertedDylib(*lib);
		}
		// record count of inserted libraries so that a flat search will look at 
		// inserted libraries, then main, then others.
		sInsertedDylibCount = sAllImages.size()-1;

		// link main executable
		gLinkContext.linkingMainExecutable = true;
#if SUPPORT_ACCELERATE_TABLES
		if ( mainExcutableAlreadyRebased ) {
			// previous link() on main executable has already adjusted its internal pointers for ASLR
			// work around that by rebasing by inverse amount
			sMainExecutable->rebase(gLinkContext, -mainExecutableSlide);
		}
#endif
		link(sMainExecutable, sEnv.DYLD_BIND_AT_LAUNCH, true, ImageLoader::RPathChain(NULL, NULL), -1);
		sMainExecutable->setNeverUnloadRecursive();
		if ( sMainExecutable->forceFlat() ) {
			gLinkContext.bindFlat = true;
			gLinkContext.prebindUsage = ImageLoader::kUseNoPrebinding;
		}

		// link any inserted libraries
		// do this after linking main executable so that any dylibs pulled in by inserted 
		// dylibs (e.g. libSystem) will not be in front of dylibs the program uses
		if ( sInsertedDylibCount > 0 ) {
			for(unsigned int i=0; i < sInsertedDylibCount; ++i) {
				ImageLoader* image = sAllImages[i+1];
				link(image, sEnv.DYLD_BIND_AT_LAUNCH, true, ImageLoader::RPathChain(NULL, NULL), -1);
				image->setNeverUnloadRecursive();
			}
			if ( gLinkContext.allowInterposing ) {
				// only INSERTED libraries can interpose
				// register interposing info after all inserted libraries are bound so chaining works
				for(unsigned int i=0; i < sInsertedDylibCount; ++i) {
					ImageLoader* image = sAllImages[i+1];
					image->registerInterposing(gLinkContext);
				}
			}
		}

		if ( gLinkContext.allowInterposing ) {
			// <rdar://problem/19315404> dyld should support interposition even without DYLD_INSERT_LIBRARIES
			for (long i=sInsertedDylibCount+1; i < sAllImages.size(); ++i) {
				ImageLoader* image = sAllImages[i];
				if ( image->inSharedCache() )
					continue;
				image->registerInterposing(gLinkContext);
			}
		}
	#if SUPPORT_ACCELERATE_TABLES
		if ( (sAllCacheImagesProxy != NULL) && ImageLoader::haveInterposingTuples() ) {
			// Accelerator tables cannot be used with implicit interposing, so relaunch with accelerator tables disabled
			ImageLoader::clearInterposingTuples();
			// unmap all loaded dylibs (but not main executable)
			for (long i=1; i < sAllImages.size(); ++i) {
				ImageLoader* image = sAllImages[i];
				if ( image == sMainExecutable )
					continue;
				if ( image == sAllCacheImagesProxy )
					continue;
				image->setCanUnload();
				ImageLoader::deleteImage(image);
			}
			// note: we don't need to worry about inserted images because if DYLD_INSERT_LIBRARIES was set we would not be using the accelerator table
			sAllImages.clear();
			sImageRoots.clear();
			sImageFilesNeedingTermination.clear();
			sImageFilesNeedingDOFUnregistration.clear();
			sAddImageCallbacks.clear();
			sRemoveImageCallbacks.clear();
			sAddLoadImageCallbacks.clear();
			sAddBulkLoadImageCallbacks.clear();
			sDisableAcceleratorTables = true;
			sAllCacheImagesProxy = NULL;
			sMappedRangesStart = NULL;
			mainExcutableAlreadyRebased = true;
			gLinkContext.linkingMainExecutable = false;
			resetAllImages();
			goto reloadAllImages;
		}
	#endif

		// apply interposing to initial set of images
		for(int i=0; i < sImageRoots.size(); ++i) {
			sImageRoots[i]->applyInterposing(gLinkContext);
		}
		ImageLoader::applyInterposingToDyldCache(gLinkContext);

		// Bind and notify for the main executable now that interposing has been registered
		uint64_t bindMainExecutableStartTime = mach_absolute_time();
		sMainExecutable->recursiveBindWithAccounting(gLinkContext, sEnv.DYLD_BIND_AT_LAUNCH, true);
		uint64_t bindMainExecutableEndTime = mach_absolute_time();
		ImageLoaderMachO::fgTotalBindTime += bindMainExecutableEndTime - bindMainExecutableStartTime;
		gLinkContext.notifyBatch(dyld_image_state_bound, false);

		// Bind and notify for the inserted images now interposing has been registered
		if ( sInsertedDylibCount > 0 ) {
			for(unsigned int i=0; i < sInsertedDylibCount; ++i) {
				ImageLoader* image = sAllImages[i+1];
				image->recursiveBind(gLinkContext, sEnv.DYLD_BIND_AT_LAUNCH, true, nullptr);
			}
		}
		
		// <rdar://problem/12186933> do weak binding only after all inserted images linked
		sMainExecutable->weakBind(gLinkContext);
		gLinkContext.linkingMainExecutable = false;

		sMainExecutable->recursiveMakeDataReadOnly(gLinkContext);

		CRSetCrashLogMessage("dyld: launch, running initializers");
	#if SUPPORT_OLD_CRT_INITIALIZATION
		// Old way is to run initializers via a callback from crt1.o
		if ( ! gRunInitializersOldWay ) 
			initializeMainExecutable(); 
	#else
		// run all initializers
		initializeMainExecutable(); 
	#endif

		// notify any montoring proccesses that this process is about to enter main()
		notifyMonitoringDyldMain();
		if (dyld3::kdebug_trace_dyld_enabled(DBG_DYLD_TIMING_LAUNCH_EXECUTABLE)) {
			dyld3::kdebug_trace_dyld_duration_end(launchTraceID, DBG_DYLD_TIMING_LAUNCH_EXECUTABLE, 0, 0, 2);
		}
		ARIADNEDBG_CODE(220, 1);

#if TARGET_OS_OSX
		if ( gLinkContext.driverKit ) {
			result = (uintptr_t)sEntryOverride;
			if ( result == 0 )
				halt("no entry point registered");
			*startGlue = (uintptr_t)gLibSystemHelpers->startGlueToCallExit;
		}
		else
#endif
		{
			// find entry point for main executable
			result = (uintptr_t)sMainExecutable->getEntryFromLC_MAIN();
			if ( result != 0 ) {
				// main executable uses LC_MAIN, we need to use helper in libdyld to call into main()
				if ( (gLibSystemHelpers != NULL) && (gLibSystemHelpers->version >= 9) )
					*startGlue = (uintptr_t)gLibSystemHelpers->startGlueToCallExit;
				else
					halt("libdyld.dylib support not present for LC_MAIN");
			}
			else {
				// main executable uses LC_UNIXTHREAD, dyld needs to let "start" in program set up for main()
				result = (uintptr_t)sMainExecutable->getEntryFromLC_UNIXTHREAD();
				*startGlue = 0;
			}
		}
	}
	catch(const char* message) {
		syncAllImages();
		halt(message);
	}
	catch(...) {
		dyld::log("dyld: launch failed\n");
	}

	CRSetCrashLogMessage("dyld2 mode");
#if !TARGET_OS_SIMULATOR
	if (sLogClosureFailure) {
		// We failed to launch in dyld3, but dyld2 can handle it. synthesize a crash report for analytics
		dyld3::syntheticBacktrace("Could not generate launchClosure, falling back to dyld2", true);
	}
#endif

	if (sSkipMain) {
		notifyMonitoringDyldMain();
		if (dyld3::kdebug_trace_dyld_enabled(DBG_DYLD_TIMING_LAUNCH_EXECUTABLE)) {
			dyld3::kdebug_trace_dyld_duration_end(launchTraceID, DBG_DYLD_TIMING_LAUNCH_EXECUTABLE, 0, 0, 2);
		}
		ARIADNEDBG_CODE(220, 1);
		result = (uintptr_t)&fake_main;
		*startGlue = (uintptr_t)gLibSystemHelpers->startGlueToCallExit;
	}

	return result;
}


} // namespace



