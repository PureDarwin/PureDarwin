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

#include "DAQueue.h"

#include "DABase.h"
#include "DACallback.h"
#include "DAInternal.h"
#include "DALog.h"
#include "DAMain.h"
#include "DARequest.h"
#include "DASession.h"
#include "DAStage.h"

struct __DAResponseContext
{
    DAResponseCallback callback;
    void *             callbackContext;
    CFTypeRef          response;
};

typedef struct __DAResponseContext __DAResponseContext;

const CFTimeInterval __kDAResponseTimerGrace = 1;
const CFTimeInterval __kDAResponseTimerLimit = 10;

static void __DAResponseTimerRefresh( void );

static void __DAQueueCallbacks( _DACallbackKind kind, DADiskRef argument0, CFTypeRef argument1 )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDASessionList );

    for ( index = 0; index < count; index++ )
    {
        DASessionRef session;

        session = ( void * ) CFArrayGetValueAtIndex( gDASessionList, index );

        if ( kind == _kDAIdleCallback )
        {
            if ( DASessionGetState( session, kDASessionStateIdle ) )
            {
                continue;
            }
        }

        DAQueueCallbacks( session, kind, argument0, argument1 );

        if ( kind == _kDAIdleCallback )
        {
            DASessionSetState( session, kDASessionStateIdle, TRUE );
        }
    }
}

static void __DAQueueRequest( _DARequestKind kind, DADiskRef argument0, CFIndex argument1, CFTypeRef argument2, CFTypeRef argument3, DACallbackRef callback )
{
    DARequestRef request;

    request = DARequestCreate( kCFAllocatorDefault, kind, argument0, argument1, argument2, argument3, ___UID_ROOT, ___GID_WHEEL, callback );

    if ( request )
    {
        DAQueueRequest( request );

        CFRelease( request );
    }
}

static void __DAResponseComplete( DADiskRef disk )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDAResponseList );

    for ( index = 0; index < count; index++ )
    {
        DACallbackRef callback;

        callback = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( DACallbackGetDisk( callback ) == disk )
        {
            break;
        }
    }

    if ( index == count )
    {
        __DAResponseContext context;

        context = *( ( __DAResponseContext * ) CFDataGetBytePtr( DADiskGetContextRe( disk ) ) );

        DADiskSetContextRe( disk, NULL );

        ( context.callback )( context.response, context.callbackContext );

        if ( context.response )  CFRelease( context.response );
    }

    __DAResponseTimerRefresh( );
}

static void __DAResponsePrepare( DADiskRef disk, DAResponseCallback callback, void * callbackContext )
{
    CFDataRef data;

    data = CFDataCreateMutable( kCFAllocatorDefault, sizeof( __DAResponseContext ) );

    if ( data )
    {
        __DAResponseContext * context;

        context = ( void * ) CFDataGetBytePtr( data );

        bzero( context, sizeof( __DAResponseContext ) );

        context->callback        = callback;
        context->callbackContext = callbackContext;

        DADiskSetContextRe( disk, data );

        CFRelease( data );
    }
}

static void __DAResponseTimerCallback( CFRunLoopTimerRef timer, void * info )
{
    CFAbsoluteTime clock;
    CFIndex        count;
    CFIndex        index;

    clock = CFAbsoluteTimeGetCurrent( );

    count = CFArrayGetCount( gDAResponseList );

    for ( index = count - 1; index > -1; index-- )
    {
        DACallbackRef callback;

        callback = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( callback )
        {
            DASessionRef session;

            session = DACallbackGetSession( callback );

            if ( DASessionGetOption( session, kDASessionOptionNoTimeout ) == FALSE )
            {
                CFAbsoluteTime timeout;

                timeout = DACallbackGetTime( callback ) + __kDAResponseTimerLimit;

                if ( timeout < clock )
                {
                    DADiskRef disk;

                    disk = DACallbackGetDisk( callback );

                    if ( DASessionGetState( session, kDASessionStateTimeout ) == FALSE )
                    {
                        DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

                        DALogDebug( "  timed out session, id = %@.", session );

                        DALogError( "%@ not responding.", session );

                        DASessionSetState( session, kDASessionStateTimeout, TRUE );
                    }

                    CFRetain( disk );

                    CFArrayRemoveValueAtIndex( gDAResponseList, index );

                    __DAResponseComplete( disk );

                    CFRelease( disk );
                }
            }
        }
    }
}

static void __DAResponseTimerRefresh( void )
{
    static CFRunLoopTimerRef timer = NULL;

    CFAbsoluteTime clock;
    CFIndex        count;
    CFIndex        index;

    clock = kCFAbsoluteTimeIntervalSince1904;

    count = CFArrayGetCount( gDAResponseList );

    for ( index = 0; index < count; index++ )
    {
        DACallbackRef callback;

        callback = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( callback )
        {
            DASessionRef session;

            session = DACallbackGetSession( callback );

            if ( DASessionGetOption( session, kDASessionOptionNoTimeout ) == FALSE )
            {
                CFAbsoluteTime timeout;

                timeout = DACallbackGetTime( callback ) + __kDAResponseTimerLimit + __kDAResponseTimerGrace;

                if ( timeout < clock )
                {
                    clock = timeout;
                }
            }
        }
    }

    if ( timer )
    {
        CFRunLoopTimerSetNextFireDate( timer, clock );
    }
    else
    {
        timer = CFRunLoopTimerCreate( kCFAllocatorDefault, clock, kCFAbsoluteTimeIntervalSince1904, 0, 0, __DAResponseTimerCallback, NULL );

        if ( timer )
        {
            CFRunLoopAddTimer( CFRunLoopGetCurrent( ), timer, kCFRunLoopDefaultMode );
        }
    }
}

Boolean _DAResponseDispatch( CFTypeRef response, SInt32 responseID )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDAResponseList );

    for ( index = 0; index < count; index++ )
    {
        DACallbackRef callback;

        callback = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( ___CFNumberGetIntegerValue( DACallbackGetArgument1( callback ) ) == responseID )
        {
            DADiskRef disk;

            disk = DACallbackGetDisk( callback );

            switch ( DACallbackGetKind( callback ) )
            {
                case _kDADiskClaimReleaseCallback:
                case _kDADiskEjectApprovalCallback:
                case _kDADiskMountApprovalCallback:
                case _kDADiskUnmountApprovalCallback:
                {
                    DADissenterRef dissenter;

                    dissenter = ( void * ) response;

                    if ( dissenter )
                    {
                        CFDataRef data;

                        data = DADiskGetContextRe( disk );

                        if ( data )
                        {
                            __DAResponseContext * context;

                            context = ( void * ) CFDataGetBytePtr( data );

                            if ( context->response == NULL )
                            {
                                context->response = CFRetain( dissenter );
                            }
                        }

                        DALogError( "  dispatched response, id = %016llX:%016llX, kind = %s, disk = %@, dissented, status = 0x%08X.",
                                    DACallbackGetAddress( callback ),
                                    DACallbackGetContext( callback ),
                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                    disk,
                                    DADissenterGetStatus( dissenter ) );
                    }
                    else
                    {
                        DALogDebug( "  dispatched response, id = %016llX:%016llX, kind = %s, disk = %@, approved.",
                                    DACallbackGetAddress( callback ),
                                    DACallbackGetContext( callback ),
                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                    disk );
                    }

                    break;
                }
                case _kDADiskPeekCallback:
                {
                    DALogDebug( "  dispatched response, id = %016llX:%016llX, kind = %s, disk = %@.",
                                DACallbackGetAddress( callback ),
                                DACallbackGetContext( callback ),
                                _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                disk );

                    break;
                }
            }

            CFArrayRemoveValueAtIndex( gDAResponseList, index );

            __DAResponseComplete( disk );

            break;
        }
    }

    return ( index < count ) ? TRUE : FALSE;
}

void DADiskAppearedCallback( DADiskRef disk )
{
    __DAQueueCallbacks( _kDADiskAppearedCallback, disk, NULL );
}

void DADiskClaimReleaseCallback( DADiskRef disk, DACallbackRef callback, DAResponseCallback response, void * responseContext )
{
    __DAResponsePrepare( disk, response, responseContext );

    DAQueueCallback( callback, disk, NULL );

    __DAResponseComplete( disk );
}

void DADiskDescriptionChangedCallback( DADiskRef disk, CFTypeRef key )
{
    if ( CFGetTypeID( key ) == CFArrayGetTypeID( ) )
    {
        __DAQueueCallbacks( _kDADiskDescriptionChangedCallback, disk, key );
    }
    else
    {
        CFMutableArrayRef keys;

        keys = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

        CFArrayAppendValue( keys, key );

        __DAQueueCallbacks( _kDADiskDescriptionChangedCallback, disk, keys );

        CFRelease( keys );
    }
}

void DADiskDisappearedCallback( DADiskRef disk )
{
    __DAQueueCallbacks( _kDADiskDisappearedCallback, disk, NULL );
}

void DADiskEject( DADiskRef disk, DADiskEjectOptions options, DACallbackRef callback )
{
    __DAQueueRequest( _kDADiskEject, disk, options, NULL, NULL, callback );
}

void DADiskEjectApprovalCallback( DADiskRef disk, DAResponseCallback response, void * responseContext )
{
    __DAResponsePrepare( disk, response, responseContext );

    __DAQueueCallbacks( _kDADiskEjectApprovalCallback, disk, NULL );

    __DAResponseComplete( disk );
}

void DADiskMount( DADiskRef disk, CFURLRef mountpoint, DADiskMountOptions options, DACallbackRef callback )
{
    DADiskMountWithArguments( disk, mountpoint, options, callback, NULL );
}

void DADiskMountApprovalCallback( DADiskRef disk, DAResponseCallback response, void * responseContext )
{
    __DAResponsePrepare( disk, response, responseContext );

    __DAQueueCallbacks( _kDADiskMountApprovalCallback, disk, NULL );

    __DAResponseComplete( disk );
}

void DADiskMountWithArguments( DADiskRef disk, CFURLRef mountpoint, DADiskMountOptions options, DACallbackRef callback, CFStringRef arguments )
{
    __DAQueueRequest( _kDADiskMount, disk, options, mountpoint, arguments, callback );
}

void DADiskPeekCallback( DADiskRef disk, DACallbackRef callback, DAResponseCallback response, void * responseContext )
{
    __DAResponsePrepare( disk, response, responseContext );
    
    DAQueueCallback( callback, disk, NULL );

    __DAResponseComplete( disk );
}

void DADiskProbe( DADiskRef disk, DACallbackRef callback )
{
    __DAQueueRequest( _kDADiskProbe, disk, 0, NULL, NULL, callback );
}

void DADiskRefresh( DADiskRef disk, DACallbackRef callback )
{
    __DAQueueRequest( _kDADiskRefresh, disk, 0, NULL, NULL, callback );
}

void DADiskUnmount( DADiskRef disk, DADiskUnmountOptions options, DACallbackRef callback )
{
    __DAQueueRequest( _kDADiskUnmount, disk, options, NULL, NULL, callback );
}

void DADiskUnmountApprovalCallback( DADiskRef disk, DAResponseCallback response, void * responseContext )
{
    __DAResponsePrepare( disk, response, responseContext );

    __DAQueueCallbacks( _kDADiskUnmountApprovalCallback, disk, NULL );

    __DAResponseComplete( disk );
}

void DAIdleCallback( void )
{
    __DAQueueCallbacks( _kDAIdleCallback, NULL, NULL );
}

void DADiskListCompleteCallback( void )
{
    __DAQueueCallbacks( _kDADiskListCompleteCallback, NULL, NULL );
}

void DAQueueCallback( DACallbackRef callback, DADiskRef argument0, CFTypeRef argument1 )
{
    static SInt32 responseID = 0;
    
    DASessionRef session;

    session = DACallbackGetSession( callback );

    DALogDebugHeader( "%s -> %@", gDAProcessNameID, session );

    if ( DASessionGetState( session, kDASessionStateZombie ) == FALSE )
    {
        if ( DACallbackGetAddress( callback ) )
        {
            CFDictionaryRef match;

            match = DACallbackGetMatch( callback );

            if ( match )
            {
                if ( DADiskMatch( argument0, match ) == FALSE )
                {
                    return;
                }
            }

            switch ( DACallbackGetKind( callback ) )
            {
                case _kDADiskAppearedCallback:
                case _kDADiskDisappearedCallback:
                {
                    callback = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                    if ( callback )
                    {
                        DACallbackSetDisk( callback, argument0 );

                        DACallbackSetArgument0( callback, DADiskGetSerialization( argument0 ) );

                        DASessionQueueCallback( session, callback );

                        DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s, disk = %@.",
                                    DACallbackGetAddress( callback ),
                                    DACallbackGetContext( callback ),
                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                    argument0 );

                        CFRelease( callback );
                    }

                    break;
                }
                case _kDADiskClaimCallback:
                case _kDADiskEjectCallback:
                case _kDADiskMountCallback:
                case _kDADiskRenameCallback:
                case _kDADiskUnmountCallback:
                {
                    DACallbackSetDisk( callback, argument0 );

                    DACallbackSetArgument0( callback, DADiskGetSerialization( argument0 ) );

                    DACallbackSetArgument1( callback, argument1 );

                    DASessionQueueCallback( session, callback );

                    if ( argument1 )
                    {
                        DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s, disk = %@, status = 0x%08X.",
                                    DACallbackGetAddress( callback ),
                                    DACallbackGetContext( callback ),
                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                    argument0,
                                    DADissenterGetStatus( argument1 ) );
                    }
                    else
                    {
                        DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s, disk = %@, success.",
                                    DACallbackGetAddress( callback ),
                                    DACallbackGetContext( callback ),
                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                    argument0 );
                    }

                    break;
                }
                case _kDADiskClaimReleaseCallback:
                {
                    if ( DASessionGetState( session, kDASessionStateTimeout ) == FALSE )
                    {
                        assert( argument1 == NULL );

                        argument1 = ___CFNumberCreateWithIntegerValue( kCFAllocatorDefault, responseID );

                        if ( argument1 )
                        {
                            DACallbackRef response;

                            response = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                            if ( response )
                            {
                                DACallbackSetDisk( response, argument0 );

                                DACallbackSetArgument1( response, argument1 );

                                DACallbackSetTime( response, CFAbsoluteTimeGetCurrent( ) );

                                CFArrayAppendValue( gDAResponseList, response );

                                CFRelease( response );
                            }

                            callback = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                            if ( callback )
                            {
                                DACallbackSetDisk( callback, argument0 );

                                DACallbackSetArgument0( callback, DADiskGetSerialization( argument0 ) );

                                DACallbackSetArgument1( callback, argument1 );

                                DASessionQueueCallback( session, callback );

                                DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s, disk = %@.",
                                            DACallbackGetAddress( callback ),
                                            DACallbackGetContext( callback ),
                                            _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                            argument0 );

                                CFRelease( callback );
                            }

                            CFRelease( argument1 );
                        }

                        responseID++;
                    }

                    break;
                }
                case _kDADiskEjectApprovalCallback:
                case _kDADiskMountApprovalCallback:
                case _kDADiskPeekCallback:
                case _kDADiskUnmountApprovalCallback:
                {
                    if ( DASessionGetState( session, kDASessionStateTimeout ) == FALSE )
                    {
                        assert( argument1 == NULL );

                        argument1 = ___CFNumberCreateWithIntegerValue( kCFAllocatorDefault, responseID );

                        if ( argument1 )
                        {
                            DACallbackRef response;

                            response = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                            if ( response )
                            {
                                DACallbackSetDisk( response, argument0 );

                                DACallbackSetArgument1( response, argument1 );

                                DACallbackSetTime( response, CFAbsoluteTimeGetCurrent( ) );

                                CFArrayAppendValue( gDAResponseList, response );

                                CFRelease( response );
                            }

                            callback = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                            if ( callback )
                            {
                                DACallbackSetDisk( callback, argument0 );

                                DACallbackSetArgument0( callback, DADiskGetSerialization( argument0 ) );

                                DACallbackSetArgument1( callback, argument1 );

                                DASessionQueueCallback( session, callback );

                                DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s, disk = %@.",
                                            DACallbackGetAddress( callback ),
                                            DACallbackGetContext( callback ),
                                            _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                            argument0 );

                                CFRelease( callback );
                            }

                            CFRelease( argument1 );
                        }

                        responseID++;
                    }

                    break;
                }
                case _kDADiskDescriptionChangedCallback:
                {
                    if ( DADiskGetState( argument0, kDADiskStateZombie ) == FALSE )
                    {
                        CFMutableArrayRef intersection;
                        CFArrayRef        watch;

                        watch = DACallbackGetWatch( callback );

                        if ( watch )
                        {
                            intersection = CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, argument1 );

                            if ( intersection )
                            {
                                ___CFArrayIntersect( intersection, watch );
                            }
                        }
                        else
                        {
                            intersection = ( void * ) CFRetain( argument1 );
                        }

                        if ( intersection )
                        {
                            if ( CFArrayGetCount( intersection ) )
                            {
                                callback = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                                if ( callback )
                                {
                                    CFIndex count;
                                    CFIndex index;

                                    count = CFArrayGetCount( intersection );

                                    for ( index = 0; index < count; index++ )
                                    {
                                        DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s, disk = %@, key = %@.",
                                                    DACallbackGetAddress( callback ),
                                                    DACallbackGetContext( callback ),
                                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ),
                                                    argument0,
                                                    CFArrayGetValueAtIndex( intersection, index ) );
                                    }

                                    DACallbackSetDisk( callback, argument0 );

                                    DACallbackSetArgument0( callback, DADiskGetSerialization( argument0 ) );

                                    DACallbackSetArgument1( callback, intersection );

                                    DASessionQueueCallback( session, callback );

                                    CFRelease( callback );
                                }
                            }

                            CFRelease( intersection );
                        }
                    }

                    break;
                }
                case _kDAIdleCallback:
                case _kDADiskListCompleteCallback:
                {
                    callback = DACallbackCreateCopy( kCFAllocatorDefault, callback );

                    if ( callback )
                    {
                        DASessionQueueCallback( session, callback );

                        DALogDebug( "  dispatched callback, id = %016llX:%016llX, kind = %s.",
                                    DACallbackGetAddress( callback ),
                                    DACallbackGetContext( callback ),
                                    _DACallbackKindGetName( DACallbackGetKind( callback ) ) );

                        CFRelease( callback );
                    }

                    break;
                }
            }
        }
    }
}

void DAQueueCallbacks( DASessionRef session, _DACallbackKind kind, DADiskRef argument0, CFTypeRef argument1 )
{
    CFArrayRef callbacks;

    callbacks = DASessionGetCallbackRegister( session );

    if ( callbacks )
    {
        CFIndex count;
        CFIndex index;

        count = CFArrayGetCount( callbacks );

        for ( index = 0; index < count; index++ )
        {
            DACallbackRef callback;

            callback = ( void * ) CFArrayGetValueAtIndex( callbacks, index );

            if ( DACallbackGetKind( callback ) == kind )
            {
                DAQueueCallback( callback, argument0, argument1 );
            }
        }
    }
}

void DAQueueReleaseDisk( DADiskRef disk )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDAResponseList );

    for ( index = count - 1; index > -1; index-- )
    {
        DACallbackRef callback;

        callback = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( DACallbackGetDisk( callback ) == disk )
        {
            CFArrayRemoveValueAtIndex( gDAResponseList, index );

            __DAResponseComplete( disk );
        }
    }

    count = CFArrayGetCount( gDARequestList );

    for ( index = count - 1; index > -1; index-- )
    {
        DARequestRef request;

        request = ( void * ) CFArrayGetValueAtIndex( gDARequestList, index );

        if ( DARequestGetDisk( request ) == disk )
        {
            DARequestDispatchCallback( request, kDAReturnNotFound );

            CFArrayRemoveValueAtIndex( gDARequestList, index );
        }            
    }
}

void DAQueueReleaseSession( DASessionRef session )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDAResponseList );

    for ( index = count - 1; index > -1; index-- )
    {
        DACallbackRef callback;

        callback = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( DACallbackGetSession( callback ) == session )
        {
            DADiskRef disk;

            disk = DACallbackGetDisk( callback );

            CFArrayRemoveValueAtIndex( gDAResponseList, index );

            __DAResponseComplete( disk );
        }
    }

    count = CFArrayGetCount( gDARequestList );

    for ( index = count - 1; index > -1; index-- )
    {
        DARequestRef request;

        request = ( void * ) CFArrayGetValueAtIndex( gDARequestList, index );

        if ( request )
        {
            DACallbackRef callback;

            callback = DARequestGetCallback( request );

            if ( callback )
            {
                if ( DACallbackGetSession( callback ) == session )
                {
                    DARequestSetCallback( request, NULL );
                }
            }
        }
    }

    count = CFArrayGetCount( gDADiskList );

    for ( index = count - 1; index > -1; index-- )
    {
        DADiskRef disk;

        disk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

        if ( disk )
        {
            DACallbackRef callback;

            callback = DADiskGetClaim( disk );

            if ( callback )
            {
                if ( DACallbackGetSession( callback ) == session )
                {
                    DADiskSetClaim( disk, NULL );
                }
            }
        }
    }
}

void DAQueueRequest( DARequestRef request )
{
    DAReturn status;

    status = kDAReturnSuccess;

    switch ( DARequestGetKind( request ) )
    {
        case _kDADiskEject:
        case _kDADiskMount:
        case _kDADiskUnmount:
        {
            DADiskMountOptions options;

            options = DARequestGetArgument1( request );

            assert( kDADiskMountOptionWhole == kDADiskUnmountOptionWhole );

            if ( DARequestGetKind( request ) == _kDADiskEject )
            {
                options |= kDADiskMountOptionWhole;
            }

            if ( ( options & kDADiskMountOptionWhole ) )
            {
                DADiskRef disk;

                disk = DARequestGetDisk( request );

                if ( DARequestGetArgument2( request ) )
                {
                    status = kDAReturnBadArgument;
                }

                if ( DARequestGetArgument3( request ) )
                {
                    status = kDAReturnBadArgument;
                }

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
                    break;
                }
                else
                {
                    CFMutableArrayRef link;

                    link = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

                    if ( link )
                    {
                        CFIndex count;
                        CFIndex index;

                        count = CFArrayGetCount( gDADiskList );

                        for ( index = 0; index < count; index++ )
                        {
                            DADiskRef subdisk;

                            subdisk = ( void * ) CFArrayGetValueAtIndex( gDADiskList, index );

                            if ( disk != subdisk )
                            {
                                if ( DADiskGetBSDUnit( disk ) == DADiskGetBSDUnit( subdisk ) )
                                {
                                    DARequestRef subrequest;

                                    subrequest = DARequestCreate( kCFAllocatorDefault,
                                                                  DARequestGetKind( request ),
                                                                  subdisk,
                                                                  options,
                                                                  NULL,
                                                                  NULL,
                                                                  DARequestGetUserUID( request ),
                                                                  DARequestGetUserGID( request ),
                                                                  NULL );

                                    if ( subrequest )
                                    {
                                        CFArrayAppendValue( link, subrequest );

                                        CFArrayAppendValue( gDARequestList, subrequest );

                                        CFRelease( subrequest );
                                    }
                                }
                            }
                        }

                        DARequestSetLink( request, link );

                        CFRelease( link );
                    }
                }
            }

            break;
        }
    }

    if ( status )
    {
        DARequestDispatchCallback( request, status );
    }
    else
    {
        CFArrayAppendValue( gDARequestList, request );

        DAStageSignal( );
    }
}

void DAQueueUnregisterCallback( DACallbackRef callback )
{
    CFIndex count;
    CFIndex index;

    count = CFArrayGetCount( gDAResponseList );

    for ( index = count - 1; index > -1; index-- )
    {
        DACallbackRef item;

        item = ( void * ) CFArrayGetValueAtIndex( gDAResponseList, index );

        if ( DACallbackGetSession( item ) == DACallbackGetSession( callback ) )
        {
            if ( DACallbackGetAddress( item ) == DACallbackGetAddress( callback ) )
            {
                if ( DACallbackGetContext( item ) == DACallbackGetContext( callback ) )
                {
                    DADiskRef disk;

                    disk = DACallbackGetDisk( item );

                    CFArrayRemoveValueAtIndex( gDAResponseList, index );

                    __DAResponseComplete( disk );
                }
            }
        }
    }
}
