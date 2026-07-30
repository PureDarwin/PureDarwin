/*
 * Copyright (c) 1998-2017 Apple Inc. All rights reserved.
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

#include "DARequest.h"

#include "DABase.h"
#include "DACallback.h"
#include "DADialog.h"
#include "DADissenter.h"
#include "DAFileSystem.h"
#include "DALog.h"
#include "DAMain.h"
#include "DAMount.h"
#include "DAPrivate.h"
#include "DAQueue.h"
#include "DAStage.h"
#include "DASupport.h"
#include "DAThread.h"

#include <fcntl.h>
#include <libproc.h>
#include <unistd.h>
#include <sys/disk.h>
#include <DiskArbitration/DiskArbitration.h>

static void __DARequestClaimCallback( int status, void * context );
static void __DARequestClaimReleaseCallback( CFTypeRef response, void * context );
static void __DARequestEjectCallback( int status, void * context );
static void __DARequestEjectApprovalCallback( CFTypeRef response, void * context );
static int  __DARequestEjectEject( void * context );
static void __DARequestMountCallback( int status, CFURLRef mountpoint, void * context );
static void __DARequestMountApprovalCallback( CFTypeRef response, void * context );
static void __DARequestProbeCallback( int status, void * context );
static void __DARequestRefreshCallback( int status, void * context );
static void __DARequestRenameCallback( int status, void * context );
static void __DARequestUnmountCallback( int status, void * context );
static void __DARequestUnmountApprovalCallback( CFTypeRef response, void * context );
static int  __DARequestUnmountGetProcessID( void * context );
///w:start
static void __DARequestMountAuthorizationCallback( DAReturn status, void * context );
static int  __DARequestUnmountTickle( void * context );
static void __DARequestUnmountTickleCallback( int status, void * context );

static void __DARequestAuthorize( DARequestRef        request,
                                  DAAuthorizeCallback callback,
                                  void *              callbackContext,
                                  const char *        right )
{
    DASessionRef session;
    CFArrayRef   sessionList;
    CFIndex      sessionListCount;
    CFIndex      sessionListIndex;

    sessionList      = gDASessionList;
    sessionListCount = CFArrayGetCount( sessionList );

    for ( sessionListIndex = 0; sessionListIndex < sessionListCount; sessionListIndex++ )
    {
        session = ( void * ) CFArrayGetValueAtIndex( sessionList, sessionListIndex );

        if ( strcmp( _DASessionGetName( session ), "SystemUIServer" ) == 0 )
        {
            break;
        }
    }

    if ( sessionListIndex < sessionListCount )
    {
        DAAuthorizeWithCallback( session,
                                 _kDAAuthorizeOptionAuthenticateAdministrator,
                                 DARequestGetDisk( request ),
                                 DARequestGetUserUID( request ),
                                 DARequestGetUserGID( request ),
                                 callback,
                                 callbackContext,
                                 right );
    }
    else
    {
        ( callback )( kDAReturnNotPrivileged, callbackContext );
    }
}
///w:stop
static void __DARequestDispatchCallback( DARequestRef request, DADissenterRef dissenter )
{
    DACallbackRef callback;

    callback = DARequestGetCallback( request );

    if ( callback )
    {
        CFArrayRef link;

        link = DARequestGetLink( request );

        if ( link )
        {
            dissenter = DARequestGetDissenter( request );

            if ( dissenter == NULL )
            {
                CFIndex count;
                CFIndex index;

                count = CFArrayGetCount( link );

                for ( index = 0; index < count; index++ )
                {
                    DARequestRef subrequest;

                    subrequest = ( void * ) CFArrayGetValueAtIndex( link, index );

                    dissenter = DARequestGetDissenter( subrequest );

                    if ( dissenter )  break;
                }
            }
        }

        DAQueueCallback( callback, DARequestGetDisk( request ), dissenter );
    }
}

static Boolean __DARequestClaim( DARequestRef request )
{
    DADiskRef disk;

    disk = DARequestGetDisk( request );

    /*
     * Commence the claim release.
     */

    if ( DARequestGetState( request, kDARequestStateStagedApprove ) == FALSE )
    {
        DACallbackRef callback;

        callback = DADiskGetClaim( disk );
        
        CFRetain( request );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DARequestSetState( request, kDARequestStateStagedApprove, TRUE );

        if ( callback )
        {
            if ( DACallbackGetAddress( callback ) )
            {
                DADiskClaimReleaseCallback( disk, callback, __DARequestClaimReleaseCallback, request );
            }
            else
            {
                DADissenterRef dissenter;

                dissenter = DADissenterCreate( kCFAllocatorDefault, kDAReturnNotPermitted );

                __DARequestClaimReleaseCallback( dissenter, request );

                CFRelease( dissenter );
            }
        }
        else
        {
            __DARequestClaimReleaseCallback( NULL, request );
        }

        return FALSE;
    }

    if ( DARequestGetDissenter( request ) )
    {
        DADissenterRef dissenter;

        dissenter = DARequestGetDissenter( request );

        __DARequestDispatchCallback( request, dissenter );

        DAStageSignal( );

        return TRUE;
    }

    /*
     * Commence the claim.
     */

    {
        DACallbackRef callback;

        CFRetain( request );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DADiskSetClaim( disk, NULL );

        callback = DARequestGetCallback( request );

        if ( callback )
        {
            DASessionRef session;

            session = DACallbackGetSession( callback );

            if ( session )
            {
                mach_vm_offset_t address;
                mach_vm_offset_t context;

                address = ___CFNumberGetIntegerValue( DARequestGetArgument2( request ) );
                context = ___CFNumberGetIntegerValue( DARequestGetArgument3( request ) );

                callback = DACallbackCreate( kCFAllocatorDefault, session, address, context, _kDADiskClaimReleaseCallback, 0, NULL, NULL );

                if ( callback )
                {
                    DADiskSetClaim( disk, callback );

                    CFRelease( callback );
                }
            } 
        }

        __DARequestClaimCallback( 0, request );

        return TRUE;
    }
}

static void __DARequestClaimCallback( int status, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    disk = DARequestGetDisk( request );

    DALogDebug( "  claimed disk, id = %@, success.", disk );

    DARequestDispatchCallback( request, status ? unix_err( status ) : status );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static void __DARequestClaimReleaseCallback( CFTypeRef response, void * context )
{
    DARequestRef request = context;

    DARequestSetDissenter( request, response );

    DADiskSetState( DARequestGetDisk( request ), kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static Boolean __DARequestEject( DARequestRef request )
{
    DADiskRef disk;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    disk = DARequestGetDisk( request );

    /*
     * Commence the eject approval.
     */

    if ( DARequestGetState( request, kDARequestStateStagedApprove ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is ejectable.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == NULL )
        {
            status = kDAReturnUnsupported;
        }

        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanFalse )
        {
            status = kDAReturnUnsupported;
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            CFRetain( request );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DARequestSetState( request, kDARequestStateStagedApprove, TRUE );

            DADiskEjectApprovalCallback( disk, __DARequestEjectApprovalCallback, request );

            return FALSE;
        }
    }

    if ( DARequestGetDissenter( request ) )
    {
        DADissenterRef dissenter;

        dissenter = DARequestGetDissenter( request );

        __DARequestDispatchCallback( request, dissenter );

        DAStageSignal( );

        return TRUE;
    }

    /*
     * Commence the eject.
     */

    if ( DAUnitGetStateRecursively( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        CFRetain( request );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

        DALogDebug( "  ejected disk, id = %@, ongoing.", disk );

        DAThreadExecute( __DARequestEjectEject, disk, __DARequestEjectCallback, request );

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void __DARequestEjectCallback( int status, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    disk = DARequestGetDisk( request );

    if ( status )
    {
        /*
         * We were unable to eject the disk.
         */

        DADissenterRef dissenter;

        DALogDebug( "  ejected disk, id = %@, failure.", disk );

        DALogDebug( "unable to eject %@ (status code 0x%08X).", disk, status );

        dissenter = DADissenterCreate( kCFAllocatorDefault, unix_err( status ) );

        DARequestSetDissenter( request, dissenter );

        CFRelease( dissenter );
    }
    else
    {
        /*
         * We were able to eject the disk.
         */

        DALogDebug( "  ejected disk, id = %@, success.", disk );
    }

    DARequestDispatchCallback( request, status ? unix_err( status ) : status );

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static void __DARequestEjectApprovalCallback( CFTypeRef response, void * context )
{
    DARequestRef request = context;

    DARequestSetDissenter( request, response );

    DADiskSetState( DARequestGetDisk( request ), kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static int __DARequestEjectEject( void * context )
{
    DADiskRef disk = context;
    int       file;
    int       status;

    file = open( DADiskGetBSDPath( disk, TRUE ), O_RDONLY );

    if ( file == -1 )
    {
        status = errno;
    }
    else
    {
        status = ioctl( file, DKIOCEJECT, NULL );

        if ( status == -1 )
        {
            status = ( errno == ENOTTY ) ? 0 : errno;
        }

        close( file );
    }

    return status;
}

static Boolean __DARequestMount( DARequestRef request )
{
    DADiskRef disk;

    disk = DARequestGetDisk( request );

    if ( DARequestGetLink( request ) )
    {
        if ( DAUnitGetState( disk, kDAUnitStateCommandActive ) )
        {
            return FALSE;
        }    
    }

    /*
     * Commence the probe.
     */

    if ( DARequestGetState( request, kDARequestStateStagedProbe ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is probeable.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaPathKey ) == NULL )
        {
            status = kDAReturnUnsupported;
        }

        /*
         * Determine whether the disk is mounted.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
        {
            CFStringRef arguments;

            arguments = DARequestGetArgument3( request );

            if ( arguments == NULL || DAMountContainsArgument( arguments, kDAFileSystemMountArgumentUpdate ) == FALSE )
            {
                status = kDAReturnBusy;
            }
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            DARequestSetState( request, kDARequestStateStagedProbe, TRUE );

///w:start
            CFStringRef arguments;

            arguments = DARequestGetArgument3( request );

            if ( arguments == NULL || CFEqual( arguments, CFSTR( "automatic" ) ) == FALSE )
///w:stop
            if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) == NULL )
            {
                DADiskSetState( disk, kDADiskStateStagedProbe, FALSE );

                DAStageSignal( );

                return FALSE;
            }
        }
    }
    else
    {
        if ( DADiskGetState( disk, kDADiskStateStagedProbe ) == FALSE )
        {
            return FALSE;
        }
    }

    /*
     * Commence the mount approval.
     */

    if ( DARequestGetState( request, kDARequestStateStagedApprove ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is mountable.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
        {
            status = kDAReturnUnsupported;
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            CFRetain( request );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DARequestSetState( request, kDARequestStateStagedApprove, TRUE );

            DADiskMountApprovalCallback( disk, __DARequestMountApprovalCallback, request );

            return FALSE;
        }
    }
///w:start
    /*
     * Commence the mount authorization.
     */

    if ( DARequestGetState( request, _kDARequestStateStagedAuthorize ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        if ( DARequestGetDissenter( request ) )
        {
            DADissenterRef dissenter;

            dissenter = DARequestGetDissenter( request );

            if ( DADissenterGetStatus( dissenter ) == 0xF8DAFF01 )
            {
                DARequestSetDissenter( request, NULL );

                status = kDAReturnNotPrivileged;
            }
            else if ( DADissenterGetStatus( dissenter ) == 0xF8DAFF02 )
            {
                DARequestSetDissenter( request, NULL );

                DARequestSetState( request, _kDARequestStateMountArgumentNoWrite, TRUE );
            }
            else if ( DADissenterGetStatus( dissenter ) == 0xF8DAFF03 )
            {
                DARequestSetDissenter( request, NULL );

                DARequestSetState( request, _kDARequestStateMountArgumentNoWrite, TRUE );

                status = kDAReturnNotPrivileged;
            }
        }

        if ( status )
        {
            CFRetain( request );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DARequestSetState( request, _kDARequestStateStagedAuthorize, TRUE );

            __DARequestAuthorize( request, __DARequestMountAuthorizationCallback, request, _kDAAuthorizeRightMount );

            return FALSE;
        }
        else
        {
            DARequestSetState( request, _kDARequestStateStagedAuthorize, TRUE );
        }
    }
///w:stop

    if ( DARequestGetDissenter( request ) )
    {
        DADissenterRef dissenter;

        dissenter = DARequestGetDissenter( request );

        __DARequestDispatchCallback( request, dissenter );

        DAStageSignal( );

        return TRUE;
    }

    /*
     * Commence the mount.
     */

    if ( DAUnitGetStateRecursively( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        CFTypeRef path;

        path = DARequestGetArgument2( request );

        if ( path )
        {
            path = CFURLCreateWithString( kCFAllocatorDefault, path, NULL );
        }

        CFRetain( request );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

///w:start
        if ( DARequestGetState( request, _kDARequestStateMountArgumentNoWrite ) )
        {
            DAMountWithArguments( disk, path, __DARequestMountCallback, request, kDAFileSystemMountArgumentNoWrite, DARequestGetArgument3( request ), NULL );
        }
        else
///w:stop
        DAMountWithArguments( disk, path, __DARequestMountCallback, request, DARequestGetArgument3( request ), NULL );

        if ( path )
        {
            CFRelease( path );
        }

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void __DARequestMountCallback( int status, CFURLRef mountpoint, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    disk = DARequestGetDisk( request );

    if ( status )
    {
        /*
         * We were unable to mount the volume.
         */

        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, unix_err( status ) );

        DARequestSetDissenter( request, dissenter );

        CFRelease( dissenter );
    }
    else
    {
        /*
         * We were able to mount the volume.
         */

        CFStringRef arguments;

        DADiskSetBypath( disk, mountpoint );

        DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, mountpoint );

        arguments = DARequestGetArgument3( request );

        if ( arguments == NULL || DAMountContainsArgument( arguments, kDAFileSystemMountArgumentUpdate ) == FALSE )
        {
            DADiskDescriptionChangedCallback( disk, kDADiskDescriptionVolumePathKey );
        }
    }

    DARequestDispatchCallback( request, status ? unix_err( status ) : status );

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static void __DARequestMountApprovalCallback( CFTypeRef response, void * context )
{
    DARequestRef request = context;

    DARequestSetDissenter( request, response );

    DADiskSetState( DARequestGetDisk( request ), kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}
///w:start
static void __DARequestMountAuthorizationCallback( DAReturn status, void * context )
{
    DARequestRef request = context;

    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status );

        DARequestSetDissenter( request, dissenter );

        CFRelease( dissenter );
    }

    DADiskSetState( DARequestGetDisk( request ), kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}
///w:stop

static Boolean __DARequestProbe( DARequestRef request )
{
    DADiskRef disk;

    disk = DARequestGetDisk( request );

    /*
     * Commence the probe.
     */

///w:start
    if ( DARequestGetState( request, kDARequestStateStagedProbe ) == FALSE )
    {
        /*
         * Determine whether the disk is mounted.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) == NULL )
        {
            DARequestSetState( request, kDARequestStateStagedProbe, TRUE );

            DADiskSetState( disk, kDADiskStateStagedProbe, FALSE );

            DAStageSignal( );

            return FALSE;
        }
    }
    else
    {
        if ( DADiskGetState( disk, kDADiskStateStagedProbe ) == FALSE )
        {
            return FALSE;
        }
    }
///w:stop
    if ( DAUnitGetStateRecursively( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is mounted.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
        {
            status = kDAReturnBusy;
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            CFRetain( request );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

///w:start
//          DAProbe( disk, __DARequestProbeCallback, request );
            __DARequestProbeCallback( 0, request );
///w:stop

            return TRUE;
        }
    }
    else
    {
        return FALSE;
    }
}

static void __DARequestProbeCallback( int status, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    disk = DARequestGetDisk( request );

    DARequestDispatchCallback( request, status ? unix_err( status ) : status );

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static Boolean __DARequestRefresh( DARequestRef request )
{
    DADiskRef disk;

    disk = DARequestGetDisk( request );

    /*
     * Commence the refresh.
     */

    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is mountable.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
        {
            status = kDAReturnUnsupported;
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            CFRetain( request );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            status = _DADiskRefresh( disk );

            __DARequestRefreshCallback( status ? ENOTSUP : 0, request );

            return TRUE;
        }
    }
}

static void __DARequestRefreshCallback( int status, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    disk = DARequestGetDisk( request );

    DARequestDispatchCallback( request, status ? unix_err( status ) : status );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static Boolean __DARequestRename( DARequestRef request )
{
    DADiskRef disk;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    disk = DARequestGetDisk( request );

    /*
     * Commence the rename.
     */

    if ( DAUnitGetStateRecursively( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is mountable.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
        {
            status = kDAReturnUnsupported;
        }

        /*
         * Determine whether the disk is mounted.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) == NULL )
        {
            status = kDAReturnNotMounted;
        }

        /*
         * Determine whether the name is valid.
         */

        if ( DARequestGetArgument2( request ) == NULL )
        {
            status = kDAReturnUnsupported;
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            CFRetain( request );

            DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

            DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

            DALogDebug( "  renamed disk, id = %@, ongoing.", disk );

            DAFileSystemRename( DADiskGetFileSystem( disk ),
                                DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ),
                                DARequestGetArgument2( request ),
                                __DARequestRenameCallback,
                                request );

            return TRUE;
        }
    }
    else
    {
        return FALSE;
    }
}

static void __DARequestRenameCallback( int status, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    disk = DARequestGetDisk( request );

    if ( status )
    {
        /*
         * We were unable to rename the disk.
         */

        DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

        DALogDebug( "  renamed disk, id = %@, failure.", disk );

        DALogDebug( "unable to rename %@ (status code 0x%08X).", disk, status );
    }
    else
    {
        /*
         * We were able to rename the disk.
         */

        CFMutableArrayRef keys;

        keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

        if ( keys )
        {
            CFStringRef name;

            name = DARequestGetArgument2( request );

            if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, name ) )
            {
                CFURLRef mountpoint;

                DADiskSetDescription( disk, kDADiskDescriptionVolumeNameKey, name );

                CFArrayAppendValue( keys, kDADiskDescriptionVolumeNameKey );

                /*
                 * Rename the mount point.
                 */

                mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

                if ( CFEqual( CFURLGetString( mountpoint ), CFSTR( "file:///" ) ) )
                {
                    mountpoint = DAMountCreateMountPointWithAction( disk, kDAMountPointActionMove );

                    if ( mountpoint )
                    {
                        DADiskSetBypath( disk, mountpoint );

                        CFRelease( mountpoint );
                    }
                }
                else
                {
                    mountpoint = DAMountCreateMountPointWithAction( disk, kDAMountPointActionMove );

                    if ( mountpoint )
                    {
                        DADiskSetBypath( disk, mountpoint );

                        DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, mountpoint );

                        CFArrayAppendValue( keys, kDADiskDescriptionVolumePathKey );

                        CFRelease( mountpoint );
                    }
                }

                DADiskDescriptionChangedCallback( disk, keys );
            }

            CFRelease( keys );
        }

        DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

        DALogDebug( "  renamed disk, id = %@, success.", disk );
    }

    DARequestDispatchCallback( request, status ? unix_err( status ) : status );

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static Boolean __DARequestUnmount( DARequestRef request )
{
    DADiskRef disk;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    disk = DARequestGetDisk( request );

    if ( DARequestGetLink( request ) )
    {
        if ( DAUnitGetState( disk, kDAUnitStateCommandActive ) )
        {
            return FALSE;
        }    
    }

    /*
     * Commence the unmount approval.
     */

    if ( DARequestGetState( request, kDARequestStateStagedApprove ) == FALSE )
    {
        DAReturn status;

        status = kDAReturnSuccess;

        /*
         * Determine whether the disk is mountable.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumeMountableKey ) == kCFBooleanFalse )
        {
            status = kDAReturnUnsupported;
        }

        /*
         * Determine whether the disk is mounted.
         */

        if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) == NULL )
        {
            status = kDAReturnNotMounted;
        }
        else
        {
            CFURLRef mountpoint;

            mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

            if ( CFEqual( CFURLGetString( mountpoint ), CFSTR( "file:///" ) ) )
            {
                DADissenterRef dissenter;

                status = kDAReturnBusy;

                dissenter = DADissenterCreate( kCFAllocatorDefault, status );

                DARequestSetDissenter( request, dissenter );

                CFRelease( dissenter );
            }
        }

        if ( status )
        {
            DARequestDispatchCallback( request, status );

            DAStageSignal( );

            return TRUE;
        }
        else
        {
            if ( DADiskGetState( disk, kDADiskStateZombie ) )
            {
                DARequestSetState( request, kDARequestStateStagedApprove, TRUE );
            }
            else
            {
                CFRetain( request );

                DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

                DARequestSetState( request, kDARequestStateStagedApprove, TRUE );
///w:start
                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanTrue )
                {
                    DAThreadExecute( __DARequestUnmountTickle, disk, __DARequestUnmountTickleCallback, request );

                    return FALSE;
                }
///w:stop

                DADiskUnmountApprovalCallback( disk, __DARequestUnmountApprovalCallback, request );

                return FALSE;
            }
        }
    }

    if ( DARequestGetDissenter( request ) )
    {
        DADissenterRef dissenter;

        dissenter = DARequestGetDissenter( request );

        __DARequestDispatchCallback( request, dissenter );

        DAStageSignal( );

        return TRUE;
    }

    /*
     * Commence the unmount.
     */

    if ( DAUnitGetStateRecursively( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        DADiskUnmountOptions options;

        options = DARequestGetArgument1( request );

        CFRetain( request );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

        DALogDebug( "  unmounted disk, id = %@, ongoing.", disk );

        DAFileSystemUnmountWithArguments( DADiskGetFileSystem( disk ),
                                          DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ),
                                          __DARequestUnmountCallback,
                                          request,
                                          ( options & kDADiskUnmountOptionForce ) ? kDAFileSystemUnmountArgumentForce : NULL,
                                          NULL );

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static void __DARequestUnmountCallback( int status, void * context )
{
    DADiskRef    disk;
    DARequestRef request = context;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    disk = DARequestGetDisk( request );

    if ( status )
    {
        /*
         * We were unable to unmount the volume.
         */
        struct statfs * mountList;
        int mountListCount;
        int mountListIndex;

        /*
         * getmntinfo returns 0 in case of failure. In that case, the following loop will not be executed.
         */
        mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

        for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
        {
            if ( strncmp( _DAVolumeGetID( mountList + mountListIndex ), DADiskGetID( disk ), strlen( DADiskGetID( disk ) ) + 1 ) == 0 )
            {
                break;
            }
        }

        /*
         * It is possible for the disk to be already unmounted by the filesystem
         * due to unmounting system or data volume.
         * If the mountpoint for the disk does not exist, ignore the error.
         * In the case of getmntinfo returning 0 as mountListCount, then also the error is ignored.
         */
        if ( mountListIndex == mountListCount )
        {
            DALogDebug( " %@ is not mounted. Ignore the umount error", disk);
            status = 0;
            goto handleumount;
        }

        DADissenterRef dissenter;

        dissenter = DARequestGetDissenter( request );

        if ( dissenter == NULL )
        {
            DALogDebug( "  unmounted disk, id = %@, failure.", disk );

            DALogError( "unable to unmount %@ (status code 0x%08X).", disk, status );

///w:start
            status = EBUSY;
///w:stop
            dissenter = DADissenterCreate( kCFAllocatorDefault, unix_err( status ) );

            DARequestSetDissenter( request, dissenter );

            DAThreadExecute( __DARequestUnmountGetProcessID, request, __DARequestUnmountCallback, request );

            CFRelease( dissenter );

            return;
        }

        __DARequestDispatchCallback( request, dissenter );
    }

handleumount:

    if (0 == status)
    {
        /*
         * We were able to unmount the volume.
         */

        CFURLRef mountpoint;

        mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

        DAMountRemoveMountPoint( mountpoint );

        DADiskSetBypath( disk, NULL );

        DALogDebug( "  unmounted disk, id = %@, success.", disk );

        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaPathKey ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, NULL );

            DADiskDescriptionChangedCallback( disk, kDADiskDescriptionVolumePathKey );
        }
        else
        {
            DALogDebug( "  removed disk, id = %@.", disk );

            DADiskDisappearedCallback( disk );

            DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, NULL );

            DADiskSetState( disk, kDADiskStateZombie, TRUE );

            ___CFArrayRemoveValue( gDADiskList, disk );
        }

        __DARequestDispatchCallback( request, NULL );
    }

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static void __DARequestUnmountApprovalCallback( CFTypeRef response, void * context )
{
    DARequestRef request = context;

    if ( response )
    {
        DADiskUnmountOptions options;

        options = DARequestGetArgument1( request );

        if ( ( options & kDADiskUnmountOptionForce ) == 0 )
        {
            DARequestSetDissenter( request, response );
        }
///w:start
        if ( DADissenterGetStatus( response ) == 0xF8DAFF01 )
        {
            DARequestSetDissenter( request, response );
        }
///w:stop
    }

    DADiskSetState( DARequestGetDisk( request ), kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( request );
}

static int  __DARequestUnmountGetProcessID( void * context )
{
    DADiskRef    disk;
    CFURLRef     mountpoint;
    char *       path;
    DARequestRef request = context;

    disk = DARequestGetDisk( request );

    mountpoint = DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey );

    path = ___CFURLCopyFileSystemRepresentation( mountpoint );

    if ( path )
    {
        pid_t dissenterPID = 0;

        proc_listpidspath( PROC_ALL_PIDS, 0, path, PROC_LISTPIDSPATH_EXCLUDE_EVTONLY | PROC_LISTPIDSPATH_PATH_IS_VOLUME, &dissenterPID, sizeof( dissenterPID ) );

        if ( dissenterPID )
        {
            DADissenterRef dissenter;

            dissenter = DARequestGetDissenter( request );

            DADissenterSetProcessID( dissenter, dissenterPID );
        }

        free( path );
    }

    return -1;
}
///w:start
static int __DARequestUnmountTickle( void * context )
{
    DADiskRef disk = context;
    size_t    size;

    size = ___CFNumberGetIntegerValue( DADiskGetDescription( disk, kDADiskDescriptionMediaBlockSizeKey ) );

    if ( size )
    {
        char * buffer;

        buffer = malloc( size );
 
        if ( buffer )
        {
            int file;

            file = open( DADiskGetBSDPath( disk, TRUE ), O_RDONLY );

            if ( file != -1 )
            {
                read( file, buffer, size );

                close( file );
            }

            free( buffer );
        }
    }

    return 0;
}

static void __DARequestUnmountTickleCallback( int status, void * context )
{
    DADiskUnmountApprovalCallback( DARequestGetDisk( context ), __DARequestUnmountApprovalCallback, context );
}
///w:stop

DARequestRef DARequestCreate( CFAllocatorRef allocator,
                              _DARequestKind kind,
                              DADiskRef      argument0,
                              CFIndex        argument1,
                              CFTypeRef      argument2,
                              CFTypeRef      argument3,
                              uid_t          userUID,
                              gid_t          userGID,
                              DACallbackRef  callback )
{
    CFMutableDictionaryRef request;

    request = CFDictionaryCreateMutable( kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    if ( request )
    {
        ___CFDictionarySetIntegerValue( request, _kDARequestKindKey, kind );

        if ( argument0 )  CFDictionarySetValue( request, _kDARequestDiskKey, argument0 );
        if ( argument1 )  ___CFDictionarySetIntegerValue( request, _kDARequestArgument1Key, argument1 );
        if ( argument2 )  CFDictionarySetValue( request, _kDARequestArgument2Key, argument2 );
        if ( argument3 )  CFDictionarySetValue( request, _kDARequestArgument3Key, argument3 );

        ___CFDictionarySetIntegerValue( request, _kDARequestUserGIDKey, userGID );
        ___CFDictionarySetIntegerValue( request, _kDARequestUserUIDKey, userUID );

        if ( callback )  CFDictionarySetValue( request, _kDARequestCallbackKey,  callback );
    }

    return ( void * ) request;
}

Boolean DARequestDispatch( DARequestRef request )
{
    Boolean dispatch;

    dispatch = FALSE;

    if ( request )
    {
        DADiskRef disk;

        disk = DARequestGetDisk( request );

        if ( disk )
        {
            if ( DADiskGetState( disk, kDADiskStateCommandActive ) == FALSE )
            {
                if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
                {
                    switch ( DARequestGetKind( request ) )
                    {
                        case _kDADiskClaim:
                        {
                            dispatch = __DARequestClaim( request );

                            break;
                        }
                        case _kDADiskEject:
                        {
                            dispatch = __DARequestEject( request );

                            break;
                        }
                        case _kDADiskMount:
                        {
                            dispatch = __DARequestMount( request );

                            break;
                        }
                        case _kDADiskProbe:
                        {
                            dispatch = __DARequestProbe( request );

                            break;
                        }
                        case _kDADiskRefresh:
                        {
                            dispatch = __DARequestRefresh( request );

                            break;
                        }
                        case _kDADiskRename:
                        {
                            dispatch = __DARequestRename( request );

                            break;
                        }
                        case _kDADiskUnmount:
                        {
                            dispatch = __DARequestUnmount( request );

                            break;
                        }
                    }
                }
            }
        }
    }

    return dispatch;
}

void DARequestDispatchCallback( DARequestRef request, DAReturn status )
{
    if ( status )
    {
        DADissenterRef dissenter;

        dissenter = DADissenterCreate( kCFAllocatorDefault, status );

        __DARequestDispatchCallback( request, dissenter );

        CFRelease( dissenter );
    }
    else
    {
        __DARequestDispatchCallback( request, NULL );
    }
}

CFIndex DARequestGetArgument1( DARequestRef request )
{
    return ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestArgument1Key );
}

CFTypeRef DARequestGetArgument2( DARequestRef request )
{
    return CFDictionaryGetValue( ( void * ) request, _kDARequestArgument2Key );
}

CFTypeRef DARequestGetArgument3( DARequestRef request )
{
    return CFDictionaryGetValue( ( void * ) request, _kDARequestArgument3Key );
}

DACallbackRef DARequestGetCallback( DARequestRef request )
{
    return ( void * ) CFDictionaryGetValue( ( void * ) request, _kDARequestCallbackKey );
}

DADiskRef DARequestGetDisk( DARequestRef request )
{
    return ( void * ) CFDictionaryGetValue( ( void * ) request, _kDARequestDiskKey );
}

DADissenterRef DARequestGetDissenter( DARequestRef request )
{
    return CFDictionaryGetValue( ( void * ) request, _kDARequestDissenterKey );
}

_DARequestKind DARequestGetKind( DARequestRef request )
{
    return ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestKindKey );
}

CFArrayRef DARequestGetLink( DARequestRef request )
{
    return CFDictionaryGetValue( ( void * ) request, _kDARequestLinkKey );
}

Boolean DARequestGetState( DARequestRef request, DARequestState state )
{
    return ( ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestStateKey ) & state ) ? TRUE : FALSE;
}

gid_t DARequestGetUserGID( DARequestRef request )
{
    return ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestUserGIDKey );
}

uid_t DARequestGetUserUID( DARequestRef request )
{
    return ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestUserUIDKey );
}

void DARequestSetCallback( DARequestRef request, DACallbackRef callback )
{
    if ( callback )
    {
        CFDictionarySetValue( ( void * ) request, _kDARequestCallbackKey, callback );
    }
    else
    {
        CFDictionaryRemoveValue( ( void * ) request, _kDARequestCallbackKey );
    }
}

void DARequestSetDissenter( DARequestRef request, DADissenterRef dissenter )
{
    if ( dissenter )
    {
        CFDictionarySetValue( ( void * ) request, _kDARequestDissenterKey, dissenter );
    }
    else
    {
        CFDictionaryRemoveValue( ( void * ) request, _kDARequestDissenterKey );
    }
}

void DARequestSetLink( DARequestRef request, CFArrayRef link )
{
    if ( link )
    {
        CFDictionarySetValue( ( void * ) request, _kDARequestLinkKey, link );
    }
    else
    {
        CFDictionaryRemoveValue( ( void * ) request, _kDARequestLinkKey );
    }
}

void DARequestSetState( DARequestRef request, DARequestState state, Boolean value )
{
    if ( value )
    {
        state = ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestStateKey ) | state;
    }
    else
    {
        state = ___CFDictionaryGetIntegerValue( ( void * ) request, _kDARequestStateKey ) & ~state;
    }

    ___CFDictionarySetIntegerValue( ( void * ) request, _kDARequestStateKey, state );
}
