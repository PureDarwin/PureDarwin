/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef __IOKIT_IO_SCSI_ARCHITECTURE_MODEL_FAMILY_DEBUGGING__
#define __IOKIT_IO_SCSI_ARCHITECTURE_MODEL_FAMILY_DEBUGGING__

#if KERNEL
#include <IOKit/IOLib.h>
#else
#include <IOKit/IOKitLib.h>
#endif

#ifndef DEBUG_ASSERT_COMPONENT_NAME_STRING
	#define DEBUG_ASSERT_COMPONENT_NAME_STRING "SAM"
#endif

#if KERNEL

#ifdef __cplusplus
extern "C" {
#endif

#if SAM_MODULE

/* IOSCSIArchitectureModelFamilyDebugAssert prototype*/
void
IOSCSIArchitectureModelFamilyDebugAssert ( 	const char * componentNameString,
											const char * assertionString, 
											const char * exceptionLabelString,
											const char * errorString,
											const char * fileName,
											long lineNumber,
											int errorCode );

#else

extern void
IOSCSIArchitectureModelFamilyDebugAssert ( 	const char * componentNameString,
											const char * assertionString, 
											const char * exceptionLabelString,
											const char * errorString,
											const char * fileName,
											long lineNumber,
											int errorCode );


#endif /* SAM_MODULE */

#ifdef __cplusplus
}
#endif


#define DEBUG_ASSERT_MESSAGE( componentNameString, \
	assertionString, \
	exceptionLabelString, \
	errorString, \
	fileName, \
	lineNumber, \
	error ) \
	IOSCSIArchitectureModelFamilyDebugAssert( componentNameString, \
	assertionString, \
	exceptionLabelString, \
	errorString, \
	fileName, \
	lineNumber, \
	error )


#endif	/* KERNEL */


#include </usr/include/AssertMacros.h>


// Other helpful macros (maybe some day these will make
// their way into /usr/include/AssertMacros.h)

#define require_success( errorCode, exceptionLabel ) \
	require( kIOReturnSuccess == (errorCode), exceptionLabel )

#define require_success_action( errorCode, exceptionLabel, action ) \
	require_action( kIOReturnSuccess == (errorCode), exceptionLabel, action )

#define require_success_quiet( errorCode, exceptionLabel ) \
	require_quiet( kIOReturnSuccess == (errorCode), exceptionLabel )

#define require_success_action_quiet( errorCode, exceptionLabel, action ) \
	require_action_quiet( kIOReturnSuccess == (errorCode), exceptionLabel, action )

#define require_success_string( errorCode, exceptionLabel, message ) \
	require_string( kIOReturnSuccess == (errorCode), exceptionLabel, message )

#define require_success_action_string( errorCode, exceptionLabel, action, message ) \
	require_action_string( kIOReturnSuccess == (errorCode), exceptionLabel, action, message )


#define require_nonzero( obj, exceptionLabel ) \
	require( ( 0 != obj ), exceptionLabel )

#define require_nonzero_action( obj, exceptionLabel, action ) \
	require_action( ( 0 != obj ), exceptionLabel, action )

#define require_nonzero_quiet( obj, exceptionLabel ) \
	require_quiet( ( 0 != obj ), exceptionLabel )

#define require_nonzero_action_quiet( obj, exceptionLabel, action ) \
	require_action_quiet( ( 0 != obj ), exceptionLabel, action )

#define require_nonzero_string( obj, exceptionLabel, message ) \
	require_string( ( 0 != obj ), exceptionLabel, message )

#define require_nonzero_action_string( obj, exceptionLabel, action, message ) \
	require_action_string( ( 0 != obj ), exceptionLabel, action, message )


#endif	/* __IOKIT_IO_SCSI_ARCHITECTURE_MODEL_FAMILY_DEBUGGING__ */