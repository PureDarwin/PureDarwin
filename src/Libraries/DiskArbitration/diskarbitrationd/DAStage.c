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

#include "DAStage.h"

#include "DABase.h"
#include "DACallback.h"
#include "DADialog.h"
#include "DADisk.h"
#include "DAFileSystem.h"
#include "DAMain.h"
#include "DAMount.h"
#include "DAPrivate.h"
#include "DAProbe.h"
#include "DAQueue.h"
#include "DASupport.h"

#include <unistd.h>
#include <sys/mount.h>

///w:start
void ___os_transaction_begin( void );
void ___os_transaction_end( void );
///w:end

const CFTimeInterval __kDABusyTimerGrace = 1;
const CFTimeInterval __kDABusyTimerLimit = 10;

static CFRunLoopSourceRef __gDAStageRunLoopSource = NULL;

static void               __DAStageAppeared( DADiskRef disk );
static void               __DAStageMount( DADiskRef disk );
static void               __DAStagePeek( DADiskRef disk );
static void               __DAStagePeekCallback( CFTypeRef response, void * context );
static CFComparisonResult __DAStagePeekCompare( const void * value1, const void * value2, void * context );
static void               __DAStageProbe( DADiskRef disk );

static void __DAStageProbeCallback( int             status,
                                    DAFileSystemRef filesystem,
                                    CFBooleanRef    clean,
                                    CFStringRef     name,
                                    CFStringRef     type,
                                    CFUUIDRef       uuid,
                                    void *          context );

static void __DABusyTimerCallback( CFRunLoopTimerRef timer, void * info )
{
    DAStageSignal( );
}

static void __DABusyTimerRefresh( CFAbsoluteTime clock )
{
    static CFRunLoopTimerRef timer = NULL;

    if ( timer )
    {
        CFRunLoopTimerSetNextFireDate( timer, clock );
    }
    else
    {
        timer = CFRunLoopTimerCreate( kCFAllocatorDefault, clock, kCFAbsoluteTimeIntervalSince1904, 0, 0, __DABusyTimerCallback, NULL );

        if ( timer )
        {
            CFRunLoopAddTimer( CFRunLoopGetCurrent( ), timer, kCFRunLoopDefaultMode );
        }
    }
}

static void __DAStageAppeared( DADiskRef disk )
{
    /*
     * We commence the "appeared" stage if the conditions are right.
     */

    DADiskSetState( disk, kDADiskStateStagedAppear, TRUE );
    
    DADiskAppearedCallback( disk );

    DAStageSignal( );
}

static void __DAStageDispatch( void * info )
{
    static Boolean fresh = FALSE;

    CFAbsoluteTime clock;
    CFIndex        count;
    CFIndex        index;
    Boolean        quiet = TRUE;

    /*
     * Determine whether a unit has quiesced.  We do not allow I/O Kit to stay busy excessively.
     */

    clock = kCFAbsoluteTimeIntervalSince1904;

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( disk )
        {
            CFAbsoluteTime timeout;

            timeout = DADiskGetBusy( disk );

            if ( timeout == 0 )
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanTrue )
                {
                    DAUnitSetState( disk, kDAUnitStateHasQuiescedNoTimeout, TRUE );
                }
            }

            timeout += __kDABusyTimerLimit;

            if ( timeout < CFAbsoluteTimeGetCurrent( ) )
            {
                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanTrue )
                {
                    DAUnitSetState( disk, kDAUnitStateHasQuiesced, TRUE );
                }
            }
            else
            {
                timeout += __kDABusyTimerGrace;

                if ( timeout < clock )
                {
                    clock = timeout;
                }

                quiet = FALSE;
            }
        }
    }

    __DABusyTimerRefresh( clock );

    count = CFArrayGetCount( gDADiskList );

    for ( index = 0; index < count; index++ )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( DADiskGetState( disk, kDADiskStateCommandActive ) == FALSE )
        {
            if ( DADiskGetState( disk, kDADiskStateStagedProbe ) == FALSE )
            {
                if ( fresh )
                {
                    DAFileSystemListRefresh( );

                    DAMountMapListRefresh1( );
                    
                    DAMountMapListRefresh2( );

                    fresh = FALSE;
                }

                __DAStageProbe( disk );
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedPeek ) == FALSE )
            {
                __DAStagePeek( disk );
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedMount ) == FALSE )
            {
                if ( gDAExit )
                {
                    continue;
                }

                /*
                 * We stall the "mount" stage if the conditions are not right.
                 */

                if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
                {
                    CFIndex subcount;
                    CFIndex subindex;

                    subcount = CFArrayGetCount( gDADiskList );

                    for ( subindex = 0; subindex < subcount; subindex++ )
                    {
                        DADiskRef subdisk;

                        subdisk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, subindex );

                        if ( DADiskGetBSDUnit( disk ) == DADiskGetBSDUnit( subdisk ) )
                        {
                            if ( DADiskGetState( subdisk, kDADiskStateStagedProbe ) == FALSE )
                            {
                                break;
                            }

                            if ( DADiskGetState( subdisk, kDADiskStateStagedMount ) == FALSE )
                            {
                                if ( DADiskGetState( subdisk, kDADiskStateRequireRepair ) == FALSE )
                                {
                                    break;
                                }
                            }
                        }
                    }

                    if ( subindex == subcount )
                    {
                        __DAStageMount( disk );
                    }
                }
                else
                {
                    __DAStageMount( disk );
                }
            }
            else if ( DADiskGetState( disk, kDADiskStateStagedAppear ) == FALSE )
            {
                __DAStageAppeared( disk );
            }
            else
            {
///w:start
                if ( gDAConsoleUserList == NULL )
                {
                    if ( DADiskGetDescription( disk, kDADiskDescriptionMediaTypeKey ) )
                    {
                        CFNumberRef size;

                        size = DADiskGetDescription( disk, kDADiskDescriptionMediaSizeKey );

                        if ( size )
                        {
                            if ( ___CFNumberGetIntegerValue( size ) == 0 )
                            {
                                if ( DAUnitGetState( disk, kDAUnitStateStagedUnreadable ) == FALSE )
                                {
                                    if ( _DAUnitIsUnreadable( disk ) )
                                    {
                                        DADiskEject( disk, kDADiskEjectOptionDefault, NULL );
                                    }

                                    DAUnitSetState( disk, kDAUnitStateStagedUnreadable, TRUE );
                                }
                            }
                        }
                    }
                }
///w:stop
                continue;
            }
        }

        quiet = FALSE;
    }

    count = CFArrayGetCount( gDARequestList );

    if ( count )
    {
        CFMutableSetRef dependencies;

        dependencies = CFSetCreateMutable( kCFAllocatorDefault, 0, &kCFTypeSetCallBacks );

        if ( dependencies )
        {
            for ( index = 0; index < count; index++ )
            {
                DARequestRef request;

                request = ( void * ) CFArrayGetValueAtIndex( gDARequestList, index );

                if ( request )
                {
                    DADiskRef disk;
                    Boolean   dispatch = TRUE;

                    disk = DARequestGetDisk( request );

                    /*
                     * Determine whether the request has undispatched dependencies.
                     */

                    if ( disk )
                    {
                        CFArrayRef link;

                        link = DARequestGetLink( request );

                        if ( link )
                        {
                            CFIndex subcount;
                            CFIndex subindex;

                            subcount = CFArrayGetCount( link );

                            for ( subindex = 0; subindex < subcount; subindex++ )
                            {
                                DARequestRef subrequest;

                                subrequest = ( void * ) CFArrayGetValueAtIndex( link, subindex );

                                if ( CFSetContainsValue( dependencies, DARequestGetDisk( subrequest ) ) )
                                {
                                    break;
                                }
                            }

                            if ( subindex < subcount )
                            {
                                dispatch = FALSE;
                            }
                        }

                        if ( CFSetContainsValue( dependencies, disk ) )
                        {
                            dispatch = FALSE;
                        }
                    }
                    else
                    {
                        if ( index )
                        {
                            break;
                        }
                    }

                    if ( dispatch )
                    {
                        /*
                         * Prepare to dispatch the request.
                         */

                        if ( DARequestGetKind( request ) == _kDADiskMount )
                        {
                            if ( fresh )
                            {
                                DAFileSystemListRefresh( );

                                DAMountMapListRefresh1( );

                                DAMountMapListRefresh2( );

                                fresh = FALSE;
                            }
                        }

                        /*
                         * Dispatch the request.
                         */

                        dispatch = DARequestDispatch( request );
                    }

                    if ( dispatch )
                    {
                        CFArrayRemoveValueAtIndex( gDARequestList, index );

                        count--;
                        index--;
                    }
                    else
                    {
                        /*
                         * Add the request to the undispatched dependencies.
                         */

                        if ( disk )
                        {
                            CFArrayRef link;

                            link = DARequestGetLink( request );

                            if ( link )
                            {
                                CFIndex subcount;
                                CFIndex subindex;

                                subcount = CFArrayGetCount( link );

                                for ( subindex = 0; subindex < subcount; subindex++ )
                                {
                                    DARequestRef subrequest;

                                    subrequest = ( void * ) CFArrayGetValueAtIndex( link, subindex );

                                    CFSetSetValue( dependencies, DARequestGetDisk( subrequest ) );
                                }
                            }

                            CFSetSetValue( dependencies, disk );
                        }
                    }
                }
            }

            CFRelease( dependencies );
        }

        quiet = FALSE;
    }

    if ( quiet )
    {
        fresh = TRUE;

        gDAIdle = TRUE;

        DAIdleCallback( );

        ___os_transaction_end( );

        if ( gDAConsoleUser )
        {
            /*
             * Determine whether a unit is unreadable or a volume is unrepairable.
             */

            count = CFArrayGetCount( gDADiskList );

            for ( index = 0; index < count; index++ )
            {
                DADiskRef disk;

                disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

                /*
                 * Determine whether a unit is unreadable.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWholeKey ) == kCFBooleanTrue )
                {
                    if ( DAUnitGetState( disk, kDAUnitStateStagedUnreadable ) == FALSE )
                    {
                        if ( _DAUnitIsUnreadable( disk ) )
                        {
                            DADialogShowDeviceUnreadable( disk );
                        }

                        DAUnitSetState( disk, kDAUnitStateStagedUnreadable, TRUE );
                    }
                }

                /*
                 * Determine whether a volume is unrepairable.
                 */

                if ( DADiskGetDescription( disk, kDADiskDescriptionVolumePathKey ) )
                {
                    if ( DADiskGetState( disk, kDADiskStateStagedUnrepairable ) == FALSE )
                    {
                        if ( DADiskGetState( disk, kDADiskStateRequireRepair ) )
                        {
                            if ( DADiskGetState( disk, _kDADiskStateMountAutomatic ) )
                            {
                                if ( DADiskGetClaim( disk ) == NULL )
                                {
                                    DADialogShowDeviceUnrepairable( disk );
                                }
                            }
                        }

                        DADiskSetState( disk, kDADiskStateStagedUnrepairable, TRUE );
                    }
                }
            }
        }
    }
}

static void __DAStageMount( DADiskRef disk )
{
    /*
     * We commence the "mount" stage if the conditions are right.
     */

    DADiskSetState( disk, kDADiskStateStagedMount, TRUE );

    /*
     * Skip the mount stage if auto mounts are disabled in preferences.
     */
    if ( DAMountGetPreference( disk, kDAMountPreferenceDisableAutoMount ) == false )
    {

        DADiskMountWithArguments( disk, NULL, kDADiskMountOptionDefault, NULL, CFSTR( "automatic" ) );
    }

    DAStageSignal( );
}

static void __DAStagePeek( DADiskRef disk )
{
    /*
     * We commence the "peek" stage if the conditions are right.
     */

    CFMutableArrayRef candidates;

    candidates = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    if ( candidates )
    {
        DASessionRef session;
        CFArrayRef   sessionList;
        CFIndex      sessionListCount;
        CFIndex      sessionListIndex;

        sessionList      = gDASessionList;
        sessionListCount = CFArrayGetCount( sessionList );

        for ( sessionListIndex = 0; sessionListIndex < sessionListCount; sessionListIndex++ )
        {
            DACallbackRef callback;
            CFArrayRef    callbackList;
            CFIndex       callbackListCount;
            CFIndex       callbackListIndex;
            
            session = ( void * ) CFArrayGetValueAtIndex( sessionList, sessionListIndex );

            callbackList      = DASessionGetCallbackRegister( session );
            callbackListCount = CFArrayGetCount( callbackList );

            for ( callbackListIndex = 0; callbackListIndex < callbackListCount; callbackListIndex++ )
            {
                callback = ( void * ) CFArrayGetValueAtIndex( callbackList, callbackListIndex );

                if ( DACallbackGetKind( callback ) == _kDADiskPeekCallback )
                {
                    CFArrayAppendValue( candidates, callback );
                }
            }
        }

        CFArraySortValues( candidates, CFRangeMake( 0, CFArrayGetCount( candidates ) ), __DAStagePeekCompare, NULL );

        /*
         * Commence the peek.
         */

        CFRetain( disk );

        DADiskSetContext( disk, candidates );

        DADiskSetState( disk, kDADiskStateStagedPeek, TRUE );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        __DAStagePeekCallback( NULL, disk );

        CFRelease( candidates );
    }
}

static void __DAStagePeekCallback( CFTypeRef response, void * context )
{
    CFMutableArrayRef candidates;
    DADiskRef         disk = context;

    candidates = ( void * ) DADiskGetContext( disk );

    if ( CFArrayGetCount( candidates ) )
    {
        DACallbackRef candidate;

        candidate = ( void * ) CFArrayGetValueAtIndex( candidates, 0 );

        CFRetain( candidate );

        CFArrayRemoveValueAtIndex( candidates, 0 );

        DADiskPeekCallback( disk, candidate, __DAStagePeekCallback, context );

        CFRelease( candidate );

        return;
    }
    
    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DADiskSetContext( disk, NULL );

    DAStageSignal( );

    CFRelease( disk );
}

static CFComparisonResult __DAStagePeekCompare( const void * value1, const void * value2, void * context )
{
    SInt32 order1 = DACallbackGetOrder( ( void * ) value1 );
    SInt32 order2 = DACallbackGetOrder( ( void * ) value2 );

    if ( order1 > order2 )  return kCFCompareGreaterThan;
    if ( order1 < order2 )  return kCFCompareLessThan;

    return kCFCompareEqualTo;
}

static void __DAStageProbe( DADiskRef disk )
{
    /*
     * We commence the "probe" stage if the conditions are right.
     */

    if ( DAUnitGetStateRecursively( disk, kDAUnitStateCommandActive ) == FALSE )
    {
        /*
         * Commence the probe.
         */

        CFRetain( disk );

        DADiskSetState( disk, kDADiskStateStagedProbe, TRUE );

        DADiskSetState( disk, kDADiskStateCommandActive, TRUE );

        DAUnitSetState( disk, kDAUnitStateCommandActive, TRUE );

        DAProbe( disk, __DAStageProbeCallback, disk );
    }
}

static void __DAStageProbeCallback( int             status,
                                    DAFileSystemRef filesystem,
                                    CFBooleanRef    clean,
                                    CFStringRef     name,
                                    CFStringRef     type,
                                    CFUUIDRef       uuid,
                                    void *          context )
{
    DADiskRef         disk = context;
    CFMutableArrayRef keys;
    CFStringRef       kind;

    DADiskSetFileSystem( disk, filesystem );

    DADiskSetState( disk, kDADiskStateRequireRepair,       FALSE );
    DADiskSetState( disk, kDADiskStateRequireRepairQuotas, FALSE );

    if ( status )
    {
        /*
         * We have found no probe match for this media object.
         */

        kind = NULL;
    }
    else
    {
        /*
         * We have found a probe match for this media object.
         */

        kind = DAFileSystemGetKind( filesystem );

///w:start
        if ( DADiskGetDescription( disk, kDADiskDescriptionMediaWritableKey ) == kCFBooleanFalse )
        {
            clean = kCFBooleanTrue;
        }
///w:stop
        if ( clean == kCFBooleanFalse )
        {
            DADiskSetState( disk, kDADiskStateRequireRepair,       TRUE );
            DADiskSetState( disk, kDADiskStateRequireRepairQuotas, TRUE );
        }
    }

    keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    if ( keys )
    {
        CFTypeRef object;

        object = kind ? kCFBooleanTrue : kCFBooleanFalse;

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeMountableKey, object ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeMountableKey, object );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeMountableKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeKindKey, kind ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeKindKey, kind );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeKindKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeNameKey, name ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeNameKey, name );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeNameKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeTypeKey, type ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeTypeKey, type );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeTypeKey );
        }

        if ( DADiskCompareDescription( disk, kDADiskDescriptionVolumeUUIDKey, uuid ) )
        {
            DADiskSetDescription( disk, kDADiskDescriptionVolumeUUIDKey, uuid );

            CFArrayAppendValue( keys, kDADiskDescriptionVolumeUUIDKey );
        }

        if ( CFArrayGetCount( keys ) )
        {
            if ( DADiskGetState( disk, kDADiskStateStagedAppear ) )
            {
                DADiskDescriptionChangedCallback( disk, keys );
            }
        }

        CFRelease( keys );
    }

    if ( DADiskGetState( disk, kDADiskStateStagedMount ) == FALSE )
    {
        struct statfs * mountList;
        int             mountListCount;
        int             mountListIndex;

        /*
         * Determine whether the disk is mounted.
         */

        mountListCount = getmntinfo( &mountList, MNT_NOWAIT );

        for ( mountListIndex = 0; mountListIndex < mountListCount; mountListIndex++ )
        {
            if ( mountList[mountListIndex].f_fsid.val[0] == DADiskGetBSDNode( disk ) )
            {
                /*
                 * We have determined that the disk is mounted.
                 */

                CFURLRef path;

                path = CFURLCreateFromFileSystemRepresentation( kCFAllocatorDefault,
                                                                ( void * ) mountList[mountListIndex].f_mntonname,
                                                                strlen( mountList[mountListIndex].f_mntonname ),
                                                                TRUE );

                if ( path )
                {
                    DADiskSetBypath( disk, path );

                    DADiskSetDescription( disk, kDADiskDescriptionVolumePathKey, path );

                    CFRelease( path );
                }

                if ( strcmp( mountList[mountListIndex].f_mntonname, "/" ) == 0 )
                {
                    path = DAMountCreateMountPointWithAction( disk, kDAMountPointActionLink );

                    if ( path )
                    {
                        DADiskSetBypath( disk, path );

                        CFRelease( path );
                    }

                    DADiskSetState( disk, _kDADiskStateMountAutomatic,        TRUE );
                    DADiskSetState( disk, _kDADiskStateMountAutomaticNoDefer, TRUE );
                }

                DADiskSetState( disk, kDADiskStateRequireRepair,       FALSE );
                DADiskSetState( disk, kDADiskStateRequireRepairQuotas, FALSE );

                break;
            }
        }
    }

    DAUnitSetState( disk, kDAUnitStateCommandActive, FALSE );

    DADiskSetState( disk, kDADiskStateCommandActive, FALSE );

    DAStageSignal( );

    CFRelease( disk );
}

CFRunLoopSourceRef DAStageCreateRunLoopSource( CFAllocatorRef allocator, CFIndex order )
{
    /*
     * Create a CFRunLoopSource for DAStage signals.
     */

    if ( __gDAStageRunLoopSource == NULL )
    {
        CFRunLoopSourceContext context = { 0 };

        context.perform = __DAStageDispatch;

        __gDAStageRunLoopSource = CFRunLoopSourceCreate( allocator, order, &context );
    }

    if ( __gDAStageRunLoopSource )
    {
        CFRetain( __gDAStageRunLoopSource );
    }

    return __gDAStageRunLoopSource;
}

void DAStageSignal( void )
{
    /*
     * Signal DAStage.
     */

    if ( gDAIdle )
    {
        ___os_transaction_begin( );
    }

    gDAIdle = FALSE;

    CFRunLoopSourceSignal( __gDAStageRunLoopSource );
}
