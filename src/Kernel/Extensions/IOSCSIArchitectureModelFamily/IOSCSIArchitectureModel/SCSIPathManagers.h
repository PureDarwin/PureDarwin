/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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



#ifndef __IOKIT_SCSI_PATH_MANAGERS_H__
#define __IOKIT_SCSI_PATH_MANAGERS_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern includes
#include <libkern/c++/OSArray.h>

// IOKit includes
#include <IOKit/IOLocks.h>

// SCSI Architecture Model Family includes
#include "SCSITargetDevicePathManager.h"
#include <IOKit/scsi/SCSIPort.h>

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSIPathSet : public OSArray
{
	
	OSDeclareDefaultStructors ( SCSIPathSet )
	
public:
	
	static SCSIPathSet *	withCapacity ( unsigned int capacity );
	bool 					setObject ( const SCSITargetDevicePath * path );
	bool 					member ( const SCSITargetDevicePath * path ) const;
	bool					member ( const IOSCSIProtocolServices * interface ) const;
	void					removeObject ( const IOSCSIProtocolServices * interface );
	SCSITargetDevicePath *	getAnyObject ( void ) const;
	SCSITargetDevicePath *	getObject ( unsigned int index ) const;
	SCSITargetDevicePath *	getObjectWithInterface ( const IOSCSIProtocolServices * interface ) const;
	
};


class SCSIPressurePathManager : public SCSITargetDevicePathManager
{
	
	OSDeclareDefaultStructors ( SCSIPressurePathManager )
	
private:
	
	IOLock *		fLock;
	SCSIPathSet *	fPathSet;
	SCSIPathSet *	fInactivePathSet;
	
protected:
	
	bool InitializePathManagerForTarget (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	void free ( void );
	
public:
	
	static SCSIPressurePathManager * Create (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	void							ExecuteCommand ( SCSITaskIdentifier request );
	virtual SCSIServiceResponse		AbortTask ( SCSILogicalUnitNumber theLogicalUnit, SCSITaggedTaskIdentifier theTag );
	virtual SCSIServiceResponse		AbortTaskSet ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		ClearACA ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		ClearTaskSet ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		LogicalUnitReset ( SCSILogicalUnitNumber theLogicalUnit );
	virtual SCSIServiceResponse		TargetReset ( void );
	virtual void					TaskCompletion ( SCSITaskIdentifier request, SCSITargetDevicePath * path );
	
	bool		AddPath ( IOSCSIProtocolServices * path );
	void		ActivatePath ( IOSCSIProtocolServices * path );
	void		InactivatePath ( IOSCSIProtocolServices * path );
	void		RemovePath ( IOSCSIProtocolServices * path );
	void		PathStatusChanged ( IOSCSIProtocolServices * path, SCSIPortStatus newStatus );
	
	
	class PortBandwidthGlobals
	{
		
	public:
		
		static PortBandwidthGlobals * GetSharedInstance ( void );
		
		PortBandwidthGlobals ( void );
		virtual ~PortBandwidthGlobals ( void );
		
		SCSITargetDevicePath *	AllocateBandwidth ( SCSIPathSet *	pathSet,
													UInt64			bytes );
		
		void	DeallocateBandwidth ( SCSITargetDevicePath * 	path,
									  UInt64 					bytes );
		
		void AddSCSIPort ( UInt32 domainID );
		
	private:
	
	#if DEBUG_STATS
		
		static void	sDumpDebugInfo (
							thread_call_param_t 	param0,
							thread_call_param_t 	param1 );
		void	DumpDebugInfo ( void );
		
		void	SetTimer ( void );
		
	#endif	/* DEBUG_STATS */
		
		UInt64 *		fListHead;
		IOLock *		fLock;
		UInt32			fPathsAllocated;
		UInt32			fCapacity;
		
	};

	
};


#endif	/* __IOKIT_SCSI_PATH_MANAGERS_H__ */