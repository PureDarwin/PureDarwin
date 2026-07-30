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

#include "DAProbe.h"

#include "DALog.h"
#include "DAMain.h"
#include "DASupport.h"

#include <fsproperties.h>
#include <sys/loadable_fs.h>

struct __DAProbeCallbackContext
{
    DAProbeCallback   callback;
    void *            callbackContext;
    CFMutableArrayRef candidates;
    DADiskRef         disk;
    DAFileSystemRef   filesystem;
};

typedef struct __DAProbeCallbackContext __DAProbeCallbackContext;

static void __DAProbeCallback( int status, CFBooleanRef clean, CFStringRef name, CFStringRef type, CFUUIDRef uuid, void * parameter )
{
    /*
     * Process the probe command's completion.
     */

    __DAProbeCallbackContext * context = parameter;

    DALogDebugHeader( "%s -> %s", gDAProcessNameID, gDAProcessNameID );

    if ( status )
    {
        /*
         * We have found no probe match for this media object.
         */

        if ( context->filesystem )
        {
            CFStringRef kind;

            kind = DAFileSystemGetKind( context->filesystem );

            DALogDebug( "  probed disk, id = %@, with %@, failure.", context->disk, kind );

            if ( status != FSUR_UNRECOGNIZED )
            {
                DALogError( "unable to probe %@ (status code 0x%08X).", context->disk, status );
            }

            CFRelease( context->filesystem );

            context->filesystem = NULL;
        }

        /*
         * Find a probe candidate for this media object.
         */

        while ( CFArrayGetCount( context->candidates ) )
        {
            CFDictionaryRef candidate;

            candidate = CFArrayGetValueAtIndex( context->candidates, 0 );

            if ( candidate )
            {
                DAFileSystemRef filesystem;

                filesystem = ( void * ) CFDictionaryGetValue( candidate, kDAFileSystemKey );

                if ( filesystem )
                {
                    CFDictionaryRef properties;

                    properties = CFDictionaryGetValue( candidate, CFSTR( kFSMediaPropertiesKey ) );

                    if ( properties )
                    {
                        boolean_t match = FALSE;

                        IOServiceMatchPropertyTable( DADiskGetIOMedia( context->disk ), properties, &match );

                        if ( match )
                        {
                            /*
                             * We have found a probe candidate for this media object.
                             */

                            CFStringRef kind;

                            kind = DAFileSystemGetKind( filesystem );

                            CFRetain( filesystem );

                            context->filesystem = filesystem;

                            if ( CFDictionaryGetValue( candidate, CFSTR( "autodiskmount" ) ) == kCFBooleanFalse )
                            {
                                DADiskSetState( context->disk, _kDADiskStateMountAutomatic,        FALSE );
                                DADiskSetState( context->disk, _kDADiskStateMountAutomaticNoDefer, FALSE );
                            }

                            CFArrayRemoveValueAtIndex( context->candidates, 0 );

                            DALogDebug( "  probed disk, id = %@, with %@, ongoing.", context->disk, kind );

                            DAFileSystemProbe( filesystem, DADiskGetDevice( context->disk ), __DAProbeCallback, context );

                            return;
                        }
                    }
                }
            }

            CFArrayRemoveValueAtIndex( context->candidates, 0 );
        }
    }
    else
    {
        /*
         * We have found a probe match for this media object.
         */

        CFStringRef kind;

        kind = DAFileSystemGetKind( context->filesystem );

        DALogDebug( "  probed disk, id = %@, with %@, success.", context->disk, kind );
    }

    if ( context->callback )
    {
        ( context->callback )( status, context->filesystem, clean, name, type, uuid, context->callbackContext );
    }

    CFRelease( context->candidates );
    CFRelease( context->disk       );

    if ( context->filesystem )  CFRelease( context->filesystem );

    free( context );
}

void DAProbe( DADiskRef disk, DAProbeCallback callback, void * callbackContext )
{
    /*
     * Probe the specified volume.  A status of 0 indicates success.
     */

    CFMutableArrayRef          candidates = NULL;
    __DAProbeCallbackContext * context    = NULL;
    CFNumberRef                size       = NULL;
    int                        status     = 0;

    /*
     * Prepare the probe context.
     */

    context = malloc( sizeof( __DAProbeCallbackContext ) );

    if ( context == NULL )
    {
        status = ENOMEM;

        goto DAProbeErr;
    }

    /*
     * Prepare the probe candidates.
     */

    candidates = CFArrayCreateMutableCopy( kCFAllocatorDefault, 0, gDAFileSystemProbeList );

    if ( candidates == NULL )
    {
        status = ENOMEM;

        goto DAProbeErr;
    }

    /*
     * Determine whether the disk is formatted.
     */

    size = DADiskGetDescription( disk, kDADiskDescriptionMediaSizeKey );

    if ( size )
    {
        if ( ___CFNumberGetIntegerValue( size ) == 0 )
        {
            CFArrayRemoveAllValues( candidates );
        }
    }

    /*
     * Probe the volume.
     */

    CFRetain( disk );

    context->callback        = callback;
    context->callbackContext = callbackContext;
    context->candidates      = candidates;
    context->disk            = disk;
    context->filesystem      = NULL;

    __DAProbeCallback( -1, NULL, NULL, NULL, NULL, context );

DAProbeErr:

    if ( status )
    {
        if ( candidates )
        {
            CFRelease( candidates );
        }

        if ( context )
        {
            free( context );
        }

        if ( callback )
        {
            ( callback )( status, NULL, NULL, NULL, NULL, NULL, callbackContext );
        }
    }
}
