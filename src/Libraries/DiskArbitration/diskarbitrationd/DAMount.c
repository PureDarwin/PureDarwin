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

#include "DAMount.h"

#include "DABase.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DASupport.h"

#include <fstab.h>
#include <sys/stat.h>
#include <IOKit/pwr_mgt/IOPMLib.h>

struct __DAMountCallbackContext
{
///w:start
    Boolean         automatic;
///w:stop
    IOPMAssertionID assertionID;
    DAMountCallback callback;
    void *          callbackContext;
    DADiskRef       disk;
    Boolean         force;
    CFURLRef        mountpoint;
    CFStringRef     options;
};

typedef struct __DAMountCallbackContext __DAMountCallbackContext;

static void __DAMountWithArgumentsCallbackStage1( int status, void * context );
static void __DAMountWithArgumentsCallbackStage2( int status, void * context );
static void __DAMountWithArgumentsCallbackStage3( int status, void * context );

static void __DAMountWithArgumentsCallback( int status, void * parameter )
{
    /*
     * Process the mount request completion.
     */

    __DAMountCallbackContext * context = parameter;

///w:start
    if ( context->automatic )
    {
        if ( status == ___EDIRTY )
        {
            DAMountWithArguments( context->disk, NULL, context->callback, context->callbackContext, kDAFileSystemMountArgumentForce, kDAFileSystemMountArgumentNoWrite, NULL );

            context->callback = NULL;
        }
    }
///w:stop
    if ( context->callback )
    {
        ( context->callback )( status, context->mountpoint, context->callbackContext );
    }

    CFRelease( context->disk    );
    CFRelease( context->options );

    if ( context->mountpoint )  CFRelease( context->mountpoint );

    free( context );
}

static void __DAMountWithArgumentsCallbackStage1( int status, void * parameter )
{
    /*
     * Process the repair command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( context->assertionID != kIOPMNullAssertionID )
    {
        IOPMAssertionRelease( context->assertionID );

        context->assertionID = kIOPMNullAssertionID;
    }

    if ( status )
    {
        /*
         * We were unable to repair the volume.
         */

        if ( status == ECANCELED )
        {
            status = 0;
        }
        else
        {
            DALogDebug( "  repaired disk, id = %@, failure.", context->disk );

            DALogError( "unable to repair %@ (status code 0x%08X).", context->disk, status );

            if ( context->force )
            {
                status = 0;
            }
            else
            {
                __DAMountWithArgumentsCallback( ___EDIRTY, context );
            }
        }
    }
    else
    {
        /*
         * We were able to repair the volume.
         */

        DADiskSetState( context->disk, kDADiskStateRequireRepair, FALSE );

        DALogDebug( "  repaired disk, id = %@, success.", context->disk );
    }

    /*
     * Mount the volume.
     */

    if ( status == 0 )
    {
        /*
         * Create the mount point, in case one needs to be created.
         */

        if ( context->mountpoint == NULL )
        {
            context->mountpoint = DAMountCreateMountPointWithAction( context->disk, kDAMountPointActionMake );
        }

        /*
         * Execute the mount command.
         */

        if ( context->mountpoint )
        {
            DALogDebug( "  mounted disk, id = %@, ongoing.", context->disk );

            DAFileSystemMountWithArguments( DADiskGetFileSystem( context->disk ),
                                            DADiskGetDevice( context->disk ),
                                            context->mountpoint,
                                            DADiskGetUserUID( context->disk ),
                                            DADiskGetUserGID( context->disk ),
                                            __DAMountWithArgumentsCallbackStage2,
                                            context,
                                            context->options,
                                            NULL );
        }
        else
        {
            __DAMountWithArgumentsCallback( ENOSPC, context );
        }
    }
}

static void __DAMountWithArgumentsCallbackStage2( int status, void * parameter )
{
    /*
     * Process the mount command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( status )
    {
        /*
         * We were unable to mount the volume.
         */

        DALogDebug( "  mounted disk, id = %@, failure.", context->disk );

        DALogError( "unable to mount %@ (status code 0x%08X).", context->disk, status );

        DAMountRemoveMountPoint( context->mountpoint );

        __DAMountWithArgumentsCallback( status, context );
    }
    else
    {
        /*
         * We were able to mount the volume.
         */

        DALogDebug( "  mounted disk, id = %@, success.", context->disk );

        /*
         * Execute the "repair quotas" command.
         */

        if ( DADiskGetState( context->disk, kDADiskStateRequireRepairQuotas ) )
        {
            DAFileSystemRepairQuotas( DADiskGetFileSystem( context->disk ),
                                      context->mountpoint,
                                      __DAMountWithArgumentsCallbackStage3,
                                      context );
        }
        else
        {
            __DAMountWithArgumentsCallbackStage3( 0, context );
        }
    }
}

static void __DAMountWithArgumentsCallbackStage3( int status, void * parameter )
{
    /*
     * Process the "repair quotas" command's completion.
     */

    __DAMountCallbackContext * context = parameter;

    if ( status )
    {
        DALogError( "unable to repair quotas on disk %@ (status code 0x%08X).", context->disk, status );
    }
    else
    {
        DADiskSetState( context->disk, kDADiskStateRequireRepairQuotas, FALSE );
    }

    __DAMountWithArgumentsCallback( 0, context );
}

void DAMount( DADiskRef disk, CFURLRef mountpoint, DAMountCallback callback, void * callbackContext )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.
     */

    return DAMountWithArguments( disk, mountpoint, callback, callbackContext, NULL );

}

Boolean DAMountContainsArgument( CFStringRef arguments, CFStringRef argument )
{
    CFBooleanRef argumentValue;
    CFBooleanRef argumentsValue;

    argumentsValue = NULL;

    if ( CFStringHasPrefix( argument, CFSTR( "no" ) ) )
    {
        argument      = CFStringCreateWithSubstring( kCFAllocatorDefault, argument, CFRangeMake( 2, CFStringGetLength( argument ) - 2 ) );
        argumentValue = kCFBooleanFalse;
    }
    else
    {
        argument      = CFRetain( argument );
        argumentValue = kCFBooleanTrue;
    }

    if ( argument )
    {
        CFArrayRef argumentList;
        CFIndex    argumentListCount;
        CFIndex    argumentListIndex;

        argumentList = CFStringCreateArrayBySeparatingStrings( kCFAllocatorDefault, arguments, CFSTR( "," ) );

        if ( argumentList )
        {
            argumentListCount = CFArrayGetCount( argumentList );

            for ( argumentListIndex = 0; argumentListIndex < argumentListCount; argumentListIndex++ )
            {
                CFStringRef compare;

                compare = CFArrayGetValueAtIndex( argumentList, argumentListIndex );

                if ( compare )
                {
                    CFBooleanRef compareValue;

                    if ( CFStringHasPrefix( compare, CFSTR( "no" ) ) )
                    {
                        compare      = CFStringCreateWithSubstring( kCFAllocatorDefault, compare, CFRangeMake( 2, CFStringGetLength( compare ) - 2 ) );
                        compareValue = kCFBooleanFalse;
                    }
                    else
                    {
                        compare      = CFRetain( compare );
                        compareValue = kCFBooleanTrue;
                    }

                    if ( compare )
                    {
                        if ( CFEqual( compare, CFSTR( FSTAB_RO ) ) )
                        {
                            CFRelease( compare );

                            compare      = CFRetain( kDAFileSystemMountArgumentNoWrite );
                            compareValue = compareValue;
                        }

                        if ( CFEqual( compare, CFSTR( FSTAB_RW ) ) )
                        {
                            CFRelease( compare );

                            compare      = CFRetain( kDAFileSystemMountArgumentNoWrite );
                            compareValue = ( compareValue == kCFBooleanTrue ) ? kCFBooleanFalse : kCFBooleanTrue;
                        }
                    }

                    if ( compare )
                    {
                        if ( CFEqual( argument, compare ) )
                        {
                            argumentsValue = compareValue;
                        }

                        CFRelease( compare );
                    }
                }
            }

            CFRelease( argumentList );
        }

        CFRelease( argument );
    }

    return ( argumentValue == argumentsValue ) ? TRUE : FALSE;
}

CFURLRef DAMountCreateMountPoint( DADiskRef disk )
{
    return DAMountCreateMountPointWithAction( disk, kDAMountPointActionMake );
}

CFURLRef DAMountCreateMountPointWithAction( DADiskRef disk, DAMountPointAction action )
{
    CFIndex     index;
    CFURLRef    mountpoint;
    char        name[MAXPATHLEN];
    char        path[MAXPATHLEN];
    CFStringRef string;

    mountpoint = NULL;

    /*
     * Obtain the volume name.
     */

    string = DADiskGetDescription( disk, kDADiskDescriptionVolumeNameKey );

    if ( string )
    {
        if ( CFStringGetLength( string ) )
        {
            CFRetain( string );
        }
        else
        {
            string = NULL;
        }
    }

    if ( string == NULL )
    {
        string = ___CFBundleCopyLocalizedStringInDirectory( gDABundlePath, CFSTR( "Untitled" ), CFSTR( "Untitled" ), NULL );
    }

    if ( ___CFStringGetCString( string, name, MNAMELEN - 20 ) )
    {
        /*
         * Adjust the volume name.
         */

        while ( strchr( name, '/' ) )
        {
            *strchr( name, '/' ) = ':';
        }

        /*
         * Create the mount point path.
         */

        for ( index = 0; index < 100; index++ )
        {
            if ( index == 0 )
            {
                snprintf( path, sizeof( path ), "%s/%s", kDAMainMountPointFolder, name );
            }
            else
            {
                snprintf( path, sizeof( path ), "%s/%s %lu", kDAMainMountPointFolder, name, index );
            }

            switch ( action )
            {
                case kDAMountPointActionLink:
                {
                    /*
                     * Link the mount point.
                     */

                    CFURLRef url;

                    url = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

                    if ( url )
                    {
                        char source[MAXPATHLEN];

                        if ( CFURLGetFileSystemRepresentation( url, TRUE, ( void * ) source, sizeof( source ) ) )
                        {
                            if ( symlink( source, path ) == 0 )
                            {
                                mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                            }
                        }
                    }

                    break;
                }
                case kDAMountPointActionMake:
                {
                    /*
                     * Create the mount point.
                     */

                    if ( mkdir( path, 0111 ) == 0 )
                    {
                        if ( DADiskGetUserUID( disk ) )
                        {
                            chown( path, DADiskGetUserUID( disk ), -1 );
                        }

                        mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                    }

                    break;
                }
                case kDAMountPointActionMove:
                {
                    /*
                     * Move the mount point.
                     */

                    CFURLRef url;

                    url = DADiskGetBypath( disk );

                    if ( url )
                    {
                        char source[MAXPATHLEN];

                        if ( CFURLGetFileSystemRepresentation( url, TRUE, ( void * ) source, sizeof( source ) ) )
                        {
                            if ( rename( source, path ) == 0 )
                            {
                                mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );
                            }
                        }
                    }

                    break;
                }
                case kDAMountPointActionNone:
                {
                    mountpoint = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault, ( void * ) path, strlen( path ), TRUE );

                    break;
                }
            }

            if ( mountpoint )
            {
                break;
            }
        }
    }

    CFRelease( string );

    return mountpoint;
}

Boolean DAMountGetPreference( DADiskRef disk, DAMountPreference preference )
{
    CFBooleanRef value;

    switch ( preference )
    {
        case kDAMountPreferenceDefer:
        {
            /*
             * Determine whether the media is removable.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferRemovableKey );

                value = value ? value : kCFBooleanTrue;
            }
            else
            {
                /*
                 * Determine whether the device is internal.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferInternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountDeferExternalKey );

                    value = value ? value : kCFBooleanTrue;
                }
            }

            break;
        }
        case kDAMountPreferenceTrust:
        {
            /*
             * Determine whether the media is removable.
             */

            if ( DADiskGetDescription( disk, kDADiskDescriptionMediaRemovableKey ) == kCFBooleanTrue )
            {
                value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustRemovableKey );

                value = value ? value : kCFBooleanFalse;
            }
            else
            {
                /*
                 * Determine whether the device is internal.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceInternalKey ) == kCFBooleanTrue )
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustInternalKey );

                    value = value ? value : kCFBooleanTrue;
                }
                else
                {
                    value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceMountTrustExternalKey );

                    value = value ? value : kCFBooleanFalse;
                }
            }

            break;
        }
        case kDAMountPreferenceDisableAutoMount:
        {
            /*
            * Determine whether auto mounts are allowed
            */

            value = CFDictionaryGetValue( gDAPreferenceList, kDAPreferenceAutoMountDisableKey );

            value = value ? value : kCFBooleanFalse;

            break;
        }

        default:
        {
            value = kCFBooleanFalse;

            break;
        }
    }

    assert( value );

    return CFBooleanGetValue( value );
}

void DAMountRemoveMountPoint( CFURLRef mountpoint )
{
    char path[MAXPATHLEN];

    /*
     * Obtain the mount point path.
     */

    if ( CFURLGetFileSystemRepresentation( mountpoint, TRUE, ( void * ) path, sizeof( path ) ) )
    {
        if ( ___isautofs( path ) == 0 )
        {
            Boolean remove;
            char       * p = path;

            remove = FALSE;

            if ( strncmp( p, kDAMainDataVolumeMountPointFolder, strlen( kDAMainDataVolumeMountPointFolder ) ) == 0 )
            {
                p += strlen( kDAMainDataVolumeMountPointFolder );
            }

            if ( strncmp( p, kDAMainMountPointFolder, strlen( kDAMainMountPointFolder ) ) == 0 )
            {
                if ( strrchr( p + strlen( kDAMainMountPointFolder ), '/' ) == p + strlen( kDAMainMountPointFolder ) )
                {
                    remove = TRUE;
                }
            }

///w:start
//          if ( remove == FALSE )
///w:stop
            {
                char file[MAXPATHLEN];

                strlcpy( file, path,                              sizeof( file ) );
                strlcat( file, "/",                               sizeof( file ) );
                strlcat( file, kDAMainMountPointFolderCookieFile, sizeof( file ) );

                /*
                 * Remove the mount point cookie file.
                 */

                if ( unlink( file ) == 0 )
                {
                    remove = TRUE;
                }
            }

            if ( remove )
            {
                /*
                 * Remove the mount point.
                 */

                rmdir( path );
            }
        }
    }
}

static Boolean DAAPFSCompareVolumeRole(DADiskRef disk, CFStringRef inRole)
{
    CFTypeRef              roles;
    Boolean                matchesRole = FALSE;

    roles = IORegistryEntrySearchCFProperty ( DADiskGetIOMedia( disk ),
                                            kIOServicePlane,
                                            CFSTR( "Role" ),
                                            kCFAllocatorDefault,
                                            0 );

    if ( roles )
    {

        if (CFGetTypeID( roles ) == CFArrayGetTypeID())
        {

            CFIndex count = CFArrayGetCount( roles );

            for ( int i=0; i<count; i++ )
            {
                CFStringRef role = CFArrayGetValueAtIndex( roles, i );

                if ( ( CFGetTypeID( role ) == CFStringGetTypeID() ) &&
                  ( (CFStringCompare( role, inRole, kCFCompareCaseInsensitive ) == 0) ) )
                {
                    matchesRole = TRUE;
                    break;
                }
            }

        }

        CFRelease ( roles );
    }

    return matchesRole;
}

void DAMountWithArguments( DADiskRef disk, CFURLRef mountpoint, DAMountCallback callback, void * callbackContext, ... )
{
    /*
     * Mount the specified volume.  A status of 0 indicates success.  All arguments in
     * the argument list shall be of type CFStringRef.  The argument list must be NULL
     * terminated.
     */

    CFStringRef                argument   = NULL;
    va_list                    arguments;
    CFBooleanRef               automatic  = kCFBooleanTrue;
    CFBooleanRef               check      = NULL;
    __DAMountCallbackContext * context    = NULL;
    CFIndex                    count      = 0;
    DAFileSystemRef            filesystem = DADiskGetFileSystem( disk );
    Boolean                    force      = FALSE;
    CFIndex                    index      = 0;
    CFDictionaryRef            map        = NULL;
    CFMutableStringRef         options    = NULL;
    int                        status     = 0;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    /*
     * Initialize our minimal state.
     */

    if ( mountpoint )
    {
        CFRetain( mountpoint );
    }

    /*
     * Prepare the mount context.
     */

    context = malloc( sizeof( __DAMountCallbackContext ) );

    if ( context == NULL )
    {
        status = ENOMEM;

        goto DAMountWithArgumentsErr;
    }

    /*
     * Prepare the mount options.
     */

    options = CFStringCreateMutable( kCFAllocatorDefault, 0 );

    if ( options == NULL )
    {
        status = ENOMEM;

        goto DAMountWithArgumentsErr;
    }

    va_start( arguments, callbackContext );

    while ( ( argument = va_arg( arguments, CFStringRef ) ) )
    {
        if ( CFEqual( argument, kDAFileSystemMountArgumentForce ) )
        {
            force = TRUE;
        }
        else if ( CFEqual( argument, CFSTR( "automatic" ) ) )
        {
            automatic = NULL;

            check = kCFBooleanTrue;
        }
        else
        {
            CFStringAppend( options, argument );
            CFStringAppend( options, CFSTR( "," ) );
        }
    }

    va_end( arguments );

    CFStringTrim( options, CFSTR( "," ) );
///w:start
    context->automatic = ( automatic == NULL ) ? TRUE : FALSE;
///w:stop

    /*
     * no DA mount allowed except apfs preboot volume
     */
    if ( DADiskGetDescription( disk, kDADiskDescriptionDeviceTDMLockedKey ) == kCFBooleanTrue )
    {
        status = EPERM;

///w:start
        /*
         * In the future, use APFSVolumeRole when link with apfs framework can not be avoided.
         */
        if ( DAUnitGetState( disk, _kDAUnitStateHasAPFS ) )
        {
            if ( DAAPFSCompareVolumeRole ( disk, CFSTR("PreBoot") ) == TRUE )
            {
                status = 0;
            }
        }
///w:stop

        if ( status )
            goto DAMountWithArgumentsErr;
    }

///w:start

        /*
        * Mount APFS system volumes as read only.
        */
        if ( ( context->automatic == TRUE ) && ( DAUnitGetState( disk, _kDAUnitStateHasAPFS ) ) )
        {
            if ( DAAPFSCompareVolumeRole ( disk, CFSTR("System") ) == TRUE )
            {
                CFStringInsert( options, 0, CFSTR( "," ) );
                CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );
            }
        }
///w:stop

    /*
     * Determine whether the volume is to be updated.
     */

    if ( DAMountContainsArgument( options, kDAFileSystemMountArgumentUpdate ) )
    {
        if ( mountpoint )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

        if ( mountpoint == NULL )
        {
            status = EINVAL;

            goto DAMountWithArgumentsErr;
        }

        CFRetain( mountpoint );
    }

    /*
     * Scan the mount map list.
     */

    count = CFArrayGetCount( gDAMountMapList1 );

    for ( index = 0; index < count; index++ )
    {
        map = CFArrayGetValueAtIndex( gDAMountMapList1, index );

        if ( map )
        {
            CFTypeRef   id;
            CFStringRef kind;

            id   = CFDictionaryGetValue( map, kDAMountMapProbeIDKey );
            kind = CFDictionaryGetValue( map, kDAMountMapProbeKindKey );

            if ( kind )
            {
                /*
                 * Determine whether the volume kind matches.
                 */

                if ( CFEqual( kind, DAFileSystemGetKind( filesystem ) ) == FALSE )
                {
                    continue;
                }
            }

            if ( CFGetTypeID( id ) == CFUUIDGetTypeID( ) )
            {
                /*
                 * Determine whether the volume UUID matches.
                 */

                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, id ) == kCFCompareEqualTo )
                {
                    break;
                }
            }
            else if ( CFGetTypeID( id ) == CFStringGetTypeID( ) )
            {
                /*
                 * Determine whether the volume name matches.
                 */

                if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, id ) == kCFCompareEqualTo )
                {
                    break;
                }
            }
            else if ( CFGetTypeID( id ) == CFDictionaryGetTypeID( ) )
            {
                boolean_t match = FALSE;

                /*
                 * Determine whether the device description matches.
                 */

                IOServiceMatchPropertyTable( DADiskGetIOMedia( disk ), id, &match );

                if ( match )
                {
                    break;
                }
            }
        }
    }

    /*
     * Process the map.
     */

    if ( index < count )
    {
        CFStringRef string;

        /*
         * Determine whether the volume is to be mounted.
         */

        if ( automatic == NULL )
        {
            automatic = CFDictionaryGetValue( map, kDAMountMapMountAutomaticKey );

            if ( automatic == kCFBooleanTrue )
            {
                DADiskSetState( disk, _kDADiskStateMountAutomatic,        TRUE );
                DADiskSetState( disk, _kDADiskStateMountAutomaticNoDefer, TRUE );
            }
        }

        /*
         * Prepare the mount options.
         */

        string = CFDictionaryGetValue( map, kDAMountMapMountOptionsKey );

        if ( string )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, string );
        }

        /*
         * Prepare the mount point.
         */

        if ( mountpoint == NULL )
        {
            mountpoint = CFDictionaryGetValue( map, kDAMountMapMountPathKey );

            if ( mountpoint )
            {
                CFRetain( mountpoint );
            }
        }
    }

    /*
     * Scan the mount map list.
     */

    count = CFArrayGetCount( gDAMountMapList2 );

    for ( index = 0; index < count; index++ )
    {
        map = CFArrayGetValueAtIndex( gDAMountMapList2, index );

        if ( map )
        {
            CFTypeRef id;

            id = CFDictionaryGetValue( map, kDAMountMapProbeIDKey );

            /*
             * Determine whether the volume UUID matches.
             */

            if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, id ) == kCFCompareEqualTo )
            {
                break;
            }
        }
    }

    /*
     * Process the map.
     */

    if ( index < count )
    {
        CFStringRef string;

        /*
         * Prepare the mount options.
         */

        string = CFDictionaryGetValue( map, kDAMountMapMountOptionsKey );

        if ( string )
        {
            CFStringInsert( options, 0, CFSTR( "," ) );
            CFStringInsert( options, 0, string );
        }
    }

    /*
     * Determine whether the volume is to be mounted.
     */

    if ( automatic == NULL )
    {
        if ( DADiskGetState( disk, _kDADiskStateMountAutomatic ) )
        {
            if ( DADiskGetState( disk, _kDADiskStateMountAutomaticNoDefer ) )
            {
                automatic = kCFBooleanTrue;
            }
        }
        else
        {
            automatic = kCFBooleanFalse;
        }

        if ( automatic == NULL )
        {
            if ( gDAConsoleUserList == NULL )
            {
                if ( DAMountGetPreference( disk, kDAMountPreferenceDefer ) )
                {
                    automatic = kCFBooleanFalse;
                }
            }
        }
    }

    if ( automatic == kCFBooleanFalse )
    {
        status = ECANCELED;

        goto DAMountWithArgumentsErr;
    }

    /*
     * Prepare the mount options.
     */

    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoWrite );
    }

    if ( DAMountGetPreference( disk, kDAMountPreferenceTrust ) == FALSE )
    {
        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoSetUserID );

        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoOwnership );

        CFStringInsert( options, 0, CFSTR( "," ) );
        CFStringInsert( options, 0, kDAFileSystemMountArgumentNoDevice );
    }
///w:start
    if ( CFEqual( DAFileSystemGetKind( filesystem ), CFSTR( "hfs" ) ) )
    {
        ___CFStringInsertFormat( options, 0, CFSTR( "-m=%o," ), 0755 );

        if ( DADiskGetUserGID( disk ) )
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-g=%d," ), DADiskGetUserGID( disk ) );
        }
        else
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-g=%d," ), ___GID_UNKNOWN );
        }

        if ( DADiskGetUserUID( disk ) )
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-u=%d," ), DADiskGetUserUID( disk ) );
        }
        else
        {
            ___CFStringInsertFormat( options, 0, CFSTR( "-u=%d," ), ___UID_UNKNOWN );
        }
    }
///w:stop

    CFStringTrim( options, CFSTR( "," ) );

    /*
     * Determine whether the volume is to be repaired.
     */

    if ( check == NULL )
    {
        if ( DAMountContainsArgument( options, kDAFileSystemMountArgumentNoWrite ) )
        {
            check = kCFBooleanFalse;
        }
        else
        {
            check = kCFBooleanTrue;
        }
    }

    if ( check == kCFBooleanFalse )
    {
        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
        {
            if ( force == FALSE )
            {
                status = ___EDIRTY;

                goto DAMountWithArgumentsErr;
            }
        }
    }

    if ( check == kCFBooleanTrue )
    {
        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) == FALSE )
        {
            check = kCFBooleanFalse;
        }
    }

    /*
     * Repair the volume.
     */

    CFRetain( disk );

    context->assertionID     = kIOPMNullAssertionID;
    context->callback        = callback;
    context->callbackContext = callbackContext;
    context->disk            = disk;
    context->force           = force;
    context->mountpoint      = mountpoint;
    context->options         = options;

    if ( check == kCFBooleanTrue )
    {
        DALogDebug( "  repaired disk, id = %@, ongoing.", disk );

        IOPMAssertionCreateWithDescription( kIOPMAssertionTypePreventUserIdleSystemSleep,
                                            CFSTR( _kDADaemonName ),
                                            NULL,
                                            NULL,
                                            NULL,
                                            0,
                                            NULL,
                                            &context->assertionID );

        DAFileSystemRepair( DADiskGetFileSystem( disk ),
                            DADiskGetDevice( disk ),
                            __DAMountWithArgumentsCallbackStage1,
                            context );
    }
    else
    {
        __DAMountWithArgumentsCallbackStage1( ECANCELED, context );
    }

DAMountWithArgumentsErr:

    if ( status )
    {
        if ( context )
        {
            free( context );
        }

        if ( mountpoint )
        {
            CFRelease( mountpoint );
        }

        if ( options )
        {
            CFRelease( options );
        }

        if ( callback )
        {
            ( callback )( status, NULL, callbackContext );
        }
    }
}
