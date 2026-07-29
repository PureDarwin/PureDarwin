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

#include "DADissenter.h"

#include "DAInternal.h"

#include <unistd.h>

DADissenterRef DADissenterCreate( CFAllocatorRef allocator, DAReturn status, CFStringRef string )
{
    CFMutableDictionaryRef dissenter;

    dissenter = CFDictionaryCreateMutable( allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    if ( dissenter )
    {
        ___CFDictionarySetIntegerValue( dissenter, _kDADissenterProcessIDKey, getpid( ) );
        ___CFDictionarySetIntegerValue( dissenter, _kDADissenterStatusKey,    status    );

        if ( string )
        {
            CFDictionarySetValue( dissenter, _kDADissenterStatusStringKey, string );
        }
    }

    return ( void * ) dissenter;
}

pid_t DADissenterGetProcessID( DADissenterRef dissenter )
{
    return ___CFDictionaryGetIntegerValue( ( void * ) dissenter, _kDADissenterProcessIDKey );
}

DAReturn DADissenterGetStatus( DADissenterRef dissenter )
{
    return ___CFDictionaryGetIntegerValue( ( void * ) dissenter, _kDADissenterStatusKey );
}

CFStringRef DADissenterGetStatusString( DADissenterRef dissenter )
{
    return CFDictionaryGetValue( ( void * ) dissenter, _kDADissenterStatusStringKey );
}
