/*
 * Copyright (c) 1998-2016 Apple Inc. All rights reserved.
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

#ifndef __DISKARBITRATIOND_DABASE__
#define __DISKARBITRATIOND_DABASE__

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <SystemConfiguration/SystemConfiguration.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ___kCFUUIDNull CFUUIDGetConstantUUIDWithBytes( 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 )

#define ___EDIRTY EILSEQ

#define ___FS_DEFAULT_DIR "/Library/Filesystems"

#define ___PREFS_DEFAULT_DIR "/Library/Preferences/SystemConfiguration"

typedef char ___io_path_t[1024];

__private_extern__ int             ___isautofs( const char * path );
__private_extern__ int             ___mkdir( const char * path, mode_t mode );
__private_extern__ void            ___vproc_transaction_begin( void );
__private_extern__ void            ___vproc_transaction_end( void );
__private_extern__ const void *    ___CFArrayGetValue( CFArrayRef array, const void * value );
__private_extern__ void            ___CFArrayIntersect( CFMutableArrayRef array1, CFArrayRef array2 );
__private_extern__ CFStringRef     ___CFBundleCopyLocalizedStringInDirectory( CFURLRef bundleURL, CFStringRef key, CFStringRef value, CFStringRef table );
__private_extern__ CFURLRef        ___CFBundleCopyResourceURLInDirectory( CFURLRef bundleURL, CFStringRef resourcePath );
__private_extern__ CFDataRef       ___CFDataCreateFromString( CFAllocatorRef allocator, CFStringRef string );
__private_extern__ CFDictionaryRef ___CFDictionaryCreateFromXMLString( CFAllocatorRef allocator, CFStringRef string );
__private_extern__ const void *    ___CFDictionaryGetAnyValue( CFDictionaryRef dictionary );
__private_extern__ char *          ___CFStringCreateCStringWithFormatAndArguments( const char * format, va_list arguments );
__private_extern__ Boolean         ___CFStringGetCString( CFStringRef string, char * buffer, CFIndex length );
__private_extern__ void            ___CFStringInsertFormat( CFMutableStringRef string, CFIndex index, CFStringRef format, ... );
__private_extern__ void            ___CFStringInsertFormatAndArguments( CFMutableStringRef string, CFIndex index, CFStringRef format, va_list arguments );
__private_extern__ void            ___CFStringPad( CFMutableStringRef string, CFStringRef pad, CFIndex length, CFIndex index );
__private_extern__ CFUUIDRef       ___CFUUIDCreateFromName( CFAllocatorRef allocator, CFUUIDRef space, CFDataRef name );
__private_extern__ CFUUIDRef       ___CFUUIDCreateFromString( CFAllocatorRef allocator, CFStringRef string );
__private_extern__ CFStringRef     ___CFURLCopyRawDeviceFileSystemPath( CFURLRef url, CFURLPathStyle pathStyle );
__private_extern__ kern_return_t   ___IORegistryEntryGetPath( io_registry_entry_t entry, const io_name_t plane, ___io_path_t path );
__private_extern__ CFArrayRef      ___SCDynamicStoreCopyConsoleInformation( SCDynamicStoreRef store );
__private_extern__ CFStringRef     ___SCDynamicStoreCopyConsoleUser( SCDynamicStoreRef store, uid_t * uid, gid_t * gid );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !__DISKARBITRATIOND_DABASE__ */
