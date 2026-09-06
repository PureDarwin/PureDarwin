/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


#include <IOKit/assert.h>
#include "IOATABlockStorageDriver.h"
#include "IOATABlockStorageDevice.h"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/ata/IOATATypes.h>

#define ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL 2

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)			IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)			IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)			IOLog x
#else
#define STATUS_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_PM_DEBUGGING_LEVEL >= 1 )
#define SERIAL_STATUS_LOG(x) 	kprintf x
#else
#define SERIAL_STATUS_LOG(x)
#endif


#define	super IOService
OSDefineMetaClassAndStructors ( IOATABlockStorageDriver_PD, IOService );

enum
{
	kATAIdleDevice		= 1,
	kATAStandbyDevice	= 2,
	kATASleepDevice		= 3
};


#pragma mark Public Methods


//---------------------------------------------------------------------------
// � init - Doesn't do anything, just calls superclass' init.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver_PD::init ( OSDictionary * properties )
{
		
	// Run by our superclass
	if ( super::init ( properties ) == false )
	{
		
		return false;
		
	}
	
	return true;
	
}


//---------------------------------------------------------------------------
// � start - Starts up the driver and spawn a nub.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver_PD::start ( IOService * provider )
{
	
	IOReturn		theErr				= kIOReturnSuccess;
	IOWorkLoop *	workLoop			= NULL;
	OSNumber *		numCommandObjects 	= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::start entering.\n" ) );

	fATADevice 					= NULL;
	fCommandPool				= NULL;
	fATAUnitID					= kATAInvalidDeviceID;
	fATADeviceType				= kUnknownATADeviceType;
	fATASocketType				= kUnknownSocket;
	fAPMLevel					= 0xFF;
	
	// First call start() in our superclass
	if ( super::start ( provider ) == false )
		return false;
	
	// Cache our provider
	fATADevice = OSDynamicCast ( IOATADevice, provider );
	if ( fATADevice == NULL )
		return false;
		
	// Find out if the device type is ATA
	if ( fATADevice->getDeviceType ( ) != reportATADeviceType ( ) )
	{

		ERROR_LOG ( ( "IOATABlockStorageDriver::start exiting, not an ATA device.\n" ) );
		return false;

	}
	
	reserved = ( ExpansionData * ) IOMalloc ( sizeof ( ExpansionData ) );
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::start opening device.\n" ) );
	
	if ( fATADevice->open ( this ) == false )
		return false;
	
	// Cache the drive unit number (master/slave assignment).
	fATAUnitID 	= fATADevice->getUnitID ( );
	fATADeviceType = fATADevice->getDeviceType ( );

	STATUS_LOG ( ( "IOATABlockStorageDriver::start fATAUnitID = %d.\n", ( UInt8 ) fATAUnitID ) );
	STATUS_LOG ( ( "IOATABlockStorageDriver::start fATADeviceType is %d\n", ( UInt8 ) fATADeviceType ) );
		
	bzero ( fDeviceIdentifyData, 512 );

	fDeviceIdentifyBuffer = IOMemoryDescriptor::withAddress ( ( void * ) fDeviceIdentifyData,
																512,
																kIODirectionIn );
	
	assert ( fDeviceIdentifyBuffer != NULL );
	
	fDeviceIdentifyBuffer->prepare ( );
	
	numCommandObjects 	= OSDynamicCast ( OSNumber, getProperty ( "IOCommandPoolSize" ) );
	fNumCommandObjects 	= numCommandObjects->unsigned32BitValue ( );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::start fNumCommandObjects = %u\n", (unsigned int)fNumCommandObjects ) );
	
	// Create an IOCommandGate (for power management support) and attach
	// this event source to the provider's workloop
	fCommandGate = IOCommandGate::commandGate ( this );
	assert ( fCommandGate != NULL );
	
	workLoop = getWorkLoop ( );
	assert ( workLoop != NULL );
	
	theErr = workLoop->addEventSource ( fCommandGate );
	assert ( theErr == kIOReturnSuccess );
	
	fCommandPool = IOCommandPool::commandPool ( this, workLoop, fNumCommandObjects );
	assert ( fCommandPool != NULL );
	
	allocateATACommandObjects ( );
	
	// Inspect the provider
	if ( inspectDevice ( fATADevice ) == false )
		return false;
	
	fCurrentPowerState 			= kIOATAPowerStateActive;
	fProposedPowerState			= kIOATAPowerStateActive;
	fNumCommandsOutstanding		= 0;
	fPowerTransitionInProgress 	= false;
	
	fPowerManagementThread = thread_call_allocate (
					( thread_call_func_t ) IOATABlockStorageDriver_PD::sPowerManagement,
					( thread_call_param_t ) this );
					
	if ( fPowerManagementThread == NULL )
	{
		
		ERROR_LOG ( ( "thread allocation failed.\n" ) );
		return false;
		
	}
		
	// A policy-maker must make these calls to join the PM tree,
	// and to initialize its state
	PMinit ( );								// initialize power management variables
	setIdleTimerPeriod ( k5Minutes );		// 5 minute inactivity timer
	provider->joinPMtree ( this );  		// join power management tree
	makeUsable ( );
	fPowerManagementInitialized = true;
	initForPM ( );

	bool nubOK = createNub ( provider );
	return ( nubOK );

}


//---------------------------------------------------------------------------
// � finalize - Terminates all power management
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver_PD::finalize ( IOOptionBits options )
{
	
    if ( fPowerManagementInitialized )
    {
        
        while ( fPowerTransitionInProgress )
        {
            
            IOSleep ( 1 );
            
        }
		
		fCommandGate->commandWakeup ( &fCurrentPowerState, false );
		
        PMstop ( );
        
		if ( fPowerManagementThread != NULL )
		{
			
			// If the power management thread is scheduled, unschedule it.
			thread_call_cancel ( fPowerManagementThread );
			
		}
		
        fPowerManagementInitialized = false;
		
    }
    
    return super::finalize ( options );
	
}


//---------------------------------------------------------------------------
// � stop - Stop the driver
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::stop ( IOService * provider )
{
	
	// Call our superclass
	super::stop ( provider );
	
}


#pragma mark -
#pragma mark Protected Methods


//---------------------------------------------------------------------------
// � free - Release allocated resources.
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::free ( void )
{

	IOWorkLoop *	workLoop;
	
	if ( fCommandPool != NULL )
	{
		
		deallocateATACommandObjects ( );
		fCommandPool->release ( );
		fCommandPool = NULL;
		
	}
		
	if ( fCommandGate != NULL )
	{
		
		workLoop = getWorkLoop ( );
		if ( workLoop != NULL )
		{
		
			workLoop->removeEventSource ( fCommandGate );
		
		}
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	if ( fPowerManagementThread != NULL )
	{
		
		thread_call_free ( fPowerManagementThread );
		
	}
	
	if ( reserved != NULL )
	{
		
		// Release all memory/objects associated with the reserved fields.
		if ( fPowerDownNotifier != NULL )
		{
			
			// remove() will also call release() on this object (IONotifier).
			// See IONotifier.h for more info.
			fPowerDownNotifier->remove ( );
			fPowerDownNotifier = NULL;
			
		}
		
		IOFree ( reserved, sizeof ( ExpansionData ) );
		reserved = NULL;
		
	}
	
	if ( fDeviceIdentifyBuffer != NULL )
	{
		
		fDeviceIdentifyBuffer->complete ( );
		fDeviceIdentifyBuffer->release ( );
		fDeviceIdentifyBuffer = NULL;
		
	}
	
	super::free ( );
	
}


//---------------------------------------------------------------------------
// � inspectDevice - Fetch information about the ATA device nub.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver_PD::inspectDevice ( IOATADevice * ataDevice )
{
	
	OSString *		string			= NULL;
	IOReturn		theErr			= kIOReturnSuccess;
	
	// Fetch ATA device information from the nub.
	string = OSDynamicCast ( 	OSString,
								ataDevice->getProperty ( kATAVendorPropertyKey ) );
	
	if ( string != NULL )
	{
		
		strncpy ( fModel, string->getCStringNoCopy ( ), kSizeOfATAModelString );
		fModel[kSizeOfATAModelString] = '\0';
		
	}
	
	string = OSDynamicCast ( 	OSString,
								ataDevice->getProperty ( kATARevisionPropertyKey ) );
	
	if ( string != NULL )
	{
		
		strncpy ( fRevision, string->getCStringNoCopy ( ), kSizeOfATARevisionString );
		fRevision[kSizeOfATARevisionString] = '\0';
		
	}
	
	theErr = identifyAndConfigureATADevice ( );
	if ( theErr != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::inspectDevice theErr = %u\n", ( unsigned int ) theErr ) );
		return false;
		
	}	
	
	// Add an OSNumber property indicating the supported features.
	setProperty ( 	kIOATASupportedFeaturesKey,
					fSupportedFeatures,
					sizeof ( fSupportedFeatures ) * 8 );
	
	return true;
	
}


//---------------------------------------------------------------------------
// � reportATADeviceType - Report the type of ATA device (ATA vs. ATAPI).
//---------------------------------------------------------------------------

ataDeviceType
IOATABlockStorageDriver_PD::reportATADeviceType ( void ) const
{
	
	return kATADeviceType;
	
}


//---------------------------------------------------------------------------
// � getDeviceTypeName - Returns the device type.
//---------------------------------------------------------------------------

const char *
IOATABlockStorageDriver_PD::getDeviceTypeName ( void )
{
	
	return kIOBlockStorageDeviceTypeGeneric;
	
}


//---------------------------------------------------------------------------
// � instantiateNub - 	Instantiate an ATA specific subclass of
//						IOBlockStorageDevice
//---------------------------------------------------------------------------

IOService *
IOATABlockStorageDriver_PD::instantiateNub ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::instantiateNub entering.\n" ) );
	
	IOService * nub = new IOATABlockStorageDevice;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::instantiateNub exiting nub = %p.\n", nub ) );
	
	return nub;
	
}


//---------------------------------------------------------------------------
// � createNub - Returns an IOATABlockStorageDeviceNub.
//---------------------------------------------------------------------------

bool
IOATABlockStorageDriver_PD::createNub ( IOService * provider )
{
	
	IOService *		nub = NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::createNub entering.\n" ) );
	
	// Instantiate a generic hard disk nub so a generic driver
	// can match above us.
	nub = instantiateNub ( );
	if ( nub == NULL )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::createNub instantiateNub() failed, returning false\n" ) );
		return false;
		
	}
	
	nub->init ( );
	
	if ( !nub->attach ( this ) )
	{
		
		// panic since the nub can't attach
		PANIC_NOW ( ( "IOATABlockStorageDriver::createNub() unable to attach nub" ) );
		return false;
		
	}
	
	nub->registerService ( );
	
	// The IORegistry retains the nub, so we can release our refcount
	// because we don't need it any more.
	nub->release ( );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::createNub exiting true.\n" ) );
	
	return true;
	
}


//---------------------------------------------------------------------------
// � doAsyncReadWrite - Handles asynchronous read/write requests.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doAsyncReadWrite (
										IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks,
										IOStorageCompletion		completion )
{
	
	IOReturn			ret;
	IOATACommand *		cmd		= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doAsyncReadWrite entering.\n" ) );
	
	cmd = ataCommandReadWrite ( buffer, block, nblks );
	if ( cmd == NULL )
	{
		
		return kIOReturnNoMemory;
		
	}
	
	ret = asyncExecute ( cmd, completion );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doAsyncReadWrite exiting ret = %u.\n", ( unsigned int ) ret ) );
	
	return ret;
	
}



//---------------------------------------------------------------------------
// � doSyncReadWrite - Handles synchronous read/write requests.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doSyncReadWrite (
										IOMemoryDescriptor *	buffer,
										UInt32					block,
										UInt32					nblks )
{

	IOReturn			ret;
	IOATACommand *		cmd 	= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSyncReadWrite entering\n" ) );

	cmd = ataCommandReadWrite ( buffer, block, nblks );
	if ( cmd == NULL )
	{
		
		return kIOReturnNoMemory;
		
	}
	
	ret = syncExecute ( cmd );
		
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSyncReadWrite exiting ret = %u.\n", ( unsigned int ) ret ) );
	
	return ret;
	
}


//---------------------------------------------------------------------------
// � doEjectMedia - Eject the media in the drive.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doEjectMedia ( void )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doEjectMedia called.\n" ) );
	
	if ( fATASocketType == kPCCardSocket )
		return status;
	
	if ( getProperty ( "Power Off" ) != NULL )
	{
		
		// Spin down the media now. We use to queue up a power change but we found
		// that since the PM stuff happens at a deferred point, the machine might shutdown
		// before we finish the PM change. So, we do the spindown now, then sync with PM
		// so it knows the state.
		if ( fCurrentPowerState > kIOATAPowerStateSleep )
		{
			
			// Spin down the drive
			issuePowerTransition ( kATAStandbyDevice );
			
			// NB: Intentionally set to sleep since some devices don't like
			// being put to sleep after standby.
			fCurrentPowerState = kIOATAPowerStateSleep;
			
			// Give the heads a chance to park
			IOSleep ( 500 );
			
		}
		
	}
	
	// Synchronize our state with power management's concept of what
	// our state really is.
	changePowerStateToPriv ( kIOATAPowerStateSleep );
	
	return status;
	
}


//---------------------------------------------------------------------------
// � doFormatMedia -	Format the media in the drive. ATA devices do not
//						support low level formatting.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doFormatMedia ( UInt64 byteCapacity )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doFormatMedia called.\n" ) );
	return kIOReturnUnsupported;
	
}


//---------------------------------------------------------------------------
// � doGetFormatCapacities - Returns disk capacity.
//---------------------------------------------------------------------------

UInt32
IOATABlockStorageDriver_PD::doGetFormatCapacities ( UInt64 * 	capacities,
												 UInt32		capacitiesMaxCount ) const
{
		
	if ( ( capacities != NULL ) && ( capacitiesMaxCount > 0 ) )
	{
		
		if ( fUseExtendedLBA )
		{
			
			UInt32	hiLBA = 0;
			UInt32	loLBA = 0;
			
			IOATADevConfig::sDriveExtendedLBASize ( &hiLBA, &loLBA, fDeviceIdentifyData );
			*capacities = ( ( UInt64 ) hiLBA ) << 32 | loLBA;
			
		}
		
		else if ( fUseLBAAddressing )
		{
			
			*capacities = ( fDeviceIdentifyData[kATAIdentifyLBACapacity + 1] << 16 ) +
								( UInt32 ) fDeviceIdentifyData[kATAIdentifyLBACapacity];	
			
		}
		
		else
		{
		
			*capacities = 	fDeviceIdentifyData[kATAIdentifySectorsPerTrack] *
							fDeviceIdentifyData[kATAIdentifyLogicalHeadCount] *
							fDeviceIdentifyData[kATAIdentifyLogicalCylinderCount];
		
		}
		
		*capacities *= kATADefaultSectorSize;
				
		return 1;
	
	}
	
	return 0;
	
}


//---------------------------------------------------------------------------
// � doLockUnlockMedia - Lock the media and prevent a user-initiated eject.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doLockUnlockMedia ( bool doLock )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doLockUnlockMedia called.\n" ) );
	return kIOReturnUnsupported;	// No removable ATA device support.
	
}


//---------------------------------------------------------------------------
// � doSynchronizeCache - Flush the write-cache to the physical media
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doSynchronizeCache ( void )
{
	
	IOReturn			status 	= kIOReturnSuccess;
	IOATACommand *		cmd 	= NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSynchronizeCache called.\n" ) );
	
	if ( fATASocketType == kPCCardSocket )
	{
		
		// Device doesn�t support flush cache. Don�t send the command.
		fNumCommandsOutstanding--;
		status = kIOReturnSuccess;
		goto Exit;
		
	}
	
	cmd = ataCommandFlushCache ( );
	
	// Do we have a valid command?
	if ( cmd == NULL )
	{
		
		// Return no memory error.
		fNumCommandsOutstanding--;
		status = kIOReturnNoMemory;
		goto Exit;
		
	}
	
	// Send the command to flush the cache.
	status = syncExecute ( cmd, kATATimeout1Minute, 0 );	

Exit:
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doSynchronizeCache returning status = %u.\n", ( unsigned int ) status ) );
	
	return status;
	
}


//---------------------------------------------------------------------------
// � doStart - Handle a Start Unit command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doStart ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doStart called.\n" ) );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � doStop - Handle a Stop Unit command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::doStop ( void )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::doStop called.\n" ) );
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � getAdditionalDeviceInfoString - Return additional identification strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver_PD::getAdditionalDeviceInfoString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getAdditionalDeviceInfoString called.\n" ) );
	return ( "[ATA]" );
	
}


//---------------------------------------------------------------------------
// � getProductString - Return product identification strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver_PD::getProductString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getProductString called.\n" ) );
	return fModel;
	
}


//---------------------------------------------------------------------------
// � getRevisionString - Return product revision strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver_PD::getRevisionString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getRevisionString called.\n" ) );
	return fRevision;
	
}


//---------------------------------------------------------------------------
// � getVendorString - Return product vendor strings.
//---------------------------------------------------------------------------

char *
IOATABlockStorageDriver_PD::getVendorString ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::getVendorString called.\n" ) );
	return NULL;
	
}


//---------------------------------------------------------------------------
// � reportBlockSize - 	Report the device block size in bytes. This is
//						ALWAYS 512-bytes
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportBlockSize ( UInt64 * blockSize )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportBlockSize called.\n" ) );
	
	*blockSize = kATADefaultSectorSize;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportEjectability - 	Report the media in the ATA device as 
//							non-ejectable unless it is in the PC Card socket.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportEjectability ( bool * isEjectable )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportEjectability called.\n" ) );
	
	*isEjectable = (fATASocketType == kPCCardSocket);
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportLockability - Fixed media, locking is invalid.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportLockability ( bool * isLockable )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportLockability called.\n" ) );
	
	*isLockable = false;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportPollRequirements - 	Report the polling requirements for
//								removable media.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportPollRequirements ( 	bool * pollRequired,
													bool * pollIsExpensive )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportPollRequirements called.\n" ) );
	
	*pollIsExpensive	= false;
	*pollRequired		= false;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportMaxReadTransfer - 	Report the max number of bytes transferred for
//								an ATA read command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportMaxReadTransfer ( UInt64 blocksize, UInt64 * max )
{
	
	OSNumber *		size;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxReadTransfer called.\n" ) );
	
	*max = blocksize * kIOATAMaximumBlockCount8Bit;
	
	size = OSDynamicCast ( OSNumber, getProperty ( kIOMaximumBlockCountReadKey ) );
	if ( size != NULL )
	{
		
		*max = size->unsigned64BitValue ( ) * blocksize;
		
	}
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxReadTransfer max = %llu.\n", *max ) );
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportMaxWriteTransfer - 	Report the max number of bytes transferred for
//								an ATA write command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportMaxWriteTransfer ( UInt64 blocksize, UInt64 * max )
{
	
	OSNumber *		size;

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxWriteTransfer called.\n" ) );	
	
	*max = blocksize * kIOATAMaximumBlockCount8Bit;
	
	size = OSDynamicCast ( OSNumber, getProperty ( kIOMaximumBlockCountWriteKey ) );
	if ( size != NULL )
	{
		
		*max = size->unsigned64BitValue ( ) * blocksize;
		
	}
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportMaxValidBlock - Returns the maximum addressable sector number
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportMaxValidBlock ( UInt64 * maxBlock )
{
	
	UInt64		diskCapacity = 0;
	
	assert ( fATADevice && maxBlock );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxValidBlock called.\n" ) );
	
	doGetFormatCapacities ( &diskCapacity, 1 );
	
	*maxBlock = ( diskCapacity / kATADefaultSectorSize ) - 1;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMaxValidBlock maxBlock = %llu.\n", *maxBlock ) );
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportMediaState - Report whether the media is currently present, and
//						whether a media change has been registered since the
//						last reporting.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportMediaState ( bool * mediaPresent, bool * changed )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportMediaState called.\n" ) );

	*mediaPresent	= true;
	*changed		= true;
	
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportRemovability - Report whether the media is removable.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportRemovability ( bool * isRemovable )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportRemovability called.\n" ) );
	
	*isRemovable = (fATASocketType == kPCCardSocket);
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � reportWriteProtection - Report if the media is write-protected.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reportWriteProtection ( bool * isWriteProtected )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::reportWriteProtection called.\n" ) );

	*isWriteProtected = false;
	return kIOReturnSuccess;
	
}


//---------------------------------------------------------------------------
// � getWriteCacheState - Gets the write cache state.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::getWriteCacheState ( bool * enabled )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::getWriteCacheState called.\n" ) );

	if ( activityTickle ( kIOPMSuperclassPolicy1, ( UInt32 ) kIOATAPowerStateActive ) )
	{
		
		status = identifyATADevice ( );
		
		if ( status == kIOReturnSuccess )
		{
			
			// Get write cache enabled bit. It's in word 85, bit 5
			*enabled = fDeviceIdentifyData[kATAIdentifyCommandExtension2] & kATAWriteCacheEnabledMask;
			
		}
	
	}
	
	else
	{
		
		status = kIOReturnNotResponding;
		
	}
	
	return status;
	
}


//---------------------------------------------------------------------------
// � setWriteCacheState - Report if the media is write-protected.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::setWriteCacheState ( bool enabled )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::setWriteCacheState called.\n" ) );
	
	if ( activityTickle ( kIOPMSuperclassPolicy1, ( UInt32 ) kIOATAPowerStateActive ) )
	{
		
		status = ataCommandSetFeatures (
								enabled ? kATAEnableWriteCache : kATADisableWriteCache,
								0,
								0,
								0,
								0,
								mATAFlagImmediate,
								true );
		
	}
	
	else
	{
		
		status = kIOReturnNotResponding;
		
	}
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	� sendSMARTCommand	-	Sends a SMART command to the device
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::sendSMARTCommand ( IOATACommand * command )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	if ( activityTickle ( kIOPMSuperclassPolicy1, ( UInt32 ) kIOATAPowerStateActive ) )
	{
		
		command->setUnit ( fATAUnitID );
		command->setDevice_Head ( fATAUnitID << 4 );
		
		status = fATADevice->executeCommand ( command );
		if ( status == kATANoErr )
			status = kIOReturnSuccess;
		else
			status = kIOReturnIOError;
		
	}
	
	else
	{
		
		status = kIOReturnNotResponding;
		
	}
	
	return status;
	
}


//---------------------------------------------------------------------------
// � message - Handles messages from our provider.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::message ( UInt32 type, IOService * provider, void * argument )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::message %p %x\n", this, (unsigned int)type ) );

	switch ( type )
	{
		
		case kATAResetEvent:					// Someone gave a hard reset to the drive
			SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::message reset happened\n" ) );
			// Reconfig device here
			fWakeUpResetOccurred = true;
			status = reconfigureATADevice ( );
			
			// Need to set power state to standby mode, since that is what
			// we are in after a reset.
			if ( fPowerManagementInitialized == true )
				activityTickle ( kIOPMSuperclassPolicy1, ( UInt32 ) kIOATAPowerStateActive );
			break;
		
		case kATANullEvent:						// Just kidding -- nothing happened

		// media bay, PC-Card events the drive has gone away and there's nothing 
		// that can be done about it.	
		case kATAOnlineEvent:					// An ATA device has come online
		case kATAOfflineEvent:					// An ATA device has gone offline
		case kATARemovedEvent:					// An ATA device has been removed from the bus
		
		// on earlier hardware, the system is able to lock the PC Card slot so we can 
		// get a request that the driver can then refuse gracefully.
		case kATAOfflineRequest:				// Someone requesting to offline the drive
		case kATAEjectRequest:					// Someone requesting to eject the drive

		// atapi resets are not relevent to ATA devices, but soft-resets ARE relevant to ATAPI devices.
		case kATAPIResetEvent:					// Someone gave a ATAPI reset to the drive
			break;
		
		case kIOMessageServiceIsRequestingClose:
		{
			
			STATUS_LOG ( ("%s: kIOMessageServiceIsRequestingClose Received\n", getName( ) ) );
            
            // tell clients to to away, terminate myself
            terminate ( kIOServiceRequired );             
			fATADevice->close ( this );
			status = kIOReturnSuccess;
			
		}
			break;
		
		default:
			status = super::message ( type, provider, argument );
			break;
	}

	
	return status;
	
}


#pragma mark -
#pragma mark Static Functions


//--------------------------------------------------------------------------------------
//	� sSwapBytes16	-	swaps the buffer for device identify data
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sSwapBytes16 ( UInt8 * buffer, IOByteCount numBytesToSwap )
{
	
	IOByteCount		index;
	UInt8			temp;
	UInt8 *			firstBytePtr;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::swapBytes16 called.\n" ) );
	
	for ( index = 0; index < numBytesToSwap; index += 2 )
	{
		
		firstBytePtr 	= buffer;				// save pointer
		temp 			= *buffer++;			// Save Byte0, point to Byte1
		*firstBytePtr 	= *buffer;				// Byte0 = Byte1
		*buffer++		= temp;					// Byte1 = Byte0
		
	}
	
}


//--------------------------------------------------------------------------------------
//	� sConvertHighestBitToNumber	-	Finds the higest bit in a number and returns
//--------------------------------------------------------------------------------------

UInt8
IOATABlockStorageDriver_PD::sConvertHighestBitToNumber ( UInt16 bitField )
{
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::convertHighestBitToNumber called.\n" ) );

	UInt16  index, integer;
	
	// Test all bits from left to right, terminating at the first non-zero bit
	for ( index = 0x0080, integer = 7; ( ( index & bitField ) == 0 && index != 0 ) ; index >>= 1, integer-- )
	{ ; }
	
	return integer;
	
}


// binary compatibility reserved method space
OSMetaClassDefineReservedUsed ( IOATABlockStorageDriver_PD, 1 );	/* sendSMARTCommand */

OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 2 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 3 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 4 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 5 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 6 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 7 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 8 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 9 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 10 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 11 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 12 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 13 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 14 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 15 );
OSMetaClassDefineReservedUnused ( IOATABlockStorageDriver_PD, 16 );


//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------
