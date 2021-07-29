/* -*- mode: C++; c-basic-offset: 4; tab-width: 4 -*-
 *
 * Copyright (c) 2004-2012 Apple Inc. All rights reserved.
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

#include <stddef.h>
#include <string.h>
#include <malloc/malloc.h>
#include <sys/mman.h>
#include <execinfo.h>

#include <TargetConditionals.h>
#include <System/sys/csr.h>
#include <crt_externs.h>
#include <Availability.h>
#if !TARGET_OS_DRIVERKIT
#include <vproc_priv.h>
#endif
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <System/sys/codesign.h>
#include <libc_private.h>

#include <mach-o/dyld_images.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_priv.h>

#include <ptrauth.h>

#include "dyld_cache_format.h"
#include "objc-shared-cache.h"

#include "ImageLoader.h"
#include "dyldLock.h"

#include "APIs.h"
#include "AllImages.h"
#include "StartGlue.h"
#include "Tracing.h"


// this was in dyld_priv.h but it is no longer exported
extern "C" {
    const struct dyld_all_image_infos* _dyld_get_all_image_infos() __attribute__((visibility("hidden")));
}


extern "C" int  __cxa_atexit(void (*func)(void *), void *arg, void *dso);
extern "C" void __cxa_finalize(const void *dso);
extern "C" void __cxa_finalize_ranges(const struct __cxa_range_t ranges[], int count);

//
// private interface between libSystem.dylib and dyld
//
extern "C" int _dyld_func_lookup(const char* dyld_func_name, void **address);

template<typename T>
static void dyld_func_lookup_and_resign(const char *dyld_func_name, T *__ptrauth_dyld_function_ptr* address) {
    void *funcAsVoidPtr;
    int res = _dyld_func_lookup(dyld_func_name, &funcAsVoidPtr);
    (void)res;

    // If C function pointer discriminators are type-diverse this cast will be
    // an authenticate and resign operation.
    *address = reinterpret_cast<T *>(funcAsVoidPtr);
}

#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
namespace dyld3 {
    extern int compatFuncLookup(const char* name, void** address) __API_AVAILABLE(ios(13.0));
}
extern "C" void setLookupFunc(void*);
#endif


extern void* __ptrauth_dyld_address_auth gUseDyld3;


// <rdar://problem/61161069> libdyld.dylib should use abort_with_payload() for asserts
VIS_HIDDEN
void abort_report_np(const char* format, ...)
{
    va_list list;
    const char *str;
    _SIMPLE_STRING s = _simple_salloc();
    if ( s != NULL ) {
        va_start(list, format);
        _simple_vsprintf(s, format, list);
        va_end(list);
        str = _simple_string(s);
    }
    else {
        // _simple_salloc failed, but at least format may have useful info by itself
        str = format;
    }
    if ( gUseDyld3 ) {
        dyld3::halt(str);
    }
    else {
        typedef void (*funcType)(const char* msg) __attribute__((__noreturn__));
        static funcType __ptrauth_dyld_function_ptr p = NULL;
        dyld_func_lookup_and_resign("__dyld_halt", &p);
        p(str);
    }
    // halt() doesn't return, so we can't call _simple_sfree
}

// libc uses assert()
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-noreturn"
VIS_HIDDEN
void __assert_rtn(const char* func, const char* file, int line, const char* failedexpr)
{
    if (func == NULL) {
        abort_report_np("Assertion failed: (%s), file %s, line %d.\n", failedexpr, file, line);
    } else {
        abort_report_np("Assertion failed: (%s), function %s, file %s, line %d.\n", failedexpr, func, file, line);
    }
}
#pragma clang diagnostic pop


// deprecated APIs are still availble on Mac OS X, but not on iPhone OS
#if TARGET_OS_OSX
	#define DEPRECATED_APIS_SUPPORTED 1
#else
	#define DEPRECATED_APIS_SUPPORTED 0
#endif

/*
 * names_match() takes an install_name from an LC_LOAD_DYLIB command and a
 * libraryName (which is -lx or -framework Foo argument passed to the static
 * link editor for the same library) and determines if they match.  This depends
 * on conventional use of names including major versioning.
 */
static
bool
names_match(
const char *install_name,
const char* libraryName)
{
    const char *basename;
    unsigned long n;

	/*
	 * Conventional install names have these forms:
	 *	/System/Library/Frameworks/AppKit.framework/Versions/A/Appkit
	 *	/Local/Library/Frameworks/AppKit.framework/Appkit
	 *	/lib/libsys_s.A.dylib
	 *	/usr/lib/libsys_s.dylib
	 */
	basename = strrchr(install_name, '/');
	if(basename == NULL)
	    basename = install_name;
	else
	    basename++;

	/*
	 * By checking the base name matching the library name we take care
	 * of the -framework cases.
	 */
	if(strcmp(basename, libraryName) == 0)
	    return true;

	/*
	 * Now check the base name for "lib" if so proceed to check for the
	 * -lx case dealing with a possible .X.dylib and a .dylib extension.
	 */
	if(strncmp(basename, "lib", 3) ==0){
	    n = strlen(libraryName);
	    if(strncmp(basename+3, libraryName, n) == 0){
		if(strncmp(basename+3+n, ".dylib", 6) == 0)
		    return true;
		if(basename[3+n] == '.' &&
		   basename[3+n+1] != '\0' &&
		   strncmp(basename+3+n+2, ".dylib", 6) == 0)
		    return true;
	    }
	}
	return false;
}

#if DEPRECATED_APIS_SUPPORTED

void NSInstallLinkEditErrorHandlers(
const NSLinkEditErrorHandlers* handlers)
{
	if ( gUseDyld3 )
		return dyld3::NSInstallLinkEditErrorHandlers(handlers);

	DYLD_LOCK_THIS_BLOCK;
	typedef void (*ucallback_t)(const char* symbol_name);
 	typedef NSModule (*mcallback_t)(NSSymbol s, NSModule old, NSModule newhandler);
	typedef void (*lcallback_t)(NSLinkEditErrors c, int errorNumber,
								const char* fileName, const char* errorString);
	typedef void (*funcType)(ucallback_t undefined, mcallback_t multiple, lcallback_t linkEdit);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_install_handlers", &p);
	mcallback_t m = handlers->multiple;
	p(handlers->undefined, m, handlers->linkEdit);
}

const char* 
NSNameOfModule(
NSModule module)
{
	if ( gUseDyld3 )
		return dyld3::NSNameOfModule(module);

	DYLD_LOCK_THIS_BLOCK;
    typedef const char* (*funcType)(NSModule module);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSNameOfModule", &p);
	return(p(module));
} 

const char* 
NSLibraryNameForModule(
NSModule module)
{
	if ( gUseDyld3 )
		return dyld3::NSLibraryNameForModule(module);

	DYLD_LOCK_THIS_BLOCK;
    typedef const char*  (*funcType)(NSModule module);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSLibraryNameForModule", &p);
	return(p(module));
}

bool
NSIsSymbolNameDefined(
const char* symbolName)
{
	if ( gUseDyld3 )
		return dyld3::NSIsSymbolNameDefined(symbolName);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const char* symbolName);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSIsSymbolNameDefined", &p);
	return(p(symbolName));
}

bool
NSIsSymbolNameDefinedWithHint(
const char* symbolName,
const char* libraryNameHint)
{
	if ( gUseDyld3 )
		return dyld3::NSIsSymbolNameDefinedWithHint(symbolName, libraryNameHint);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const char* symbolName,
                             const char* libraryNameHint);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSIsSymbolNameDefinedWithHint", &p);
	return(p(symbolName, libraryNameHint));
}

bool
NSIsSymbolNameDefinedInImage(
const struct mach_header *image,
const char* symbolName)
{
	if ( gUseDyld3 )
		return dyld3::NSIsSymbolNameDefinedInImage(image, symbolName);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const struct mach_header *image,
                             const char* symbolName);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSIsSymbolNameDefinedInImage", &p);
	return(p(image, symbolName));
}

NSSymbol
NSLookupAndBindSymbol(
const char* symbolName)
{
	if ( gUseDyld3 )
		return dyld3::NSLookupAndBindSymbol(symbolName);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSSymbol (*funcType)(const char* symbolName);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSLookupAndBindSymbol", &p);
	return(p(symbolName));
}

NSSymbol
NSLookupAndBindSymbolWithHint(
const char* symbolName,
const char* libraryNameHint)
{
	if ( gUseDyld3 )
		return dyld3::NSLookupAndBindSymbolWithHint(symbolName, libraryNameHint);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSSymbol (*funcType)(const char* symbolName,
                                 const char* libraryNameHint);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSLookupAndBindSymbolWithHint", &p);
	return(p(symbolName, libraryNameHint));
}

NSSymbol
NSLookupSymbolInModule(
NSModule module,
const char* symbolName)
{
	if ( gUseDyld3 )
		return dyld3::NSLookupSymbolInModule(module, symbolName);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSSymbol (*funcType)(NSModule module, const char* symbolName);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSLookupSymbolInModule", &p);
	return(p(module, symbolName));
}

NSSymbol
NSLookupSymbolInImage(
const struct mach_header *image,
const char* symbolName,
uint32_t options)
{
	if ( gUseDyld3 )
		return dyld3::NSLookupSymbolInImage(image, symbolName, options);

 	DYLD_LOCK_THIS_BLOCK;
    typedef NSSymbol (*funcType)(const struct mach_header *image,
                                 const char* symbolName,
                                 uint32_t options);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSLookupSymbolInImage", &p);
	return(p(image, symbolName, options));
}

const char* 
NSNameOfSymbol(
NSSymbol symbol)
{
	if ( gUseDyld3 )
		return dyld3::NSNameOfSymbol(symbol);

	DYLD_LOCK_THIS_BLOCK;
    typedef char * (*funcType)(NSSymbol symbol);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSNameOfSymbol",&p);
	return(p(symbol));
}

void *
NSAddressOfSymbol(
NSSymbol symbol)
{
	if ( gUseDyld3 )
		return dyld3::NSAddressOfSymbol(symbol);

	DYLD_LOCK_THIS_BLOCK;
    typedef void * (*funcType)(NSSymbol symbol);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSAddressOfSymbol", &p);
	return(p(symbol));
}

NSModule
NSModuleForSymbol(
NSSymbol symbol)
{
	if ( gUseDyld3 )
		return dyld3::NSModuleForSymbol(symbol);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSModule (*funcType)(NSSymbol symbol);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSModuleForSymbol", &p);
	return(p(symbol));
}

bool
NSAddLibrary(
const char* pathName)
{
	if ( gUseDyld3 )
		return dyld3::NSAddLibrary(pathName);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const char* pathName);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSAddLibrary", &p);
	return(p(pathName));
}

bool
NSAddLibraryWithSearching(
const char* pathName)
{
	if ( gUseDyld3 )
		return dyld3::NSAddLibrary(pathName);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const char* pathName);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSAddLibraryWithSearching", &p);
	return(p(pathName));
}

const struct mach_header *
NSAddImage(
const char* image_name,
uint32_t options)
{
	if ( gUseDyld3 )
		return dyld3::NSAddImage(image_name, options);

	DYLD_LOCK_THIS_BLOCK;
    typedef const struct mach_header * (*funcType)(const char* image_name,
                                                   uint32_t options);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSAddImage", &p);
	return(p(image_name, options));
}
#endif // DEPRECATED_APIS_SUPPORTED

/*
 * This routine returns the current version of the named shared library the
 * executable it was built with.  The libraryName parameter is the same as the
 * -lx or -framework Foo argument passed to the static link editor when building
 * the executable (with -lx it would be "x" and with -framework Foo it would be
 * "Foo").  If this the executable was not built against the specified library
 * it returns -1.  It should be noted that if this only returns the value the
 * current version of the named shared library the executable was built with
 * and not a list of current versions that dependent libraries and bundles the
 * program is using were built with.
 */
int32_t NSVersionOfLinkTimeLibrary(const char* libraryName)
{
	if ( gUseDyld3 )
		return dyld3::NSVersionOfLinkTimeLibrary(libraryName);

	// Lazily call _NSGetMachExecuteHeader() and cache result
#if __LP64__
    static mach_header_64* mh = NULL;
#else
    static mach_header* mh = NULL;
#endif
	if ( mh == NULL )
	    mh = _NSGetMachExecuteHeader();
#if __LP64__
	const load_command* lc = (load_command*)((char*)mh + sizeof(mach_header_64));
#else
	const load_command* lc = (load_command*)((char*)mh + sizeof(mach_header));
#endif
	for(uint32_t i = 0; i < mh->ncmds; i++){
		switch ( lc->cmd ) { 
			case LC_LOAD_DYLIB:
			case LC_LOAD_WEAK_DYLIB:
			case LC_LOAD_UPWARD_DYLIB:
				const dylib_command* dl = (dylib_command *)lc;
				const char* install_name = (char*)dl + dl->dylib.name.offset;
				if ( names_match(install_name, libraryName) )
					return dl->dylib.current_version;
				break;
		}
	    lc = (load_command*)((char*)lc + lc->cmdsize);
	}
	return (-1);
}

/*
 * This routine returns the current version of the named shared library the
 * program it is running against.  The libraryName parameter is the same as
 * would be static link editor using the -lx or -framework Foo flags (with -lx
 * it would be "x" and with -framework Foo it would be "Foo").  If the program
 * is not using the specified library it returns -1.
 */
int32_t NSVersionOfRunTimeLibrary(const char* libraryName)
{
	if ( gUseDyld3 )
		return dyld3::NSVersionOfRunTimeLibrary(libraryName);

	uint32_t n = _dyld_image_count();
	for(uint32_t i = 0; i < n; i++){
	    const mach_header* mh = _dyld_get_image_header(i);
		if ( mh == NULL )
			continue;
	    if ( mh->filetype != MH_DYLIB )
			continue;
#if __LP64__
	    const load_command* lc = (load_command*)((char*)mh + sizeof(mach_header_64));
#else
	    const load_command* lc = (load_command*)((char*)mh + sizeof(mach_header));
#endif
	    for(uint32_t j = 0; j < mh->ncmds; j++){
			if ( lc->cmd == LC_ID_DYLIB ) {
				const dylib_command* dl = (dylib_command*)lc;
				const char* install_name = (char *)dl + dl->dylib.name.offset;
				if ( names_match(install_name, libraryName) )
					return dl->dylib.current_version;
			}
			lc = (load_command*)((char*)lc + lc->cmdsize);
	    }
	}
	return (-1);
}

#if TARGET_OS_WATCH
uint32_t dyld_get_program_sdk_watch_os_version()
{
    if (gUseDyld3)
        return dyld3::dyld_get_program_sdk_watch_os_version();

    __block uint32_t retval = 0;
    __block bool versionFound = false;
    dyld3::dyld_get_image_versions((mach_header*)_NSGetMachExecuteHeader(), ^(dyld_platform_t platform, uint32_t sdk_version, uint32_t min_version) {
        if (versionFound) return;

        if (dyld_get_base_platform(platform) == PLATFORM_WATCHOS) {
            versionFound = true;
            retval = sdk_version;
        }
    });

    return retval;
}

uint32_t dyld_get_program_min_watch_os_version()
{
    if (gUseDyld3)
        return dyld3::dyld_get_program_min_watch_os_version();

    __block uint32_t retval = 0;
    __block bool versionFound = false;
    dyld3::dyld_get_image_versions((mach_header*)_NSGetMachExecuteHeader(), ^(dyld_platform_t platform, uint32_t sdk_version, uint32_t min_version) {
        if (versionFound) return;

        if (dyld_get_base_platform(platform) == PLATFORM_WATCHOS) {
            versionFound = true;
            retval = min_version;
        }
    });

    return retval;
}
#endif

#if TARGET_OS_BRIDGE
uint32_t dyld_get_program_sdk_bridge_os_version()
{
    if (gUseDyld3)
        return dyld3::dyld_get_program_sdk_bridge_os_version();

    __block uint32_t retval = 0;
    __block bool versionFound = false;
    dyld3::dyld_get_image_versions((mach_header*)_NSGetMachExecuteHeader(), ^(dyld_platform_t platform, uint32_t sdk_version, uint32_t min_version) {
        if (versionFound) return;

        if (dyld_get_base_platform(platform) == PLATFORM_BRIDGEOS) {
            versionFound = true;
            retval = sdk_version;
        }
    });

    return retval;
}

uint32_t dyld_get_program_min_bridge_os_version()
{
    if (gUseDyld3)
        return dyld3::dyld_get_program_min_bridge_os_version();

    __block uint32_t retval = 0;
    __block bool versionFound = false;
    dyld3::dyld_get_image_versions((mach_header*)_NSGetMachExecuteHeader(), ^(dyld_platform_t platform, uint32_t sdk_version, uint32_t min_version) {
        if (versionFound) return;

        if (dyld_get_base_platform(platform) == PLATFORM_BRIDGEOS) {
            versionFound = true;
            retval = min_version;
        }
    });

    return retval;
}
#endif

/*
 * Returns the sdk version (encode as nibble XXXX.YY.ZZ) the
 * specified binary was built against.
 *
 * First looks for LC_VERSION_MIN_* in binary and if sdk field is 
 * not zero, return that value.
 * Otherwise, looks for the libSystem.B.dylib the binary linked
 * against and uses a table to convert that to an sdk version.
 */
uint32_t dyld_get_sdk_version(const mach_header* mh)
{
    return dyld3::dyld_get_sdk_version(mh);
}

uint32_t dyld_get_program_sdk_version()
{
    return dyld3::dyld_get_sdk_version((mach_header*)_NSGetMachExecuteHeader());
}

uint32_t dyld_get_min_os_version(const struct mach_header* mh)
{
    return dyld3::dyld_get_min_os_version(mh);
}


uint32_t dyld_get_program_min_os_version()
{
    return dyld3::dyld_get_min_os_version((mach_header*)_NSGetMachExecuteHeader());
}


bool _dyld_get_image_uuid(const struct mach_header* mh, uuid_t uuid)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_image_uuid(mh, uuid);

	const load_command* startCmds = NULL;
	if ( mh->magic == MH_MAGIC_64 )
		startCmds = (load_command*)((char *)mh + sizeof(mach_header_64));
	else if ( mh->magic == MH_MAGIC )
		startCmds = (load_command*)((char *)mh + sizeof(mach_header));
	else
		return false;  // not a mach-o file, or wrong endianness

	const load_command* const cmdsEnd = (load_command*)((char*)startCmds + mh->sizeofcmds);
	const load_command* cmd = startCmds;
	for(uint32_t i = 0; i < mh->ncmds; ++i) {
	    const load_command* nextCmd = (load_command*)((char *)cmd + cmd->cmdsize);
		if ( (cmd->cmdsize < 8) || (nextCmd > cmdsEnd) || (nextCmd < startCmds)) {
			return false;
		}
		if ( cmd->cmd == LC_UUID ) {
			const uuid_command* uuidCmd = (uuid_command*)cmd;
			memcpy(uuid, uuidCmd->uuid, 16);
			return true;
		}
		cmd = nextCmd;
	}
	bzero(uuid, 16);
	return false;
}

dyld_platform_t dyld_get_active_platform(void) {
    if (gUseDyld3)
        return dyld3::dyld_get_active_platform();

    return (dyld_platform_t)_dyld_get_all_image_infos()->platform;
}

dyld_platform_t dyld_get_base_platform(dyld_platform_t platform) {
    return dyld3::dyld_get_base_platform(platform);
}

bool dyld_is_simulator_platform(dyld_platform_t platform) {
    return dyld3::dyld_is_simulator_platform(platform);
}

bool dyld_sdk_at_least(const struct mach_header* mh, dyld_build_version_t version) {
    return dyld3::dyld_sdk_at_least(mh, version);
}

bool dyld_minos_at_least(const struct mach_header* mh, dyld_build_version_t version) {
    return dyld3::dyld_minos_at_least(mh, version);
}

bool dyld_program_sdk_at_least(dyld_build_version_t version) {
    return dyld3::dyld_program_sdk_at_least(version);
}

bool dyld_program_minos_at_least(dyld_build_version_t version) {
    return dyld3::dyld_program_minos_at_least(version);
}

// Function that walks through the load commands and calls the internal block for every version found
// Intended as a fallback for very complex (and rare) version checks, or for tools that need to
// print our everything for diagnostic reasons
void dyld_get_image_versions(const struct mach_header* mh, void (^callback)(dyld_platform_t platform, uint32_t sdk_version, uint32_t min_version)) {
    dyld3::dyld_get_image_versions(mh, callback);
}



#if DEPRECATED_APIS_SUPPORTED
/*
 * NSCreateObjectFileImageFromFile() creates an NSObjectFileImage for the
 * specified file name if the file is a correct Mach-O file that can be loaded
 * with NSloadModule().  For return codes of NSObjectFileImageFailure and
 * NSObjectFileImageFormat an error message is printed to stderr.  All
 * other codes cause no printing. 
 */
NSObjectFileImageReturnCode
NSCreateObjectFileImageFromFile(
const char* pathName,
NSObjectFileImage *objectFileImage)
{
	if ( gUseDyld3 )
		return dyld3::NSCreateObjectFileImageFromFile(pathName, objectFileImage);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSObjectFileImageReturnCode (*funcType)(const char*, NSObjectFileImage*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSCreateObjectFileImageFromFile", &p);
	return p(pathName, objectFileImage);
}


/*
 * NSCreateObjectFileImageFromMemory() creates an NSObjectFileImage for the
 * object file mapped into memory at address of size length if the object file
 * is a correct Mach-O file that can be loaded with NSloadModule().  For return
 * codes of NSObjectFileImageFailure and NSObjectFileImageFormat an error
 * message is printed to stderr.  All other codes cause no printing. 
 */
NSObjectFileImageReturnCode
NSCreateObjectFileImageFromMemory(
const void* address,
size_t size, 
NSObjectFileImage *objectFileImage)
{
    // <rdar://problem/51812762> NSCreatObjectFileImageFromMemory fail opaquely if Hardened runtime is enabled
    uint32_t flags;
    if ( csops(0, CS_OPS_STATUS, &flags, sizeof(flags)) != -1 ) {
        if ( (flags & (CS_ENFORCEMENT|CS_KILL)) == (CS_ENFORCEMENT|CS_KILL) ) {
            //fprintf(stderr, "dyld: warning: NSCreatObjectFileImageFromMemory() cannot be used in harden process 0x%08X\n", flags);
            return NSObjectFileImageAccess;
        }
    }

	if ( gUseDyld3 )
		return dyld3::NSCreateObjectFileImageFromMemory(address, size, objectFileImage);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSObjectFileImageReturnCode (*funcType)(const void*, size_t, NSObjectFileImage*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSCreateObjectFileImageFromMemory", &p);
	return p(address, size, objectFileImage);
}

#if OBSOLETE_DYLD_API
/*
 * NSCreateCoreFileImageFromFile() creates an NSObjectFileImage for the 
 * specified core file name if the file is a correct Mach-O core file.
 * For return codes of NSObjectFileImageFailure and NSObjectFileImageFormat
 * an error message is printed to stderr.  All other codes cause no printing. 
 */
NSObjectFileImageReturnCode
NSCreateCoreFileImageFromFile(
const char* pathName,
NSObjectFileImage *objectFileImage)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef NSObjectFileImageReturnCode (*funcType)(const char*, NSObjectFileImage*) = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSCreateCoreFileImageFromFile", &p);
	return p(pathName, objectFileImage);
}
#endif

bool
NSDestroyObjectFileImage(
NSObjectFileImage objectFileImage)
{
	if ( gUseDyld3 )
		return dyld3::NSDestroyObjectFileImage(objectFileImage);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(NSObjectFileImage);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSDestroyObjectFileImage", &p);
	return p(objectFileImage);
}


NSModule
NSLinkModule(
NSObjectFileImage objectFileImage, 
const char* moduleName,
uint32_t options)
{
	if ( gUseDyld3 )
		return dyld3::NSLinkModule(objectFileImage, moduleName, options);

	DYLD_LOCK_THIS_BLOCK;
    typedef NSModule (*funcType)(NSObjectFileImage, const char*, unsigned long);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSLinkModule", &p);
		
	return p(objectFileImage, moduleName, options);
}




/*
 * NSSymbolDefinitionCountInObjectFileImage() returns the number of symbol
 * definitions in the NSObjectFileImage.
 */
uint32_t
NSSymbolDefinitionCountInObjectFileImage(
NSObjectFileImage objectFileImage)
{
	if ( gUseDyld3 )
		return dyld3::NSSymbolDefinitionCountInObjectFileImage(objectFileImage);

	DYLD_LOCK_THIS_BLOCK;
    typedef uint32_t (*funcType)(NSObjectFileImage);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSSymbolDefinitionCountInObjectFileImage", &p);
		
	return p(objectFileImage);
}

/*
 * NSSymbolDefinitionNameInObjectFileImage() returns the name of the i'th
 * symbol definitions in the NSObjectFileImage.  If the ordinal specified is
 * outside the range [0..NSSymbolDefinitionCountInObjectFileImage], NULL will
 * be returned.
 */
const char* 
NSSymbolDefinitionNameInObjectFileImage(
NSObjectFileImage objectFileImage,
uint32_t ordinal)
{
	if ( gUseDyld3 )
		return dyld3::NSSymbolDefinitionNameInObjectFileImage(objectFileImage, ordinal);

	DYLD_LOCK_THIS_BLOCK;
    typedef const char*  (*funcType)(NSObjectFileImage, uint32_t);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSSymbolDefinitionNameInObjectFileImage", &p);
		
	return p(objectFileImage, ordinal);
}

/*
 * NSSymbolReferenceCountInObjectFileImage() returns the number of references
 * to undefined symbols the NSObjectFileImage.
 */
uint32_t
NSSymbolReferenceCountInObjectFileImage(
NSObjectFileImage objectFileImage)
{
	if ( gUseDyld3 )
		return dyld3::NSSymbolReferenceCountInObjectFileImage(objectFileImage);

	DYLD_LOCK_THIS_BLOCK;
    typedef uint32_t (*funcType)(NSObjectFileImage);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSSymbolReferenceCountInObjectFileImage", &p);
		
	return p(objectFileImage);
}

/*
 * NSSymbolReferenceNameInObjectFileImage() returns the name of the i'th
 * undefined symbol in the NSObjectFileImage. If the ordinal specified is
 * outside the range [0..NSSymbolReferenceCountInObjectFileImage], NULL will be
 * returned.
 */
const char* 
NSSymbolReferenceNameInObjectFileImage(
NSObjectFileImage objectFileImage,
uint32_t ordinal,
bool *tentative_definition) /* can be NULL */
{
	if ( gUseDyld3 )
		return dyld3::NSSymbolReferenceNameInObjectFileImage(objectFileImage, ordinal, tentative_definition);

	DYLD_LOCK_THIS_BLOCK;
    typedef const char*  (*funcType)(NSObjectFileImage, uint32_t, bool*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSSymbolReferenceNameInObjectFileImage", &p);
		
	return p(objectFileImage, ordinal, tentative_definition);
}

/*
 * NSIsSymbolDefinedInObjectFileImage() returns TRUE if the specified symbol
 * name has a definition in the NSObjectFileImage and FALSE otherwise.
 */
bool
NSIsSymbolDefinedInObjectFileImage(
NSObjectFileImage objectFileImage,
const char* symbolName)
{
	if ( gUseDyld3 )
		return dyld3::NSIsSymbolDefinedInObjectFileImage(objectFileImage, symbolName);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(NSObjectFileImage, const char*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSIsSymbolDefinedInObjectFileImage", &p);
		
	return p(objectFileImage, symbolName);
}

/*
 * NSGetSectionDataInObjectFileImage() returns a pointer to the section contents
 * in the NSObjectFileImage for the specified segmentName and sectionName if
 * it exists and it is not a zerofill section.  If not it returns NULL.  If
 * the parameter size is not NULL the size of the section is also returned
 * indirectly through that pointer.
 */
void *
NSGetSectionDataInObjectFileImage(
NSObjectFileImage objectFileImage,
const char* segmentName,
const char* sectionName,
unsigned long *size) /* can be NULL */
{
	if ( gUseDyld3 )
		return dyld3::NSGetSectionDataInObjectFileImage(objectFileImage, segmentName, sectionName, size);

	DYLD_LOCK_THIS_BLOCK;
    typedef void* (*funcType)(NSObjectFileImage, const char*, const char*, unsigned long*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_NSGetSectionDataInObjectFileImage", &p);
		
	return p(objectFileImage, segmentName, sectionName, size);
}


void
NSLinkEditError(
NSLinkEditErrors *c,
int *errorNumber, 
const char* *fileName,
const char* *errorString)
{
	if ( gUseDyld3 )
		return dyld3::NSLinkEditError(c, errorNumber, fileName, errorString);

	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(NSLinkEditErrors *c,
                             int *errorNumber,
                             const char* *fileName,
                             const char* *errorString);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_link_edit_error", &p);
	if(p != NULL)
	    p(c, errorNumber, fileName, errorString);
}

bool
NSUnLinkModule(
NSModule module, 
uint32_t options)
{
	if ( gUseDyld3 )
		return dyld3::NSUnLinkModule(module, options);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(NSModule module, uint32_t options);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_unlink_module", &p);

	return p(module, options);
}

#if OBSOLETE_DYLD_API
NSModule
NSReplaceModule(
NSModule moduleToReplace,
NSObjectFileImage newObjectFileImage, 
uint32_t options)
{
	return(NULL);
}
#endif


#endif // DEPRECATED_APIS_SUPPORTED

/*
 *_NSGetExecutablePath copies the path of the executable into the buffer and
 * returns 0 if the path was successfully copied in the provided buffer. If the
 * buffer is not large enough, -1 is returned and the expected buffer size is
 * copied in *bufsize. Note that _NSGetExecutablePath will return "a path" to
 * the executable not a "real path" to the executable. That is the path may be
 * a symbolic link and not the real file. And with deep directories the total
 * bufsize needed could be more than MAXPATHLEN.
 */
int
_NSGetExecutablePath(
char *buf,
uint32_t *bufsize)
{
	if ( gUseDyld3 )
		return dyld3::_NSGetExecutablePath(buf, bufsize);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef int (*funcType)(char *buf, uint32_t *bufsize);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld__NSGetExecutablePath", &p);
	return(p(buf, bufsize));
}

#if DEPRECATED_APIS_SUPPORTED
void
_dyld_lookup_and_bind(
const char* symbol_name,
void** address,
NSModule* module)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_lookup_and_bind(symbol_name, address, module);

	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(const char*, void** , NSModule*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_lookup_and_bind", &p);
	p(symbol_name, address, module);
}

void
_dyld_lookup_and_bind_with_hint(
const char* symbol_name,
const char* library_name_hint,
void** address,
NSModule* module)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(const char*, const char*, void**, NSModule*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_lookup_and_bind_with_hint", &p);
	p(symbol_name, library_name_hint, address, module);
}

#if OBSOLETE_DYLD_API
void
_dyld_lookup_and_bind_objc(
const char* symbol_name,
void** address,
NSModule* module)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(const char* , void**, NSModule*) = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_lookup_and_bind_objc", &p);
	p(symbol_name, address, module);
}
#endif

void
_dyld_lookup_and_bind_fully(
const char* symbol_name,
void** address,
NSModule* module)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(const char*, void**, NSModule*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_lookup_and_bind_fully", &p);
	p(symbol_name, address, module);
}

bool
_dyld_bind_fully_image_containing_address(
const void* address)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const void*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_bind_fully_image_containing_address", &p);
	return p(address);
}
#endif // DEPRECATED_APIS_SUPPORTED


/*
 * _dyld_register_func_for_add_image registers the specified function to be
 * called when a new image is added (a bundle or a dynamic shared library) to
 * the program.  When this function is first registered it is called for once
 * for each image that is currently part of the program.
 */
void
_dyld_register_func_for_add_image(
void (*func)(const struct mach_header *mh, intptr_t vmaddr_slide))
{
	if ( gUseDyld3 )
		return dyld3::_dyld_register_func_for_add_image(func);

	DYLD_LOCK_THIS_BLOCK;
	// Func must be a "void *" because dyld itself calls it. DriverKit
	// libdyld.dylib uses diversified C function pointers but its dyld (the
	// plain OS one) doesn't, so it must be resigned with 0 discriminator.
	typedef void (*funcType)(void *func);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_register_func_for_add_image", &p);
	p(reinterpret_cast<void *>(func));
}

/*
 * _dyld_register_func_for_remove_image registers the specified function to be
 * called when an image is removed (a bundle or a dynamic shared library) from
 * the program.
 */
void
_dyld_register_func_for_remove_image(
void (*func)(const struct mach_header *mh, intptr_t vmaddr_slide))
{
	if ( gUseDyld3 )
		return dyld3::_dyld_register_func_for_remove_image(func);

	DYLD_LOCK_THIS_BLOCK;
	// Func must be a "void *" because dyld itself calls it. DriverKit
	// libdyld.dylib uses diversified C function pointers but its dyld (the
	// plain OS one) doesn't, so it must be resigned with 0 discriminator.
	typedef void (*funcType)(void *func);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_register_func_for_remove_image", &p);
	p(reinterpret_cast<void *>(func));
}

#if OBSOLETE_DYLD_API
/*
 * _dyld_register_func_for_link_module registers the specified function to be
 * called when a module is bound into the program.  When this function is first
 * registered it is called for once for each module that is currently bound into
 * the program.
 */
void
_dyld_register_func_for_link_module(
void (*func)(NSModule module))
{
	DYLD_LOCK_THIS_BLOCK;
	// Func must be a "void *" because dyld itself calls it. DriverKit
	// libdyld.dylib uses diversified C function pointers but its dyld (the
	// plain OS one) doesn't, so it must be resigned with 0 discriminator.
	static void (*funcType)(void *func) = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_register_func_for_link_module", &p);
	p(reinterpret_cast<void *>(func));
}

/*
 * _dyld_register_func_for_unlink_module registers the specified function to be
 * called when a module is unbound from the program.
 */
void
_dyld_register_func_for_unlink_module(
void (*func)(NSModule module))
{
	DYLD_LOCK_THIS_BLOCK;
	// Func must be a "void *" because dyld itself calls it. DriverKit
	// libdyld.dylib uses diversified C function pointers but its dyld (the
	// plain OS one) doesn't, so it must be resigned with 0 discriminator.
	static void (*funcType)(void *func) = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_register_func_for_unlink_module", &p);
	p(reinterpret_cast<void *>(func));
}

/*
 * _dyld_register_func_for_replace_module registers the specified function to be
 * called when a module is to be replace with another module in the program.
 */
void
_dyld_register_func_for_replace_module(
void (*func)(NSModule oldmodule, NSModule newmodule))
{
	DYLD_LOCK_THIS_BLOCK;
	// Func must be a "void *" because dyld itself calls it. DriverKit
	// libdyld.dylib uses diversified C function pointers but its dyld (the
	// plain OS one) doesn't, so it must be resigned with 0 discriminator.
    typedef void (*funcType)(void *func) = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_register_func_for_replace_module", &p);
	p(reinterpret_cast<void *>(func));
}


/*
 * _dyld_get_objc_module_sect_for_module is passed a module and sets a 
 * pointer to the (__OBJC,__module) section and its size for the specified
 * module.
 */
void
_dyld_get_objc_module_sect_for_module(
NSModule module,
void **objc_module,
unsigned long *size)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(NSModule module,
                             void **objc_module,
                             unsigned long *size) = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_objc_module_sect_for_module", &p);
	p(module, objc_module, size);
}

#endif

#if DEPRECATED_APIS_SUPPORTED
bool
_dyld_present(void)
{
	// this function exists for compatiblity only
	return true;
}
#endif

uint32_t
_dyld_image_count(void)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_image_count();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef uint32_t (*funcType)(void);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_image_count", &p);
	return(p());
}

const struct mach_header *
_dyld_get_image_header(uint32_t image_index)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_image_header(image_index);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef struct mach_header * (*funcType)(uint32_t image_index);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_image_header", &p);
	return(p(image_index));
}

intptr_t
_dyld_get_image_vmaddr_slide(uint32_t image_index)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_image_vmaddr_slide(image_index);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef unsigned long (*funcType)(uint32_t image_index);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_image_vmaddr_slide", &p);
	return(p(image_index));
}

const char* 
_dyld_get_image_name(uint32_t image_index)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_image_name(image_index);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef const char*  (*funcType)(uint32_t image_index);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_image_name", &p);
	return(p(image_index));
}

// SPI in Mac OS X 10.6
intptr_t _dyld_get_image_slide(const struct mach_header* mh)
{
	// always use dyld3 version because it does better error handling
	return dyld3::_dyld_get_image_slide(mh);
}

const struct mach_header *
_dyld_get_prog_image_header()
{
    if ( gUseDyld3 )
        return dyld3::_dyld_get_prog_image_header();

    DYLD_LOCK_THIS_BLOCK;
    typedef const struct mach_header * (*funcType)(void);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_get_prog_image_header", &p);
    return p();
}

#if DEPRECATED_APIS_SUPPORTED
bool
_dyld_image_containing_address(const void* address)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_image_containing_address(address);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const void*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_image_containing_address", &p);
	return(p(address));
}

const struct mach_header *
_dyld_get_image_header_containing_address(
const void* address)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_image_header_containing_address(address);

	DYLD_LOCK_THIS_BLOCK;
    typedef const struct mach_header * (*funcType)(const void*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_image_header_containing_address", &p);
	return p(address);
}

bool _dyld_launched_prebound(void)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(void);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_launched_prebound", &p);
	return(p());
}

bool _dyld_all_twolevel_modules_prebound(void)
{
	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(void);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_all_twolevel_modules_prebound", &p);
	return(p());
}
#endif // DEPRECATED_APIS_SUPPORTED


#include <dlfcn_private.h>
#include <stddef.h>
#include <pthread.h>
#include <stdlib.h>
#include <mach-o/dyld.h>
#include "dyldLibSystemInterface.h"


// pthread key used to access per-thread dlerror message
static pthread_key_t dlerrorPerThreadKey;
static bool dlerrorPerThreadKeyInitialized = false;

// data kept per-thread
struct dlerrorPerThreadData
{
	size_t		sizeAllocated;
	char		message[1];
};

// function called by dyld to get buffer to store dlerror message
static char* getPerThreadBufferFor_dlerror(size_t sizeRequired)
{
	// ok to create key lazily because this function is called within dyld lock, so there is no race condition
	if (!dlerrorPerThreadKeyInitialized ) {
		// create key and tell pthread package to call free() on any data associated with key if thread dies
		pthread_key_create(&dlerrorPerThreadKey, &free);
		dlerrorPerThreadKeyInitialized = true;
	}

	const size_t size = (sizeRequired < 256) ? 256 : sizeRequired;
	dlerrorPerThreadData* data = (dlerrorPerThreadData*)pthread_getspecific(dlerrorPerThreadKey);
	if ( data == NULL ) {
		//int mallocSize = offsetof(dlerrorPerThreadData, message[size]);
		const size_t mallocSize = sizeof(dlerrorPerThreadData)+size;
		data = (dlerrorPerThreadData*)malloc(mallocSize);
		data->sizeAllocated = size;
		pthread_setspecific(dlerrorPerThreadKey, data);
	}
	else if ( data->sizeAllocated < sizeRequired ) {
		free(data);
		//int mallocSize = offsetof(dlerrorPerThreadData, message[size]);
		const size_t mallocSize = sizeof(dlerrorPerThreadData)+size;
		data = (dlerrorPerThreadData*)malloc(mallocSize);
		data->sizeAllocated = size;
		pthread_setspecific(dlerrorPerThreadKey, data);
	}
	return data->message;
}

// <rdar://problem/10595338> dlerror buffer leak
// Only allocate buffer if an actual error message needs to be set
static bool hasPerThreadBufferFor_dlerror()
{
	if (!dlerrorPerThreadKeyInitialized ) 
		return false;
		
	return (pthread_getspecific(dlerrorPerThreadKey) != NULL);
}

#if TARGET_OS_DRIVERKIT
static bool isLaunchdOwned()
{
    return false;
}
#else
// use non-lazy pointer to vproc_swap_integer so that lazy binding does not recurse
typedef vproc_err_t (*vswapproc)(vproc_t vp, vproc_gsk_t key,int64_t *inval, int64_t *outval);
static vswapproc swapProc = &vproc_swap_integer;

static bool isLaunchdOwned()
{
    static bool checked = false;
    static bool result = false;
    if ( !checked ) {
        checked = true;
	    int64_t val = 0;
	    (*swapProc)(NULL, VPROC_GSK_IS_MANAGED, NULL, &val);
	    result = ( val != 0 );
    }
    return result;
}
#endif

static void shared_cache_missing()
{
	// leave until dyld's that might call this are rare
}

static void shared_cache_out_of_date()
{
	// leave until dyld's that might call this are rare
}

// FIXME: This is a mess. Why can't Driverkit have its own dyld?
static int cxa_atexit_thunk(void (*func)(void *), void *arg, void *dso)
{
	// Func will have come from dyld and so be signed with 0 discriminator,
	// resign it appropriately before passing to the real __cxa_atexit.
	func = ptrauth_auth_and_resign(func, ptrauth_key_function_pointer, 0,
	    ptrauth_key_function_pointer,
	    ptrauth_function_pointer_type_discriminator(__typeof__(func)));
	return __cxa_atexit(func, arg, dso);
}

template<typename FTy> static FTy *resign_for_dyld(FTy *func) {
	return ptrauth_auth_and_resign(func, ptrauth_key_function_pointer,
	    ptrauth_function_pointer_type_discriminator(__typeof__(func)),
	    ptrauth_key_function_pointer, 0);
}


// the table passed to dyld containing thread helpers
static dyld::LibSystemHelpers sHelpers = { 13 };

static const objc_opt::objc_opt_t* gObjCOpt = nullptr;
//
// during initialization of libSystem this routine will run
// and call dyld, registering the helper functions.
//
extern "C" void tlv_initializer();
void _dyld_initializer()
{
    sHelpers.acquireGlobalDyldLock = resign_for_dyld(&dyldGlobalLockAcquire);
    sHelpers.releaseGlobalDyldLock = resign_for_dyld(&dyldGlobalLockRelease);
    sHelpers.getThreadBufferFor_dlerror = resign_for_dyld(&getPerThreadBufferFor_dlerror);
    sHelpers.malloc = resign_for_dyld(&malloc);
    sHelpers.free = resign_for_dyld(&free);
    sHelpers.cxa_atexit = resign_for_dyld(&cxa_atexit_thunk);
    sHelpers.dyld_shared_cache_missing = resign_for_dyld(&shared_cache_missing);
    sHelpers.dyld_shared_cache_out_of_date = resign_for_dyld(&shared_cache_out_of_date);
    sHelpers.acquireDyldInitializerLock = NULL;
    sHelpers.releaseDyldInitializerLock = NULL;
    sHelpers.pthread_key_create = resign_for_dyld(&pthread_key_create);
    sHelpers.pthread_setspecific = resign_for_dyld(&pthread_setspecific);
    sHelpers.malloc_size = resign_for_dyld(&malloc_size);
    sHelpers.pthread_getspecific = resign_for_dyld(&pthread_getspecific);
    sHelpers.cxa_finalize = resign_for_dyld(&__cxa_finalize);
    sHelpers.startGlueToCallExit = address_of_start;
    sHelpers.hasPerThreadBufferFor_dlerror = resign_for_dyld(&hasPerThreadBufferFor_dlerror);
    sHelpers.isLaunchdOwned = resign_for_dyld(&isLaunchdOwned);
    sHelpers.vm_alloc = resign_for_dyld(&vm_allocate);
    sHelpers.mmap = resign_for_dyld(&mmap);
    sHelpers.cxa_finalize_ranges = resign_for_dyld(&__cxa_finalize_ranges);

    typedef void (*funcType)(dyld::LibSystemHelpers*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    // Get the optimized objc pointer now that the cache is loaded
    const dyld_all_image_infos* allInfo = _dyld_get_all_image_infos();
    if ( allInfo != nullptr  ) {
        const DyldSharedCache* cache = (const DyldSharedCache*)(allInfo->sharedCacheBaseAddress);
        if ( cache != nullptr )
            gObjCOpt = cache->objcOpt();
    }

	if ( gUseDyld3 ) {
		dyld3::gAllImages.applyInitialImages();
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
        // For binaries built before 13.0, set the lookup function if they need it
        if (dyld_get_program_sdk_version() < DYLD_PACKED_VERSION(13,0,0))
            setLookupFunc((void*)&dyld3::compatFuncLookup);
#endif
	}
	else {
		dyld_func_lookup_and_resign("__dyld_register_thread_helpers", &p);
		if(p != NULL)
			p(&sHelpers);
	}

	tlv_initializer();
}

int dladdr(const void* addr, Dl_info* info)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_DLADDR, (uint64_t)addr, 0, 0);
    int result = 0;
    if ( gUseDyld3 ) {
        result = dyld3::dladdr(addr, info);
    } else {
        DYLD_LOCK_THIS_BLOCK;
        typedef int (*funcType)(const void* , Dl_info*);
        static funcType __ptrauth_dyld_function_ptr p = NULL;

        if(p == NULL)
            dyld_func_lookup_and_resign("__dyld_dladdr", &p);
        result = p(addr, info);
    }
    timer.setData4(result);
    timer.setData5(info != NULL ? info->dli_fbase : 0);
    timer.setData6(info != NULL ? info->dli_saddr : 0);
    return result;
}

#if !TARGET_OS_DRIVERKIT
char* dlerror()
{
	if ( gUseDyld3 )
		return dyld3::dlerror();

	DYLD_LOCK_THIS_BLOCK;
    typedef char* (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_dlerror", &p);
	return(p());
}

int dlclose(void* handle)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_DLCLOSE, (uint64_t)handle, 0, 0);
    int result = 0;
    if ( gUseDyld3 ) {
        timer.setData4(result);
		return dyld3::dlclose(handle);
    }

	DYLD_LOCK_THIS_BLOCK;
    typedef int (*funcType)(void* handle);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_dlclose", &p);
    result = p(handle);
    timer.setData4(result);
	return result;
}

static void* dlopen_internal(const char* path, int mode, void* callerAddress)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_DLOPEN, path, mode, 0);
    void* result = nullptr;
    if ( gUseDyld3 ) {
        result = dyld3::dlopen_internal(path, mode, callerAddress);
        timer.setData4(result);
        return result;
    }

    // dlopen is special. locking is done inside dyld to allow initializer to run without lock
    DYLD_NO_LOCK_THIS_BLOCK;

    typedef void* (*funcType)(const char* path, int, void*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_dlopen_internal", &p);
    result = p(path, mode, callerAddress);
    // use asm block to prevent tail call optimization
    // this is needed because dlopen uses __builtin_return_address() and depends on this glue being in the frame chain
    // <rdar://problem/5313172 dlopen() looks too far up stack, can cause crash>
    __asm__ volatile("");
    timer.setData4(result);

	return result;
}

void* dlopen(const char* path, int mode)
{
    void* result = dlopen_internal(path, mode, __builtin_return_address(0));
    if ( result )
        return result;


    return nullptr;
}

void* dlopen_from(const char* path, int mode, void* addressInCaller)
{
#if __has_feature(ptrauth_calls)
    addressInCaller = __builtin_ptrauth_strip(addressInCaller, ptrauth_key_asia);
#endif
    return dlopen_internal(path, mode, addressInCaller);
}

#if !__i386__
void* dlopen_audited(const char* path, int mode)
{
	return dlopen(path, mode);
}
#endif // !__i386__

bool dlopen_preflight(const char* path)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_DLOPEN_PREFLIGHT, path, 0, 0);
    bool result = false;

    if ( gUseDyld3 ) {
        result = dyld3::dlopen_preflight_internal(path);
        timer.setData4(result);
        return result;
    }

    DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const char* path, void* callerAddress);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_dlopen_preflight_internal", &p);
    result = p(path, __builtin_return_address(0));
    timer.setData4(result);
    return result;
}

void* dlsym(void* handle, const char* symbol)
{
    dyld3::ScopedTimer timer(DBG_DYLD_TIMING_DLSYM, handle, symbol, 0);
    void* result = nullptr;

    if ( gUseDyld3 ) {
        result = dyld3::dlsym_internal(handle, symbol, __builtin_return_address(0));
        timer.setData4(result);
        return result;
    }

    DYLD_LOCK_THIS_BLOCK;
    typedef void* (*funcType)(void* handle, const char* symbol, void *callerAddress);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_dlsym_internal", &p);
    result = p(handle, symbol, __builtin_return_address(0));
    timer.setData4(result);
    return result;
}
#endif // !TARGET_OS_DRIVERKIT


const struct dyld_all_image_infos* _dyld_get_all_image_infos()
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_all_image_infos();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef struct dyld_all_image_infos* (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_all_image_infos", &p);
	return p();
}

#if SUPPORT_ZERO_COST_EXCEPTIONS
bool _dyld_find_unwind_sections(void* addr, dyld_unwind_sections* info)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_find_unwind_sections(addr, info);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef void* (*funcType)(void*, dyld_unwind_sections*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_find_unwind_sections", &p);
	return p(addr, info);
}
#endif


#if __i386__ || __x86_64__ || __arm__ || __arm64__
__attribute__((visibility("hidden"))) 
void* _dyld_fast_stub_entry(void* loadercache, long lazyinfo)
{
	DYLD_NO_LOCK_THIS_BLOCK;
    typedef void* (*funcType)(void*, long);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_fast_stub_entry", &p);
	return p(loadercache, lazyinfo);
}
#endif


const char* dyld_image_path_containing_address(const void* addr)
{
	if ( gUseDyld3 )
		return dyld3::dyld_image_path_containing_address(addr);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef const char* (*funcType)(const void*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_image_path_containing_address", &p);
	return p(addr);
}

const struct mach_header* dyld_image_header_containing_address(const void* addr)
{
	if ( gUseDyld3 )
		return dyld3::dyld_image_header_containing_address(addr);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef const mach_header* (*funcType)(const void*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_image_header_containing_address", &p);
	return p(addr);
}


bool dyld_shared_cache_some_image_overridden()
{
	if ( gUseDyld3 )
		return dyld3::dyld_shared_cache_some_image_overridden();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef bool (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_shared_cache_some_image_overridden", &p);
	return p();
}

bool _dyld_get_shared_cache_uuid(uuid_t uuid)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_shared_cache_uuid(uuid);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(uuid_t);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_shared_cache_uuid", &p);
	return p(uuid);
}

const void* _dyld_get_shared_cache_range(size_t* length)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_get_shared_cache_range(length);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef const void* (*funcType)(size_t*);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_get_shared_cache_range", &p);
	return p(length);
}

bool _dyld_shared_cache_optimized()
{
    if ( gUseDyld3 )
        return dyld3::_dyld_shared_cache_optimized();

    const dyld_all_image_infos* allInfo = _dyld_get_all_image_infos();
    if ( allInfo != nullptr  ) {
        const dyld_cache_header* cacheHeader = (dyld_cache_header*)(allInfo->sharedCacheBaseAddress);
        if ( cacheHeader != nullptr )
            return (cacheHeader->cacheType == kDyldSharedCacheTypeProduction);
    }
    return false;
}

bool _dyld_shared_cache_is_locally_built()
{
    if ( gUseDyld3 )
        return dyld3::_dyld_shared_cache_is_locally_built();

    const dyld_all_image_infos* allInfo = _dyld_get_all_image_infos();
    if ( allInfo != nullptr  ) {
        const dyld_cache_header* cacheHeader = (dyld_cache_header*)(allInfo->sharedCacheBaseAddress);
        if ( cacheHeader != nullptr )
            return (cacheHeader->locallyBuiltCache == 1);
    }
    return false;
}

const char* _dyld_shared_cache_real_path(const char* path)
{
    const dyld_all_image_infos* allInfo = _dyld_get_all_image_infos();
    if ( allInfo != nullptr  ) {
        const DyldSharedCache* cache = (const DyldSharedCache*)(allInfo->sharedCacheBaseAddress);
        if ( cache != nullptr )
            return cache->getCanonicalPath(path);
    }
    return nullptr;
}

bool _dyld_shared_cache_contains_path(const char* path)
{
    return _dyld_shared_cache_real_path(path) != nullptr;
}


uint32_t _dyld_launch_mode()
{
    if ( gUseDyld3 )
        return dyld3::_dyld_launch_mode();

    // in dyld2 mode all flag bits are zero
    return 0;
}

void _dyld_images_for_addresses(unsigned count, const void* addresses[], struct dyld_image_uuid_offset infos[])
{
    if ( gUseDyld3 )
        return dyld3::_dyld_images_for_addresses(count, addresses, infos);

    DYLD_NO_LOCK_THIS_BLOCK;
    typedef const void (*funcType)(unsigned, const void*[], struct dyld_image_uuid_offset[]);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_images_for_addresses", &p);
    return p(count, addresses, infos);
}

void _dyld_register_for_image_loads(void (*func)(const mach_header* mh, const char* path, bool unloadable))
{
    if ( gUseDyld3 )
        return dyld3::_dyld_register_for_image_loads(func);

    DYLD_NO_LOCK_THIS_BLOCK;
    typedef const void (*funcType)(void (*)(const mach_header* mh, const char* path, bool unloadable));
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_register_for_image_loads", &p);
    return p(func);
}

void _dyld_register_for_bulk_image_loads(void (*func)(unsigned imageCount, const struct mach_header* mhs[], const char* paths[]))
{
    if ( gUseDyld3 )
        return dyld3::_dyld_register_for_bulk_image_loads(func);

    DYLD_NO_LOCK_THIS_BLOCK;
    typedef const void (*funcType)(void (*)(unsigned imageCount, const mach_header* mhs[], const char* paths[]));
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_register_for_bulk_image_loads", &p);
    return p(func);
}

bool dyld_need_closure(const char* execPath, const char* dataContainerRootDir)
{
    return dyld3::dyld_need_closure(execPath, dataContainerRootDir);
}

bool dyld_process_is_restricted()
{
	if ( gUseDyld3 )
		return dyld3::dyld_process_is_restricted();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef bool (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;
	
	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_process_is_restricted", &p);
	return p();
}

const char* dyld_shared_cache_file_path()
{
	if ( gUseDyld3 )
		return dyld3::dyld_shared_cache_file_path();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef const char* (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;
	
	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_shared_cache_file_path", &p);
	return p();
}

bool dyld_has_inserted_or_interposing_libraries()
{
	if ( gUseDyld3 )
		return dyld3::dyld_has_inserted_or_interposing_libraries();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef bool (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if (p == NULL)
	    dyld_func_lookup_and_resign("__dyld_has_inserted_or_interposing_libraries", &p);
	return p();
}

bool _dyld_has_fix_for_radar(const char *rdar) {
    // There is no point in shimming this to dyld3, actual functionality can exist purely in libSystem for
    // both dyld2 and dyld3.
    return false;
}


void dyld_dynamic_interpose(const struct mach_header* mh, const struct dyld_interpose_tuple array[], size_t count)
{
	if ( gUseDyld3 )
		return dyld3::dyld_dynamic_interpose(mh, array, count);

	DYLD_LOCK_THIS_BLOCK;
    typedef void (*funcType)(const struct mach_header* mh, const struct dyld_interpose_tuple array[], size_t count);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if (p == NULL)
	    dyld_func_lookup_and_resign("__dyld_dynamic_interpose", &p);
	p(mh, array, count);
}

// SPI called __fork
void _dyld_atfork_prepare()
{
    if ( gUseDyld3 )
        return dyld3::_dyld_atfork_prepare();
}

// SPI called __fork
void _dyld_atfork_parent()
{
    if ( gUseDyld3 )
        return dyld3::_dyld_atfork_parent();
}

// SPI called __fork
void _dyld_fork_child()
{
	if ( gUseDyld3 )
		return dyld3::_dyld_fork_child();

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef void (*funcType)();
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_fork_child", &p);
	return p();
}



static void* mapStartOfCache(const char* path, size_t length)
{
	struct stat statbuf;
	if ( dyld3::stat(path, &statbuf) == -1 )
		return NULL;

	if ( (size_t)statbuf.st_size < length )
		return NULL;

	int cache_fd = dyld3::open(path, O_RDONLY, 0);
	if ( cache_fd < 0 )
		return NULL;

	void* result = ::mmap(NULL, length, PROT_READ, MAP_PRIVATE, cache_fd, 0);
	close(cache_fd);

	if ( result == MAP_FAILED )
		return NULL;

	return result;
}


static const dyld_cache_header* findCacheInDirAndMap(const uuid_t cacheUuid, const char* dirPath)
{
	DIR* dirp = ::opendir(dirPath);
	if ( dirp != NULL) {
		dirent entry;
		dirent* entp = NULL;
		char cachePath[PATH_MAX];
		while ( ::readdir_r(dirp, &entry, &entp) == 0 ) {
			if ( entp == NULL )
				break;
			if ( entp->d_type != DT_REG ) 
				continue;
			if ( strlcpy(cachePath, dirPath, PATH_MAX) >= PATH_MAX )
				continue;
			if ( strlcat(cachePath, "/", PATH_MAX) >= PATH_MAX )
				continue;
			if ( strlcat(cachePath, entp->d_name, PATH_MAX) >= PATH_MAX )
				continue;
			if ( const dyld_cache_header* cacheHeader = (dyld_cache_header*)mapStartOfCache(cachePath, 0x00100000) ) {
				if ( (::memcmp(cacheHeader, "dyld_", 5) != 0) || (::memcmp(cacheHeader->uuid, cacheUuid, 16) != 0) ) {
					// wrong uuid, unmap and keep looking
					::munmap((void*)cacheHeader, 0x00100000);
				}
				else {
					// found cache
					closedir(dirp);
					return cacheHeader;
				}
			}
		}
		closedir(dirp);
	}
	return NULL;
}

int dyld_shared_cache_find_iterate_text(const uuid_t cacheUuid, const char* extraSearchDirs[], void (^callback)(const dyld_shared_cache_dylib_text_info* info))
{
	if ( gUseDyld3 )
		return dyld3::dyld_shared_cache_find_iterate_text(cacheUuid, extraSearchDirs, callback);

	const dyld_cache_header* cacheHeader = NULL;
	bool needToUnmap = true;

	// get info from dyld about this process, to see if requested cache is already mapped into this process
	const dyld_all_image_infos* allInfo = _dyld_get_all_image_infos();
	if ( (allInfo != NULL) && (allInfo->sharedCacheBaseAddress != 0) && (memcmp(allInfo->sharedCacheUUID, cacheUuid, 16) == 0) ) {
		// requested cache is already mapped, just re-use it
		cacheHeader = (dyld_cache_header*)(allInfo->sharedCacheBaseAddress);
		needToUnmap = false;
	}
	else {
		// look first is default location for cache files
    #if TARGET_OS_IPHONE
		cacheHeader = findCacheInDirAndMap(cacheUuid, IPHONE_DYLD_SHARED_CACHE_DIR);
    #else
		cacheHeader = findCacheInDirAndMap(cacheUuid, MACOSX_MRM_DYLD_SHARED_CACHE_DIR);
    #endif
		// if not there, look in extra search locations
		if ( cacheHeader == NULL ) {
			for (const char** p = extraSearchDirs; *p != NULL; ++p) {
				cacheHeader = findCacheInDirAndMap(cacheUuid, *p);
				if ( cacheHeader != NULL )
					break;
			}
		}
	}

	if ( cacheHeader == NULL )
		return -1;
	
	if ( cacheHeader->mappingOffset <= __offsetof(dyld_cache_header, imagesTextOffset) ) {
		// old cache without imagesText array
		if ( needToUnmap )
			::munmap((void*)cacheHeader, 0x00100000);
		return -1;
	}

	// walk imageText table and call callback for each entry
	const dyld_cache_mapping_info* mappings = (dyld_cache_mapping_info*)((char*)cacheHeader + cacheHeader->mappingOffset);
	const uint64_t cacheUnslidBaseAddress = mappings[0].address;
	const dyld_cache_image_text_info* imagesText = (dyld_cache_image_text_info*)((char*)cacheHeader + cacheHeader->imagesTextOffset);
	const dyld_cache_image_text_info* imagesTextEnd = &imagesText[cacheHeader->imagesTextCount];
	for (const dyld_cache_image_text_info* p=imagesText; p < imagesTextEnd; ++p) {
		dyld_shared_cache_dylib_text_info dylibTextInfo;
		dylibTextInfo.version			= 2;
		dylibTextInfo.loadAddressUnslid = p->loadAddress;
		dylibTextInfo.textSegmentSize	= p->textSegmentSize;
		dylibTextInfo.path				= (char*)cacheHeader + p->pathOffset;
		::memcpy(dylibTextInfo.dylibUuid, p->uuid, 16);
		dylibTextInfo.textSegmentOffset = p->loadAddress - cacheUnslidBaseAddress;
		callback(&dylibTextInfo);
	}

	if ( needToUnmap )
		::munmap((void*)cacheHeader, 0x00100000);

	return 0;
}

int dyld_shared_cache_iterate_text(const uuid_t cacheUuid, void (^callback)(const dyld_shared_cache_dylib_text_info* info))
{
	if ( gUseDyld3 )
		return dyld3::dyld_shared_cache_iterate_text(cacheUuid, callback);

	const char* extraSearchDirs[] = { NULL };
	return dyld_shared_cache_find_iterate_text(cacheUuid, extraSearchDirs, callback);
}


bool _dyld_is_memory_immutable(const void* addr, size_t length)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_is_memory_immutable(addr, length);

	DYLD_NO_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(const void*, size_t);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_is_memory_immutable", &p);
	return p(addr, length);
}


void _dyld_objc_notify_register(_dyld_objc_notify_mapped    mapped,
                                _dyld_objc_notify_init      init,
                                _dyld_objc_notify_unmapped  unmapped)
{
	if ( gUseDyld3 )
		return dyld3::_dyld_objc_notify_register(mapped, init, unmapped);

	DYLD_LOCK_THIS_BLOCK;
    typedef bool (*funcType)(_dyld_objc_notify_mapped, _dyld_objc_notify_init, _dyld_objc_notify_unmapped);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

	if(p == NULL)
	    dyld_func_lookup_and_resign("__dyld_objc_notify_register", &p);
	p(mapped, init, unmapped);
}

void _dyld_missing_symbol_abort()
{
    return dyld3::_dyld_missing_symbol_abort();
}

const char* _dyld_get_objc_selector(const char* selName)
{
    // Check the shared cache table if it exists.
    if ( gObjCOpt != nullptr ) {
        if ( const objc_opt::objc_selopt_t* selopt = gObjCOpt->selopt() ) {
            const char* name = selopt->get(selName);
            if (name != nullptr)
                return name;
        }
    }

    if ( gUseDyld3 )
        return dyld3::_dyld_get_objc_selector(selName);

    return nullptr;
}

void _dyld_for_each_objc_class(const char* className,
                           void (^callback)(void* classPtr, bool isLoaded, bool* stop)) {
    if ( gUseDyld3 )
        return dyld3::_dyld_for_each_objc_class(className, callback);
}

void _dyld_for_each_objc_protocol(const char* protocolName,
                                  void (^callback)(void* protocolPtr, bool isLoaded, bool* stop)) {
    if ( gUseDyld3 )
        return dyld3::_dyld_for_each_objc_protocol(protocolName, callback);
}

void _dyld_register_driverkit_main(void (*mainFunc)(void))
{
    if ( gUseDyld3 )
        return dyld3::_dyld_register_driverkit_main(mainFunc);

    typedef bool (*funcType)(void *);
    static funcType __ptrauth_dyld_function_ptr p = NULL;

    if(p == NULL)
        dyld_func_lookup_and_resign("__dyld_register_driverkit_main", &p);
    p(reinterpret_cast<void *>(mainFunc));
}

// This is populated in the shared cache builder, so that the ranges are protected by __DATA_CONST
// If we have a root, we can find this range in the shared cache libdyld at runtime
typedef std::pair<const uint8_t*, const uint8_t*> ObjCConstantRange;

#if TARGET_OS_OSX
__attribute__((section(("__DATA, __objc_ranges"))))
#else
__attribute__((section(("__DATA_CONST, __objc_ranges"))))
#endif
__attribute__((used))
static ObjCConstantRange gSharedCacheObjCConstantRanges[dyld_objc_string_kind + 1];

static std::pair<const void*, uint64_t> getDyldCacheConstantRanges() {
    const dyld_all_image_infos* allInfo = _dyld_get_all_image_infos();
    if ( allInfo != nullptr  ) {
        const DyldSharedCache* cache = (const DyldSharedCache*)(allInfo->sharedCacheBaseAddress);
        if ( cache != nullptr ) {
            return cache->getObjCConstantRange();
        }
    }
    return { nullptr, 0 };
}

bool _dyld_is_objc_constant(DyldObjCConstantKind kind, const void* addr) {
    assert(kind <= dyld_objc_string_kind);
    // The common case should be that the value is in range, as this is a security
    // check, so first test against the values in the struct.  If we have a root then
    // we'll take the slow path later
    if ( (addr >= gSharedCacheObjCConstantRanges[kind].first) && (addr < gSharedCacheObjCConstantRanges[kind].second) ) {
        // Make sure that we are pointing at the start of a constant object, not in to the middle of it
        uint64_t offset = (uint64_t)addr - (uint64_t)gSharedCacheObjCConstantRanges[kind].first;
        return (offset % (uint64_t)DyldSharedCache::ConstantClasses::cfStringAtomSize) == 0;
    }

    // If we are in the shared cache, then the above check was sufficient, so this really isn't a valid constant address
    extern void* __dso_handle;
    const dyld3::MachOAnalyzer* ma = (const dyld3::MachOAnalyzer*)&__dso_handle;
    if ( ma->inDyldCache() )
        return false;

    // We now know we are a root, so use the pointers in the shared cache libdyld version of gSharedCacheObjCConstantRanges
    static std::pair<const void*, uint64_t> sharedCacheRanges = { nullptr, ~0ULL };

    // FIXME: Should we fold this in as an inititalizer above?
    // That would mean we need to link against somewhere to get ___cxa_guard_acquire/___cxa_guard_release
    if ( sharedCacheRanges.second == ~0ULL )
        sharedCacheRanges = getDyldCacheConstantRanges();

    // We have the range of the section in libdyld in the shared cache, now get an array of ranges from it
    uint64_t numRanges = sharedCacheRanges.second / sizeof(ObjCConstantRange);
    if ( kind >= numRanges )
        return false;

    const ObjCConstantRange* rangeArrayBase = (const ObjCConstantRange*)sharedCacheRanges.first;
    if ( (addr >= rangeArrayBase[kind].first) && (addr < rangeArrayBase[kind].second) ) {
        // Make sure that we are pointing at the start of a constant object, not in to the middle of it
        uint64_t offset = (uint64_t)addr - (uint64_t)rangeArrayBase[kind].first;
        return (offset % (uint64_t)DyldSharedCache::ConstantClasses::cfStringAtomSize) == 0;
    }
    return false;
}
