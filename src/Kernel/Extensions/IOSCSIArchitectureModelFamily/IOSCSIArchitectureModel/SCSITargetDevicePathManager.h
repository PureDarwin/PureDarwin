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


#ifndef __IOKIT_SCSI_TARGET_DEVICE_PATH_MANAGER_H__
#define __IOKIT_SCSI_TARGET_DEVICE_PATH_MANAGER_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Libkern includes
#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSDictionary.h>

// SCSI Architecture Model Family includes
#include "IOSCSITargetDevice.h"
#include "IOSCSIProtocolServices.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class declaration
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSITargetDevicePathManager;

class SCSITargetDevicePath : public OSObject
{
	
	OSDeclareDefaultStructors ( SCSITargetDevicePath )
	
public:
	
	static SCSITargetDevicePath *	Create ( SCSITargetDevicePathManager *	manager,
											 IOSCSIProtocolServices *		initialPath );
	
	SCSITargetDevicePathManager *	GetPathManager ( void ) const { return fPathManager; }
	IOSCSIProtocolServices *		GetInterface ( void ) const { return fInterface; }
	OSDictionary *					GetStatistics ( void ) const { return fStatistics; }
	OSNumber *						GetDomainIdentifier ( void ) const { return fDomainIdentifier; }
	bool							InitWithObjects ( SCSITargetDevicePathManager *	manager,
													  IOSCSIProtocolServices *		interface );
	
	static OSNumber *				GetInterfaceDomainIdentifier ( const IOSCSIProtocolServices * interface );
	
	void	Activate ( void );
	void	Inactivate ( void );
	
	void	AddBytesTransmitted ( UInt64 bytes ) { fBytesTransmitted->addValue ( bytes ); }
	void	AddBytesReceived ( UInt64 bytes )  { fBytesReceived->addValue ( bytes ); }
	void	IncrementCommandsProcessed ( void )  { fCommandsProcessed->addValue ( 1 ); }
	
	void	free ( void );
	
protected:
	
	SCSITargetDevicePathManager	*	fPathManager;
	IOSCSIProtocolServices *		fInterface;
	OSDictionary *					fStatistics;
	OSNumber *						fDomainIdentifier;
	OSNumber *						fBytesTransmitted;
	OSNumber *						fBytesReceived;
	OSNumber *						fCommandsProcessed;
	OSString *						fPathStatus;
	char *							fStatus;
};

class SCSITargetDevicePathManager : public OSObject
{
	
	OSDeclareAbstractStructors ( SCSITargetDevicePathManager )
	
protected:
	
	virtual bool InitializePathManagerForTarget (
						IOSCSITargetDevice * 		target,
						IOSCSIProtocolServices * 	initialPath );
	
	static void		PathTaskCallback ( SCSITaskIdentifier request );
	static bool		IsTransmit ( SCSITaskIdentifier 	request,
								 IOSCSITargetDevice * 	target,
								 UInt64 *				bytes );
	static bool		IsReceive ( SCSITaskIdentifier 		request,
								IOSCSITargetDevice * 	target,
								UInt64 *				bytes );
	
	static bool		SetPathLayerReference ( SCSITaskIdentifier request, void * newReference );
	static void *	GetPathLayerReference ( SCSITaskIdentifier request );
	static UInt64	GetRequestedDataTransferCount ( SCSITaskIdentifier request );
	
	IOSCSITargetDevice *	fTarget;
	OSArray *				fStatistics;
	
	void	free ( void );
	
public:
	
	virtual void					ExecuteCommand ( SCSITaskIdentifier request ) = 0;
	virtual SCSIServiceResponse		AbortTask ( SCSILogicalUnitNumber theLogicalUnit, SCSITaggedTaskIdentifier theTag ) = 0;
	virtual SCSIServiceResponse		AbortTaskSet ( SCSILogicalUnitNumber theLogicalUnit ) = 0;
	virtual SCSIServiceResponse		ClearACA ( SCSILogicalUnitNumber theLogicalUnit ) = 0;
	virtual SCSIServiceResponse		ClearTaskSet ( SCSILogicalUnitNumber theLogicalUnit ) = 0;
	virtual SCSIServiceResponse		LogicalUnitReset ( SCSILogicalUnitNumber theLogicalUnit ) = 0;
	virtual SCSIServiceResponse		TargetReset ( void ) = 0;
	virtual void					TaskCompletion ( SCSITaskIdentifier request, SCSITargetDevicePath * path );
	
	virtual bool	AddPath ( IOSCSIProtocolServices * path ) = 0;
	virtual void	RemovePath ( IOSCSIProtocolServices * path ) = 0;
	virtual void	PathStatusChanged ( IOSCSIProtocolServices * path, UInt32 newStatus ) = 0;
	
};


#endif	/* __IOKIT_SCSI_TARGET_DEVICE_PATH_MANAGER_H__ */