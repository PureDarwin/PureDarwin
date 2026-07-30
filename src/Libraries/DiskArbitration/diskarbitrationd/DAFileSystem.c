/*
 * Copyright (c) 1998-2015 Apple Inc. All rights reserved.
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

#include "DAFileSystem.h"

#include "DABase.h"
#include "DACommand.h"
#include "DAInternal.h"

#include <fsproperties.h>
#include <paths.h>
#include <unistd.h>
#include <FSPrivate.h>
#include <sys/attr.h>
#include <sys/loadable_fs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>

#define __kDAFileSystemUUIDSpaceSHA1 CFUUIDGetConstantUUIDWithBytes( kCFAllocatorDefault,               \
                                                                     0xB3, 0xE2, 0x0F, 0x39,            \
                                                                     0xF2, 0x92,                        \
                                                                     0x11, 0xD6,                        \
                                                                     0x97, 0xA4,                        \
                                                                     0x00, 0x30, 0x65, 0x43, 0xEC, 0xAC )

struct __DAFileSystem
{
    CFRuntimeBase   _base;
    CFURLRef        _id;
    CFDictionaryRef _properties;
};

typedef struct __DAFileSystem __DAFileSystem;

struct __DAFileSystemContext
{
    DAFileSystemCallback callback;
    void *               callbackContext;
};

typedef struct __DAFileSystemContext __DAFileSystemContext;

struct __DAFileSystemProbeContext
{
    DAFileSystemProbeCallback callback;
    void *                    callbackContext;
    CFStringRef               deviceName;
    CFStringRef               devicePath;
    CFURLRef                  probeCommand;
    CFURLRef                  repairCommand;
    CFBooleanRef              volumeClean;
    CFStringRef               volumeName;
    CFStringRef               volumeType;
    CFUUIDRef                 volumeUUID;
};

typedef struct __DAFileSystemProbeContext __DAFileSystemProbeContext;

struct __DAFileSystemRenameBuffer
{
    attrreference_t data;
    char            name[MAXPATHLEN];
};

typedef struct __DAFileSystemRenameBuffer __DAFileSystemRenameBuffer;

static CFStringRef __DAFileSystemCopyDescription( CFTypeRef object );
static CFStringRef __DAFileSystemCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options );
static void        __DAFileSystemDeallocate( CFTypeRef object );
static Boolean     __DAFileSystemEqual( CFTypeRef object1, CFTypeRef object2 );
static CFHashCode  __DAFileSystemHash( CFTypeRef object );

static const CFRuntimeClass __DAFileSystemClass =
{
    0,
    "DAFileSystem",
    NULL,
    NULL,
    __DAFileSystemDeallocate,
    __DAFileSystemEqual,
    __DAFileSystemHash,
    __DAFileSystemCopyFormattingDescription,
    __DAFileSystemCopyDescription
};

static CFTypeID __kDAFileSystemTypeID = _kCFRuntimeNotATypeID;

const CFStringRef kDAFileSystemMountArgumentForce       = CFSTR( "force"    );
const CFStringRef kDAFileSystemMountArgumentNoDevice    = CFSTR( "nodev"    );
const CFStringRef kDAFileSystemMountArgumentNoExecute   = CFSTR( "noexec"   );
const CFStringRef kDAFileSystemMountArgumentNoOwnership = CFSTR( "noowners" );
const CFStringRef kDAFileSystemMountArgumentNoSetUserID = CFSTR( "nosuid"   );
const CFStringRef kDAFileSystemMountArgumentNoWrite     = CFSTR( "rdonly"   );
const CFStringRef kDAFileSystemMountArgumentUnion       = CFSTR( "union"    );
const CFStringRef kDAFileSystemMountArgumentUpdate      = CFSTR( "update"   );

const CFStringRef kDAFileSystemUnmountArgumentForce     = CFSTR( "force" );

static void __DAFileSystemProbeCallbackStage1( int status, CFDataRef output, void * context );
static void __DAFileSystemProbeCallbackStage2( int status, CFDataRef output, void * context );
static void __DAFileSystemProbeCallbackStage3( int status, CFDataRef output, void * context );

static void __DAFileSystemCallback( int status, CFDataRef output, void * parameter )
{
    /*
     * Process the probe request completion.
     */

    __DAFileSystemContext * context = parameter;

    if ( context->callback )
    {
        ( context->callback )( status, context->callbackContext );
    }

    free( context );
}

static CFStringRef __DAFileSystemCopyDescription( CFTypeRef object )
{
    DAFileSystemRef filesystem = ( DAFileSystemRef ) object;

    return CFStringCreateWithFormat( kCFAllocatorDefault,
                                     NULL,
                                     CFSTR( "<DAFileSystem %p [%p]>{id = %@}" ),
                                     object,
                                     CFGetAllocator( object ),
                                     filesystem->_id );
}

static CFStringRef __DAFileSystemCopyFormattingDescription( CFTypeRef object, CFDictionaryRef options )
{
    DAFileSystemRef filesystem = ( DAFileSystemRef ) object;

    return CFStringCreateWithFormat( kCFAllocatorDefault, NULL, CFSTR( "%@" ), filesystem->_id );
}

static DAFileSystemRef __DAFileSystemCreate( CFAllocatorRef allocator, CFURLRef id, CFDictionaryRef properties )
{
    __DAFileSystem * filesystem;

    filesystem = ( void * ) _CFRuntimeCreateInstance( allocator, __kDAFileSystemTypeID, sizeof( __DAFileSystem ) - sizeof( CFRuntimeBase ), NULL );

    if ( filesystem )
    {
        filesystem->_id         = CFRetain( id );
        filesystem->_properties = CFRetain( properties );
    }

    return filesystem;
}

static void __DAFileSystemDeallocate( CFTypeRef object )
{
    DAFileSystemRef filesystem = ( DAFileSystemRef ) object;

    if ( filesystem->_id         )  CFRelease( filesystem->_id         );
    if ( filesystem->_properties )  CFRelease( filesystem->_properties );
}

static Boolean __DAFileSystemEqual( CFTypeRef object1, CFTypeRef object2 )
{
    DAFileSystemRef filesystem1 = ( DAFileSystemRef ) object1;
    DAFileSystemRef filesystem2 = ( DAFileSystemRef ) object2;

    return CFEqual( filesystem1->_id, filesystem2->_id );
}

static CFHashCode __DAFileSystemHash( CFTypeRef object )
{
    DAFileSystemRef filesystem = ( DAFileSystemRef ) object;

    return CFHash( filesystem->_id );
}

static void __DAFileSystemProbeCallback( int status, void * parameter, CFDataRef output )
{
    /*
     * Process the probe request completion.
     */

    __DAFileSystemProbeContext * context = parameter;

    if ( context->callback )
    {
        if ( status )
        {
            ( context->callback )( status, NULL, NULL, NULL, NULL, context->callbackContext );
        }
        else
        {
            ( context->callback )( status, context->volumeClean, context->volumeName, context->volumeType, context->volumeUUID, context->callbackContext );
        }
    }

    CFRelease( context->deviceName   );
    CFRelease( context->devicePath   );
    CFRelease( context->probeCommand );

    if ( context->repairCommand )  CFRelease( context->repairCommand );
    if ( context->volumeClean   )  CFRelease( context->volumeClean   );
    if ( context->volumeName    )  CFRelease( context->volumeName    );
    if ( context->volumeType    )  CFRelease( context->volumeType    );
    if ( context->volumeUUID    )  CFRelease( context->volumeUUID    );

    free( context );
}

static void __DAFileSystemProbeCallbackStage1( int status, CFDataRef output, void * parameter )
{
    /*
     * Process the probe command's completion.
     */

    __DAFileSystemProbeContext * context = parameter;

    if ( status == FSUR_RECOGNIZED )
    {
        /*
         * Obtain the volume name.
         */

        if ( output )
        {
            CFStringRef string;

            string = CFStringCreateFromExternalRepresentation( kCFAllocatorDefault, output, kCFStringEncodingUTF8 );

            if ( string )
            {
                if ( CFStringGetLength( string ) )
                {
                    context->volumeName = CFStringCreateMutableCopy( kCFAllocatorDefault, 0, string );

                    if ( context->volumeName )
                    {
                        CFStringTrim( ( CFMutableStringRef ) context->volumeName, CFSTR( "\n" ) );
                    }
                }

                CFRelease( string );
            }
        }

        /*
         * Execute the "get UUID" command.
         */

        DACommandExecute( context->probeCommand,
                          kDACommandExecuteOptionCaptureOutput,
                          ___UID_ROOT,
                          ___GID_WHEEL,
                          __DAFileSystemProbeCallbackStage2,
                          context,
                          CFSTR( "-k" ),
                          context->deviceName,
                          NULL );
    }
    else
    {
        __DAFileSystemProbeCallback( status, context, NULL );
    }
}

static void __DAFileSystemProbeCallbackStage2( int status, CFDataRef output, void * parameter )
{
    /*
     * Process the "get UUID" command's completion.
     */

    __DAFileSystemProbeContext * context = parameter;

    if ( status == FSUR_IO_SUCCESS )
    {
        /*
         * Obtain the volume UUID.  The "get UUID" command returns a unique 64-bit number, which
         * we must map into the official, structured 128-bit UUID format.  One would expect that
         * the "get UUID" interface return the official UUID format, when it is revised later on,
         * and we take that into account here.
         */

        if ( output )
        {
            CFStringRef string;

            string = CFStringCreateFromExternalRepresentation( kCFAllocatorDefault, output, kCFStringEncodingUTF8 );

            if ( string )
            {
                context->volumeUUID = ___CFUUIDCreateFromString( kCFAllocatorDefault, string );

                CFRelease( string );
            }
        }
    }

    if ( context->repairCommand )
    {
        /*
         * Execute the "is clean" command.
         */

        DACommandExecute( context->repairCommand,
                          kDACommandExecuteOptionDefault,
                          ___UID_ROOT,
                          ___GID_WHEEL,
                          __DAFileSystemProbeCallbackStage3,
                          context,
                          CFSTR( "-q" ),
                          context->devicePath,
                          NULL );
    }
    else
    {
        /*
         * Skip the "is clean" command, as it is not applicable.
         */

        __DAFileSystemProbeCallbackStage3( 0, NULL, context );
    }
}

static void __DAFileSystemProbeCallbackStage3( int status, CFDataRef output, void * parameter )
{
    /*
     * Process the "is clean" command's completion.
     */

    __DAFileSystemProbeContext * context = parameter;

    context->volumeClean = CFRetain( ( status == 0 ) ? kCFBooleanTrue : kCFBooleanFalse );

    context->volumeType = _FSCopyNameForVolumeFormatAtNode( context->devicePath );

    __DAFileSystemProbeCallback( 0, context, NULL );
}

CFStringRef _DAFileSystemCopyName( DAFileSystemRef filesystem, CFURLRef mountpoint )
{
    struct attr_name_t
    {
        uint32_t        size;
        attrreference_t data;
        char            name[MAXPATHLEN];
    };

    struct attr_name_t attr     = { 0 };
    struct attrlist    attrlist = { 0 };
    CFStringRef        name     = NULL;
    char *             path     = NULL;
    int                status   = 0;

    attrlist.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrlist.volattr     = ATTR_VOL_INFO | ATTR_VOL_NAME;

    path = ___CFURLCopyFileSystemRepresentation( mountpoint );
    if ( path == NULL )  goto _DAFileSystemCopyNameErr;

    status = getattrlist( path, &attrlist, &attr, sizeof( attr ), 0 );
    if ( status == -1 )  goto _DAFileSystemCopyNameErr;

    if ( attr.data.attr_length )
    {
        name = CFStringCreateWithCString( kCFAllocatorDefault, ( ( char * ) &attr.data ) + attr.data.attr_dataoffset, kCFStringEncodingUTF8 );
    }

_DAFileSystemCopyNameErr:

    if ( path )  free( path );

    return name;
}

CFUUIDRef _DAFileSystemCreateUUIDFromString( CFAllocatorRef allocator, CFStringRef string )
{
    CFDataRef data;
    CFUUIDRef uuid;

    uuid = ___CFUUIDCreateFromString( allocator, string );

    if ( uuid == NULL )
    {
        data = ___CFDataCreateFromString( allocator, string );

        if ( data )
        {
            if ( CFDataGetLength( data ) == 8 )
            {
                if ( *( ( UInt64 * ) CFDataGetBytePtr( data ) ) )
                {
                    uuid = ___CFUUIDCreateFromName( allocator, __kDAFileSystemUUIDSpaceSHA1, data );
                }
                else
                {
                    uuid = CFRetain( ___kCFUUIDNull );
                }
            }

            CFRelease( data );
        }
    }

    return uuid;
}

DAFileSystemRef DAFileSystemCreate( CFAllocatorRef allocator, CFURLRef path )
{
    DAFileSystemRef filesystem = NULL;
    CFDictionaryRef properties;

    /*
     * Obtain the file system properties.
     */

    properties = CFBundleCopyInfoDictionaryInDirectory( path );

    if ( properties )
    {
        CFURLRef id;

        /*
         * Create the file system object's unique identifier.
         */

        id = CFURLCopyAbsoluteURL( path );

        if ( id )
        {
            /*
             * Create the file system object.
             */

            filesystem = __DAFileSystemCreate( allocator, id, properties );

            CFRelease( id );
        }

        CFRelease( properties );
    }

    return filesystem;
}

CFRunLoopSourceRef DAFileSystemCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order )
{
    return DACommandCreateRunLoopSource( allocator, order );
}

CFStringRef DAFileSystemGetKind( DAFileSystemRef filesystem )
{
    return CFDictionaryGetValue( filesystem->_properties, kCFBundleNameKey );
}

CFDictionaryRef DAFileSystemGetProbeList( DAFileSystemRef filesystem )
{
    return CFDictionaryGetValue( filesystem->_properties, CFSTR( kFSMediaTypesKey ) );
}

CFTypeID DAFileSystemGetTypeID( void )
{
    return __kDAFileSystemTypeID;
}

void DAFileSystemInitialize( void )
{
    __kDAFileSystemTypeID = _CFRuntimeRegisterClass( &__DAFileSystemClass );
}

void DAFileSystemMount( DAFileSystemRef      filesystem,
                        CFURLRef             device,
                        CFURLRef             mountpoint,
                        uid_t                userUID,
                        gid_t                userGID,
                        DAFileSystemCallback callback,
                        void *               callbackContext )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.
     */

    DAFileSystemMountWithArguments( filesystem, device, mountpoint, userUID, userGID, callback, callbackContext, NULL );
}

void DAFileSystemMountWithArguments( DAFileSystemRef      filesystem,
                                     CFURLRef             device,
                                     CFURLRef             mountpoint,
                                     uid_t                userUID,
                                     gid_t                userGID,
                                     DAFileSystemCallback callback,
                                     void *               callbackContext,
                                     ... )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.  All arguments in
     * the argument list shall be of type CFStringRef.  The argument list must be NULL
     * terminated.
     */

    CFStringRef             argument       = NULL;
    va_list                 arguments;
    CFURLRef                command        = NULL;
    __DAFileSystemContext * context        = NULL;
    CFStringRef             devicePath     = NULL;
    CFStringRef             mountpointPath = NULL;
    CFMutableStringRef      options        = NULL;
    int                     status         = 0;

    /*
     * Prepare to mount the volume.
     */

    command = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR( "/sbin/mount" ), kCFURLPOSIXPathStyle, FALSE );
    if ( command == NULL )  { status = ENOTSUP; goto DAFileSystemMountErr; }

    context = malloc( sizeof( __DAFileSystemContext ) );
    if ( context == NULL )  { status = ENOMEM; goto DAFileSystemMountErr; }

    devicePath = CFURLCopyFileSystemPath( device, kCFURLPOSIXPathStyle );
    if ( devicePath == NULL )  { status = EINVAL; goto DAFileSystemMountErr; }

    mountpointPath = CFURLCopyFileSystemPath( mountpoint, kCFURLPOSIXPathStyle );
    if ( mountpointPath == NULL )  { status = EINVAL; goto DAFileSystemMountErr; }

    options = CFStringCreateMutable( kCFAllocatorDefault, 0 );
    if ( options == NULL )  { status = ENOMEM; goto DAFileSystemMountErr; }

    /*
     * Prepare the mount options.
     */

    va_start( arguments, callbackContext );

    while ( ( argument = va_arg( arguments, CFStringRef ) ) )
    {
        CFStringAppend( options, argument );
        CFStringAppend( options, CFSTR( "," ) );
    }

    va_end( arguments );

    CFStringTrim( options, CFSTR( "," ) );

    /*
     * Execute the mount command.
     */

    context->callback        = callback;
    context->callbackContext = callbackContext;

    if ( CFStringGetLength( options ) )
    {
        DACommandExecute( command,
                          kDACommandExecuteOptionDefault,
                          userUID,
                          userGID,
                          __DAFileSystemCallback,
                          context,
                          CFSTR( "-t" ),
                          DAFileSystemGetKind( filesystem ),
                          CFSTR( "-o" ),
                          options,
                          devicePath,
                          mountpointPath,
                          NULL );
    }
    else
    {
        DACommandExecute( command,
                          kDACommandExecuteOptionDefault,
                          userUID,
                          userGID,
                          __DAFileSystemCallback,
                          context,
                          CFSTR( "-t" ),
                          DAFileSystemGetKind( filesystem ),
                          devicePath,
                          mountpointPath,
                          NULL );
    }

DAFileSystemMountErr:

    if ( command        )  CFRelease( command        );
    if ( devicePath     )  CFRelease( devicePath     );
    if ( mountpointPath )  CFRelease( mountpointPath );
    if ( options        )  CFRelease( options        );

    if ( status )
    {
        if ( context )  free( context );

        if ( callback )
        {
            ( callback )( status, callbackContext );
        }
    }
}

void DAFileSystemProbe( DAFileSystemRef           filesystem,
                        CFURLRef                  device,
                        DAFileSystemProbeCallback callback,
                        void *                    callbackContext )
{
    /*
     * Probe the specified volume.  A status of 0 indicates success.
     */

    __DAFileSystemProbeContext * context           = NULL;
    CFStringRef                  deviceName        = NULL;
    CFStringRef                  devicePath        = NULL;
    CFDictionaryRef              mediaType         = NULL;
    CFDictionaryRef              mediaTypes        = NULL;
    CFDictionaryRef              personality       = NULL;
    CFDictionaryRef              personalities     = NULL;
    CFURLRef                     probeCommand      = NULL;
    CFStringRef                  probeCommandName  = NULL;
    CFURLRef                     repairCommand     = NULL;
    CFStringRef                  repairCommandName = NULL;
    int                          status            = 0;

    /*
     * Prepare to probe.
     */

    personalities = CFDictionaryGetValue( filesystem->_properties, CFSTR( kFSPersonalitiesKey ) );
    if ( personalities == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }

    personality = ___CFDictionaryGetAnyValue( personalities );
    if ( personality == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }

    mediaTypes = CFDictionaryGetValue( filesystem->_properties, CFSTR( kFSMediaTypesKey ) );
    if ( mediaTypes == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }

    mediaType = ___CFDictionaryGetAnyValue( mediaTypes );
    if ( mediaType == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }

    probeCommandName = CFDictionaryGetValue( mediaType, CFSTR( kFSProbeExecutableKey ) );
    if ( probeCommandName == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }

    probeCommand = ___CFBundleCopyResourceURLInDirectory( filesystem->_id, probeCommandName );
    if ( probeCommand == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }

    repairCommandName = CFDictionaryGetValue( personality, CFSTR( kFSRepairExecutableKey ) );

    if ( repairCommandName )
    {
        repairCommand = ___CFBundleCopyResourceURLInDirectory( filesystem->_id, repairCommandName );
        if ( repairCommand == NULL )  { status = ENOTSUP; goto DAFileSystemProbeErr; }
    }

    deviceName = CFURLCopyLastPathComponent( device );
    if ( deviceName == NULL )  { status = EINVAL; goto DAFileSystemProbeErr; }

    devicePath = ___CFURLCopyRawDeviceFileSystemPath( device, kCFURLPOSIXPathStyle );
    if ( devicePath == NULL )  { status = EINVAL; goto DAFileSystemProbeErr; }

    context = malloc( sizeof( __DAFileSystemProbeContext ) );
    if ( context == NULL )  { status = ENOMEM; goto DAFileSystemProbeErr; }

    /*
     * Execute the probe command.
     */

    context->callback        = callback;
    context->callbackContext = callbackContext;
    context->deviceName      = deviceName;
    context->devicePath      = devicePath;
    context->probeCommand    = probeCommand;
    context->repairCommand   = repairCommand;
    context->volumeClean     = NULL;
    context->volumeName      = NULL;
    context->volumeType      = NULL;
    context->volumeUUID      = NULL;

    DACommandExecute( probeCommand,
                      kDACommandExecuteOptionCaptureOutput,
                      ___UID_ROOT,
                      ___GID_WHEEL,
                      __DAFileSystemProbeCallbackStage1,
                      context,
                      CFSTR( "-p" ),
                      deviceName,
                      CFSTR( "removable" ),
                      CFSTR( "readonly"  ),
                      NULL );

DAFileSystemProbeErr:

    if ( status )
    {
        if ( deviceName    )  CFRelease( deviceName    );
        if ( devicePath    )  CFRelease( devicePath    );
        if ( probeCommand  )  CFRelease( probeCommand  );
        if ( repairCommand )  CFRelease( repairCommand );

        if ( context )  free( context );

        if ( callback )
        {
            ( callback )( status, NULL, NULL, NULL, NULL, callbackContext );
        }
    }
}

void DAFileSystemRename( DAFileSystemRef      filesystem,
                         CFURLRef             mountpoint,
                         CFStringRef          name,
                         DAFileSystemCallback callback,
                         void *               callbackContext )
{
    /*
     * Rename the specified volume.  A status of 0 indicates success.
     */

    struct attrlist              attributes     = { 0 };
    __DAFileSystemRenameBuffer * buffer         = NULL;
    char *                       mountpointPath = NULL;
    int                          status         = 0;

    /*
     * Prepare to set the name.
     */

    attributes.bitmapcount = ATTR_BIT_MAP_COUNT;
    attributes.commonattr  = 0;
    attributes.dirattr     = 0;
    attributes.fileattr    = 0;
    attributes.forkattr    = 0;
    attributes.volattr     = ATTR_VOL_INFO | ATTR_VOL_NAME;

    buffer = malloc( sizeof( __DAFileSystemRenameBuffer ) );
    if ( buffer == NULL )  { status = ENOMEM; goto DAFileSystemRenameErr; }

    mountpointPath = ___CFURLCopyFileSystemRepresentation( mountpoint );
    if ( mountpointPath == NULL )  { status = EINVAL; goto DAFileSystemRenameErr; }

    /*
     * Set the name.
     */

    status = CFStringGetCString( name, buffer->name, sizeof( buffer->name ), kCFStringEncodingUTF8 );
    if ( status == FALSE )  { status = EINVAL; goto DAFileSystemRenameErr; }

    buffer->data.attr_dataoffset = sizeof( buffer->data );
    buffer->data.attr_length     = strlen( buffer->name ) + 1;

    status = setattrlist( mountpointPath, &attributes, buffer, sizeof( __DAFileSystemRenameBuffer ), 0 );
    if ( status == -1 )  { status = errno; goto DAFileSystemRenameErr; }

DAFileSystemRenameErr:

    if ( buffer         )  free( buffer );
    if ( mountpointPath )  free( mountpointPath );

    if ( callback )
    {
        ( callback )( status, callbackContext );
    }
}

void DAFileSystemRepair( DAFileSystemRef      filesystem,
                         CFURLRef             device,
                         DAFileSystemCallback callback,
                         void *               callbackContext )
{
    /*
     * Repair the specified volume.  A status of 0 indicates success.
     */

    CFURLRef                command       = NULL;
    CFStringRef             commandName   = NULL;
    __DAFileSystemContext * context       = NULL;
    CFStringRef             devicePath    = NULL;
    CFDictionaryRef         personality   = NULL;
    CFDictionaryRef         personalities = NULL;
    int                     status        = 0;

    /*
     * Prepare to repair.
     */

    personalities = CFDictionaryGetValue( filesystem->_properties, CFSTR( kFSPersonalitiesKey ) );
    if ( personalities == NULL )  { status = ENOTSUP; goto DAFileSystemRepairErr; }

    personality = ___CFDictionaryGetAnyValue( personalities );
    if ( personality == NULL )  { status = ENOTSUP; goto DAFileSystemRepairErr; }

    commandName = CFDictionaryGetValue( personality, CFSTR( kFSRepairExecutableKey ) );
    if ( commandName == NULL )  { status = ENOTSUP; goto DAFileSystemRepairErr; }

    command = ___CFBundleCopyResourceURLInDirectory( filesystem->_id, commandName );
    if ( command == NULL )  { status = ENOTSUP; goto DAFileSystemRepairErr; }

    devicePath = ___CFURLCopyRawDeviceFileSystemPath( device, kCFURLPOSIXPathStyle );
    if ( devicePath == NULL )  { status = EINVAL; goto DAFileSystemRepairErr; }

    context = malloc( sizeof( __DAFileSystemContext ) );
    if ( context == NULL )  { status = ENOMEM; goto DAFileSystemRepairErr; }

    /*
     * Execute the repair command.
     */

    context->callback        = callback;
    context->callbackContext = callbackContext;

    DACommandExecute( command,
                      kDACommandExecuteOptionDefault,
                      ___UID_ROOT,
                      ___GID_WHEEL,
                      __DAFileSystemCallback,
                      context,
                      CFSTR( "-y" ),
                      devicePath,
                      NULL );

DAFileSystemRepairErr:

    if ( command    )  CFRelease( command    );
    if ( devicePath )  CFRelease( devicePath );

    if ( status )
    {
        if ( context )  free( context );

        if ( callback )
        {
            ( callback )( status, callbackContext );
        }
    }
}

void DAFileSystemRepairQuotas( DAFileSystemRef      filesystem,
                               CFURLRef             mountpoint,
                               DAFileSystemCallback callback,
                               void *               callbackContext )
{
    /*
     * Repair the quotas on specified volume.  A status of 0 indicates success.
     */

    CFURLRef                command        = NULL;
    __DAFileSystemContext * context        = NULL;
    CFStringRef             mountpointPath = NULL;
    int                     status         = 0;

    /*
     * Prepare to repair quotas.
     */

    command = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR( "/sbin/quotacheck" ), kCFURLPOSIXPathStyle, FALSE );
    if ( command == NULL )  { status = ENOTSUP; goto DAFileSystemRepairQuotasErr; }

    context = malloc( sizeof( __DAFileSystemContext ) );
    if ( context == NULL )  { status = ENOMEM; goto DAFileSystemRepairQuotasErr; }

    mountpointPath = CFURLCopyFileSystemPath( mountpoint, kCFURLPOSIXPathStyle );
    if ( mountpointPath == NULL )  { status = EINVAL; goto DAFileSystemRepairQuotasErr; }

    /*
     * Execute the repair quotas command.
     */

    context->callback        = callback;
    context->callbackContext = callbackContext;

    DACommandExecute( command,
                      kDACommandExecuteOptionDefault,
                      ___UID_ROOT,
                      ___GID_WHEEL,
                      __DAFileSystemCallback,
                      context,
                      CFSTR( "-g" ),
                      CFSTR( "-u" ),
                      mountpointPath,
                      NULL );

DAFileSystemRepairQuotasErr:

    if ( command        )  CFRelease( command        );
    if ( mountpointPath )  CFRelease( mountpointPath );

    if ( status )
    {
        if ( context )  free( context );

        if ( callback )
        {
            ( callback )( status, callbackContext );
        }
    }
}

void DAFileSystemUnmount( DAFileSystemRef      filesystem,
                          CFURLRef             mountpoint,
                          DAFileSystemCallback callback,
                          void *               callbackContext )
{
    DAFileSystemUnmountWithArguments( filesystem, mountpoint, callback, callbackContext, NULL );
}

void DAFileSystemUnmountWithArguments( DAFileSystemRef      filesystem,
                                       CFURLRef             mountpoint,
                                       DAFileSystemCallback callback,
                                       void *               callbackContext,
                                       ... )
{
    /*
     * Unmount the specified volume.  A status of 0 indicates success.  All arguments in the
     * argument list must be of type CFStringRef.  The argument list must be NULL terminated.
     */

    CFStringRef             argument       = NULL;
    va_list                 arguments;
    CFURLRef                command        = NULL;
    __DAFileSystemContext * context        = NULL;
    CFStringRef             mountpointPath = NULL;
    int                     options        = 0;
    int                     status         = 0;

    /*
     * Prepare to unmount the volume.
     */

    command = CFURLCreateWithFileSystemPath( kCFAllocatorDefault, CFSTR( "/sbin/umount" ), kCFURLPOSIXPathStyle, FALSE );
    if ( command == NULL )  { status = ENOTSUP; goto DAFileSystemUnmountErr; }

    context = malloc( sizeof( __DAFileSystemContext ) );
    if ( context == NULL )  { status = ENOMEM; goto DAFileSystemUnmountErr; }

    mountpointPath = CFURLCopyFileSystemPath( mountpoint, kCFURLPOSIXPathStyle );
    if ( mountpointPath == NULL )  { status = EINVAL; goto DAFileSystemUnmountErr; }

    /*
     * Prepare the unmount options.
     */

    va_start( arguments, callbackContext );

    while ( ( argument = va_arg( arguments, CFStringRef ) ) )
    {
        if ( CFEqual( argument, kDAFileSystemUnmountArgumentForce ) )
        {
            options |= MNT_FORCE;
        }
    }

    va_end( arguments );

    /*
     * Execute the unmount command.
     */

    context->callback        = callback;
    context->callbackContext = callbackContext;

    if ( ( options & MNT_FORCE ) )
    {
        DACommandExecute( command,
                          kDACommandExecuteOptionDefault,
                          ___UID_ROOT,
                          ___GID_WHEEL,
                          __DAFileSystemCallback,
                          context,
                          CFSTR( "-f" ),
                          mountpointPath,
                          NULL );
    }
    else
    {
        DACommandExecute( command,
                          kDACommandExecuteOptionDefault,
                          ___UID_ROOT,
                          ___GID_WHEEL,
                          __DAFileSystemCallback,
                          context,
                          mountpointPath,
                          NULL );
    }

DAFileSystemUnmountErr:

    if ( command        )  CFRelease( command        );
    if ( mountpointPath )  CFRelease( mountpointPath );

    if ( status )
    {
        if ( context )  free( context );

        if ( callback )
        {
            ( callback )( status, callbackContext );
        }
    }
}
