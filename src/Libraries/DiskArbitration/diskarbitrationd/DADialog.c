/*
 * Copyright (c) 1998-2014 Apple Inc. All rights reserved.
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

#include "DADialog.h"

#include "DAAgent.h"
#include "DAMain.h"

#include <xpc/private.h>
#include <os/log.h>

static void __DADialogShow( CFMutableArrayRef diskinfoarray, _DAAgentAction action )
{
    xpc_object_t message;

    message = xpc_dictionary_create( NULL, NULL, 0 );

    if ( message )
    {
        xpc_connection_t connection;

        connection = xpc_connection_create_mach_service( _kDAAgentName, NULL, 0 );

        if ( connection )
        {
            xpc_dictionary_set_uint64( message, _kDAAgentActionKey, action );

            xpc_object_t array = xpc_array_create(  NULL, 0 );
            int count;
            CFIndex index;

            count = CFArrayGetCount( diskinfoarray );

            for ( index = 0; index < count; index++ )
            {
                xpc_object_t dict = xpc_dictionary_create( NULL, NULL, 0 );
                CFDataRef serialization =  CFArrayGetValueAtIndex( diskinfoarray, index );
                xpc_dictionary_set_data( dict, _kDAAgentDiskKey, CFDataGetBytePtr( serialization ), CFDataGetLength( serialization ) );
                xpc_array_append_value(array, dict);
                xpc_release(dict);
            }

            CFArrayRemoveAllValues( diskinfoarray );
            xpc_dictionary_set_value(message, _kDAAgentDiskKey, array);

            xpc_release(array);

            xpc_connection_set_event_handler( connection, ^( xpc_object_t object ) { } );

            xpc_connection_set_target_uid( connection, gDAConsoleUserUID );

            xpc_connection_resume( connection );

            xpc_connection_send_message( connection, message );

            xpc_release( connection );
        }

        xpc_release( message );
    }
}

void DADialogShowDeviceRemoval( CFMutableArrayRef diskinfoarray )
{
    __DADialogShow( diskinfoarray, _kDAAgentActionShowDeviceRemoval );
}

void DADialogShowDeviceUnreadable( DADiskRef disk )
{
    CFMutableArrayRef diskInfoArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
    CFDataRef serialization = DADiskGetSerialization( disk );
    CFArrayAppendValue( diskInfoArray, serialization );
    __DADialogShow( diskInfoArray, _kDAAgentActionShowDeviceUnreadable );
    CFRelease( diskInfoArray );
}

void DADialogShowDeviceUnrepairable( DADiskRef disk )
{
    CFMutableArrayRef diskInfoArray = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );
    CFDataRef serialization = DADiskGetSerialization( disk );
    CFArrayAppendValue( diskInfoArray, serialization );
    __DADialogShow( diskInfoArray, _kDAAgentActionShowDeviceUnrepairable );
    CFRelease( diskInfoArray );
}
