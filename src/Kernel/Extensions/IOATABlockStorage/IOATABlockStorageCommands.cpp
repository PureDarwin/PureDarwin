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
#include <IOKit/IOSyncer.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/storage/IOMedia.h>

#include "IOATABlockStorageDriver.h"

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

// Media icon keys
#define kCFBundleIdentifierKey				"CFBundleIdentifier"
#define kIOATABlockStorageIdentifierKey		"com.apple.iokit.IOATABlockStorage"
#define kPCCardIconFileKey					"PCCard.icns"

// Configuration state machine
enum
{
	kPIOTransferModeSetup	= 1,
	kPIOTransferModeDone	= 2,
	kDMATransferModeDone	= 3,
	kReadAheadEnableDone	= 4,
	kWriteCacheEnableDone	= 5
};

struct ATAConfigData
{
    IOATABlockStorageDriver_PD *	self;
	UInt32						state;
	IOSyncer *					syncer;
};
typedef struct ATAConfigData ATAConfigData;


//--------------------------------------------------------------------------------------
//	е identifyATADevice	-	Sends a device identify request to the device
//							and uses it to configure the drive speeds
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::identifyATADevice ( void )
{

	IOReturn			status 		= kIOReturnSuccess;
	IOATACommand *		cmd			= NULL;
	ATAClientData *		clientData	= NULL;
		
	STATUS_LOG ( ( "IOATABlockStorageDriver::identifyATADevice entering.\n" ) );
		
	// Get a new command object
	cmd = getATACommandObject ( );
	assert ( cmd != NULL );
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	// Zero the command object
	cmd->zeroCommand ( );
	
	// Start filling in the command
	cmd->setUnit ( fATAUnitID );
	
	sSetCommandBuffers ( cmd, fDeviceIdentifyBuffer, 0, 1 ); 
	
	cmd->setCommand ( kATAcmdDriveIdentify );
	cmd->setFlags ( mATAFlagIORead ); 
	cmd->setOpcode ( kATAFnExecIO );
	// set the device head to the correct unit
	cmd->setDevice_Head ( fATAUnitID << 4 );
	cmd->setRegMask ( ( ataRegMask ) ( mATAErrFeaturesValid | mATAStatusCmdValid ) );

	// Save state data
	clientData->command	= kATAcmdDriveIdentify;
	clientData->flags	= mATAFlagIORead;
	clientData->opCode	= kATAFnExecIO;
	clientData->regMask	= ( ataRegMask ) ( mATAErrFeaturesValid | mATAStatusCmdValid );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::identifyATADevice executing identify command.\n" ) );
	
	status = syncExecute ( cmd, kATATimeout10Seconds );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::identifyATADevice result = %u.\n", (unsigned int) cmd->getResult ( ) ) );
		return status;
	
	}
	
	// Validate the identify data before the swap!
	status = sValidateIdentifyData ( ( UInt8 * ) fDeviceIdentifyData );
	
	#if defined(__BIG_ENDIAN__)
		// The identify device info needs to be byte-swapped on big-endian (ppc) 
		// systems becuase it is data that is produced by the drive, read across a 
		// 16-bit little-endian PCI interface, directly into a big-endian system.
		// Regular data doesn't need to be byte-swapped because it is written and 
		// read from the host and is intrinsically byte-order correct.	
		sSwapBytes16 ( ( UInt8 * ) fDeviceIdentifyData, 512 );
	#endif
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::identifyATADevice exiting with status = %u.\n", (unsigned int) status ) );
	
	return status;
		
}


//--------------------------------------------------------------------------------------
//	е setupReadWriteTaskFile	-	Setup the command's taskfile registers based on
//									parameters given
//	block - Initial transfer block
//	nblks - Number of blocks to transfer
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::setupReadWriteTaskFile (
							IOATACommand *			cmd,
							IOMemoryDescriptor *	buffer,
							UInt32					block,
							UInt32					nblks )
{
	
	ATAClientData *	clientData;
	UInt32			flags		= mATAFlagUseConfigSpeed;
	UInt8			command		= 0;
	bool			isWrite		= ( buffer->getDirection ( ) == kIODirectionOut );

	STATUS_LOG ( ( "IOATABlockStorageDriver::setupReadWriteTaskFile entering.\n" ) );
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	// Zero the command
	cmd->zeroCommand ( );
	
	// Check if we are capable of DMA for the I/O
	if ( fUltraDMAMode || fDMAMode )
	{
		
		if ( fUseExtendedLBA )
		{
			
			command = ( isWrite ) ? kATAcmdWriteDMAExtended : kATAcmdReadDMAExtended;
			flags |= mATAFlag48BitLBA;
			
		}
		
		else
		{	
			
			command = ( isWrite ) ? kATAcmdWriteDMA : kATAcmdReadDMA;
			
		}
		
		flags |= mATAFlagUseDMA;
		
	}
	
	else
	{
		
		if ( fUseExtendedLBA )
		{
			
			command = ( isWrite ) ? kATAcmdWriteExtended : kATAcmdReadExtended;
		
		}
		
		else
		{	
			
			command = ( isWrite ) ? kATAcmdWrite : kATAcmdRead;
			
		}
		
	}
	
	flags |= ( isWrite ) ? mATAFlagIOWrite : mATAFlagIORead;
	
	// Set the ATA Command
	cmd->setCommand ( command );
	cmd->setSectorCount ( ( nblks == kIOATAMaxBlocksPerXfer ) ? 0 : nblks );
	
	if ( fUseLBAAddressing )
	{
		
		if ( fUseExtendedLBA )
		{
			
			IOExtendedLBA *		extLBA = cmd->getExtendedLBA ( );
			
			extLBA->setExtendedLBA ( 0, block, fATAUnitID, ( UInt16 ) nblks, command );
			
			clientData->useExtendedLBA 	= true;
			clientData->lbaLow16 		= extLBA->getLBALow16 ( );
			clientData->lbaMid16		= extLBA->getLBAMid16 ( );
			clientData->lbaHigh16		= extLBA->getLBAHigh16 ( );
			clientData->sectorCount16	= extLBA->getSectorCount16 ( );
			clientData->features16		= extLBA->getFeatures16 ( );
			clientData->device			= extLBA->getDevice ( );
			clientData->command16		= extLBA->getCommand ( );
			
		}
		
		else
		{
			
			STATUS_LOG ( ( "IOATABlockStorageDriver::setupReadWriteTaskFile block = %x.\n",  (unsigned int)block ) );
			
			// 28bit LBA addressing supported
			cmd->setLBA28 ( block, fATAUnitID );
			
		}
		
		clientData->sectorNumber 	= ( block & 0xFF );
		clientData->cylLow			= ( ( block & 0xFF00 ) >> 8 );
		clientData->cylHigh			= ( ( block & 0x00FF0000 ) >> 16 );
		
	}
	
	else
	{
	
		UInt32	heads 	= 0;
		UInt32	sectors = 0;
		
		heads 	= fDeviceIdentifyData[kATAIdentifyLogicalHeadCount];
		sectors	= fDeviceIdentifyData[kATAIdentifySectorsPerTrack];
		
		// Not LBA, so use CHS addressing model
		sConvertBlockToCHSAddress ( cmd, block, heads, sectors, fATAUnitID );
		
	}
	
	sSetCommandBuffers ( cmd, buffer, 0, nblks ); 
	
	cmd->setUnit ( fATAUnitID );
	
	cmd->setFlags ( flags );
	cmd->setOpcode ( kATAFnExecIO );
	
	clientData->command			= command;
	clientData->opCode			= kATAFnExecIO;
	clientData->flags			= flags;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::setupReadWriteTaskFile exiting.\n" ) );
	
}


//--------------------------------------------------------------------------------------
//	е ataCommandReadWrite	-	Setup the command for a read/write operation
//
//	buffer	- IOMemoryDescriptor object describing this transfer.
//	block	- Initial transfer block.
//	nblks	- Number of blocks to transfer.
//--------------------------------------------------------------------------------------

IOATACommand *
IOATABlockStorageDriver_PD::ataCommandReadWrite (
							IOMemoryDescriptor * 	buffer,
							UInt32					block,
							UInt32			   		nblks )
{
	
	IOATACommand * 		cmd 				= NULL;

	STATUS_LOG ( ( "IOATABlockStorageDriver::ataCommandReadWrite entering.\n" ) );
	
	assert ( buffer != NULL );
	
	cmd = getATACommandObject ( );
	
	if ( cmd == NULL )
		return NULL;
	
	// Setup the taskfile structure with the size and direction of the
	// transfer. This structure will be written to the actual taskfile
	// registers when this command is processed.
	setupReadWriteTaskFile ( cmd, buffer, block, nblks );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::ataCommandReadWrite exiting.\n" ) );

	return cmd;
	
}


//--------------------------------------------------------------------------------------
//	е ataCommandSetFeatures	-	Setup the command for a set features operation
//
//	features		- contents of the Features register
//	sectorCount		- contents of Sector Count register
//	sectorNumber	- contents of Sector Number register
//	cylinderLow		- contents of the Cylinder Low register
//	cylinderHigh	- contents of the Cylinder High register
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::ataCommandSetFeatures (
									UInt8 features,
									UInt8 sectorCount,
									UInt8 sectorNumber,
									UInt8 cylinderLow,
									UInt8 cylinderHigh,
									UInt32 flags,
									bool forceSync )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::ataCommandSetFeatures entering.\n" ) );
		
	// Zero the command
	fConfigurationCommand->zeroCommand ( );
	
	fConfigurationCommand->setUnit ( fATAUnitID );	
	fConfigurationCommand->setFeatures ( features );
	fConfigurationCommand->setSectorCount ( sectorCount );
	fConfigurationCommand->setSectorNumber ( sectorNumber );
	fConfigurationCommand->setOpcode ( kATAFnExecIO );
	fConfigurationCommand->setCommand ( kATAcmdSetFeatures );
	fConfigurationCommand->setDevice_Head ( fATAUnitID << 4 );
	fConfigurationCommand->setCylHi ( cylinderHigh );
	fConfigurationCommand->setCylLo ( cylinderLow );
	fConfigurationCommand->setFlags ( flags );
	fConfigurationCommand->setTimeoutMS ( kATATimeout10Seconds );
	
	if ( forceSync )
	{
		fConfigurationCommand->setCallbackPtr ( &IOATABlockStorageDriver_PD::sATAVoidCallback );
	}
	
	else
	{
		fConfigurationCommand->setCallbackPtr ( &IOATABlockStorageDriver_PD::sATAConfigStateMachine );
	}
	
	status = fATADevice->executeCommand ( fConfigurationCommand );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::ataCommandSetFeatures exiting.\n" ) );
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е ataCommandFlushCache	-	Setup the command for a flush cache operation
//--------------------------------------------------------------------------------------

IOATACommand *
IOATABlockStorageDriver_PD::ataCommandFlushCache ( void )
{
	
	IOATACommand * 		cmd 		= NULL;
	ATAClientData *		clientData	= NULL;

	STATUS_LOG ( ( "IOATABlockStorageDriver::ataCommandFlushCache entering.\n" ) );

	cmd = getATACommandObject ( );
	
	if ( cmd == NULL )
		return NULL;
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	// Zero the command
	cmd->zeroCommand ( );
	
	if ( fDeviceIdentifyData[kATAIdentifyCommandSetSupported2] & kATASupportsFlushCacheExtendedMask )
	{
		
		IOExtendedLBA *		extLBA = cmd->getExtendedLBA ( );
		
		extLBA->setExtendedLBA ( 0, 0, fATAUnitID, 0, kATAcmdFlushCacheExtended );
		cmd->setFlags ( mATAFlag48BitLBA );
		
		clientData->useExtendedLBA 	= true;
		clientData->lbaLow16 		= extLBA->getLBALow16 ( );
		clientData->lbaMid16		= extLBA->getLBAMid16 ( );
		clientData->lbaHigh16		= extLBA->getLBAHigh16 ( );
		clientData->sectorCount16	= extLBA->getSectorCount16 ( );
		clientData->features16		= extLBA->getFeatures16 ( );
		clientData->device			= extLBA->getDevice ( );
		clientData->command16		= extLBA->getCommand ( );
		
	}
	
	cmd->setOpcode ( kATAFnExecIO );
	cmd->setCommand ( kATAcmdFlushCache );
	cmd->setDevice_Head ( fATAUnitID << 4 );
	cmd->setUnit ( fATAUnitID );	
	
	clientData->command	= kATAcmdFlushCache;
	clientData->opCode	= kATAFnExecIO;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::ataCommandFlushCache exiting.\n" ) );
	
	return cmd;
	
}


//---------------------------------------------------------------------------
// Issue a synchronous ATA command.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::syncExecute (
						IOATACommand *			cmd,
						UInt32					timeout,
						UInt8					retries )
{
	
	ATAClientData * 	clientData 			= NULL;
	IOReturn			status				= kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::syncExecute entering.\n" ) );
	
	clientData = ( ATAClientData * ) cmd->refCon;
	assert ( clientData != NULL );
	
	cmd->setTimeoutMS ( timeout );
	cmd->setCallbackPtr ( &IOATABlockStorageDriver_PD::sHandleCommandCompletion );
		
	// Set up for synchronous transaction
	clientData->completion.syncLock = IOSyncer::create ( );
	assert ( clientData->completion.syncLock != NULL );
	
	if ( clientData->completion.syncLock == NULL )
	{
		returnATACommandObject ( cmd );
		return kIOReturnNoResources;
	}
	
	// set up any client data necessary for the command
	clientData->isSync 		= true;
	clientData->self 		= this;
	clientData->maxRetries 	= retries;
	clientData->timeout		= timeout;
	
	// save the state data in case we need to issue retries
	sSaveStateData ( cmd );
	
	fATADevice->executeCommand ( cmd );
	
	// Block client thread on lock until the completion handler
	// receives an indication that the processing is complete
	clientData->completion.syncLock->wait ( );

	STATUS_LOG ( ( "IOATABlockStorageDriver::syncExecute exiting.\n" ) );
	
	// Pull the error from the clientData
	status = clientData->returnCode;
		
	// Re-enqueue the command object since we're done with it
	returnATACommandObject ( cmd );
	
	return status;
	
}


//---------------------------------------------------------------------------
// Issue an asynchronous ATA command.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::asyncExecute (
							IOATACommand *	  		cmd,
							IOStorageCompletion 	completion,
							UInt32			  		timeout,
							UInt8					retries )
{
	
	ATAClientData * 	clientData 			= NULL;
	IOReturn			status				= kIOReturnSuccess;

	STATUS_LOG ( ( "IOATABlockStorageDriver::asyncExecute entering.\n" ) );
	
	clientData = ( ATAClientData * ) cmd->refCon;
	assert ( clientData != NULL );
		
	// Set timeout and register the completion handler.
	cmd->setTimeoutMS ( timeout );
    cmd->setCallbackPtr( &IOATABlockStorageDriver_PD::sHandleCommandCompletion );
		
	// Set up for an asynchronous transaction
	clientData->isSync				= false;
	clientData->self 				= this;
	clientData->maxRetries 			= retries;
	clientData->completion.async 	= completion;
	clientData->timeout				= timeout;

	// save the state data in case we need to issue retries
	sSaveStateData ( cmd );
	
	// Execute the command
	status = fATADevice->executeCommand ( cmd );
	if ( status != kATANoErr )
	{
		
		bzero ( clientData, sizeof ( ATAClientData ) );
		returnATACommandObject ( cmd );
		
	}
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е allocateATACommandObjects	-	allocates ATA command objects
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::allocateATACommandObjects ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::allocateATACommandObjects entering\n" ) );
	
	IOATACommand *			cmd 		= NULL;
	ATAClientData *			clientData 	= NULL;
	IOSyncer *				syncer		= NULL;
	ATAConfigData *			configData	= NULL;
			
	for ( UInt32 index = 0; index < fNumCommandObjects; index++ )
	{
		
		// Allocate the command
		cmd = fATADevice->allocCommand ( );
		assert ( cmd != NULL );
		
		// Allocate the command clientData
		clientData = ( ATAClientData * ) IOMalloc ( sizeof ( ATAClientData ) );
		assert ( clientData != NULL );
		bzero ( clientData, sizeof ( ATAClientData ) );
				
		// set the back pointers to each other
		cmd->refCon 	= ( void * ) clientData;
		clientData->cmd	= cmd;
		
		STATUS_LOG ( ( "adding command to pool\n" ) );
		
		// Enqueue the command in the free list
		fCommandPool->returnCommand ( cmd );
		
	}
	
	// Allocate the command
	fResetCommand 			= fATADevice->allocCommand ( );
	fPowerManagementCommand	= fATADevice->allocCommand ( );
	fConfigurationCommand	= fATADevice->allocCommand ( );
	
	assert ( fResetCommand != NULL );
	assert ( fPowerManagementCommand != NULL );
	assert ( fConfigurationCommand != NULL );
	
	syncer = IOSyncer::create ( );
	assert ( syncer != NULL );
	fPowerManagementCommand->refCon = ( void * ) syncer;
	
	configData = ( ATAConfigData * ) IOMalloc ( sizeof ( ATAConfigData ) );
	assert ( configData != NULL );
	bzero ( configData, sizeof ( ATAConfigData ) );
	configData->syncer = IOSyncer::create ( );
	assert ( configData->syncer != NULL );
	fConfigurationCommand->refCon = ( void * ) configData;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::allocateATACommandObjects exiting\n" ) );
	
}


//--------------------------------------------------------------------------------------
//	е dellocateATACommandObjects	-	deallocates ATA command objects
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::deallocateATACommandObjects ( void )
{

	STATUS_LOG ( ( "IOATABlockStorageDriver::dellocateATACommandObjects entering\n" ) );	
	
	IOATACommand *		cmd 		= NULL;
	ATAClientData *		clientData 	= NULL;
	IOSyncer *			syncer		= NULL;
	ATAConfigData *		configData	= NULL;
	
	cmd = ( IOATACommand * ) fCommandPool->getCommand ( false );
	assert ( cmd != NULL );

	//еее Walk the in-use queue and abort the commands (potential memory leak right now)
	
	
	// This handles walking the free command queue
	while ( cmd != NULL )
	{
		
		clientData = ( ATAClientData * ) cmd->refCon;
		assert ( clientData != NULL );
		
		IOFree ( clientData, sizeof ( ATAClientData ) );
		clientData = NULL;
		
		fATADevice->freeCommand ( cmd );
		
		cmd = ( IOATACommand * ) fCommandPool->getCommand ( false );
		
	}
	
	syncer = ( IOSyncer * ) fPowerManagementCommand->refCon;
	syncer->release ( );
		
	configData = ( ATAConfigData * ) fConfigurationCommand->refCon;
	configData->syncer->release ( );
	IOFree ( configData, sizeof ( ATAConfigData ) );
	
	fATADevice->freeCommand ( fResetCommand );
	fATADevice->freeCommand ( fPowerManagementCommand );
	fATADevice->freeCommand ( fConfigurationCommand );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::dellocateATACommandObjects exiting.\n" ) );	
	
}


//--------------------------------------------------------------------------------------
//	е getATACommandObject	-	Gets the first non-busy ata command object
//								If they are all busy, it returns NULL
//--------------------------------------------------------------------------------------

IOATACommand *
IOATABlockStorageDriver_PD::getATACommandObject ( bool blockForCommand )
{
	
	IOATACommand *	cmd = NULL;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::getATACommandObject entering.\n" ) );
	
	cmd = ( IOATACommand * ) fCommandPool->getCommand ( blockForCommand );
	assert ( cmd != NULL );
	
	return cmd;
	
}


//--------------------------------------------------------------------------------------
//	е returnATACommandObject	-	returns the ATA command object to the pool
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::returnATACommandObject ( IOATACommand * cmd )
{
	
	ATAClientData *		clientData;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::returnATACommandObject entering.\n" ) );

	assert ( cmd != NULL );
	clientData = ( ATAClientData * ) cmd->refCon;
	assert ( clientData != NULL );
	
	// Clear out the clientData, so we don't reuse any variables
	bzero ( clientData, sizeof ( ATAClientData ) );
	
	// set the back pointers to each other
	cmd->refCon 	= ( void * ) clientData;
	clientData->cmd	= cmd;
	
	// Finally, re-enqueue the command
	fCommandPool->returnCommand ( cmd );
	
}


//--------------------------------------------------------------------------------------
//	е identifyAndConfigureATADevice	-	Sends a device identify request to the device
//										and uses it to configure the drive speeds
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::identifyAndConfigureATADevice ( void )
{
	
	IOReturn			status				= kIOReturnSuccess;
	IOATABusInfo *		busInfoPtr			= NULL;
	IOATADevConfig *	deviceConfigPtr		= NULL;
	UInt16				tempWord			= 0;
	UInt16				maxBlocks			= 0;
	UInt64				maxSize				= 0;
	
	// Get some info about the ATA bus
	busInfoPtr = IOATABusInfo::atabusinfo ( );
	if ( busInfoPtr == NULL )
		return kIOReturnNoResources;
	
	// Zero the data, and then get the information from the ATA controller
	busInfoPtr->zeroData ( );
	status = fATADevice->provideBusInfo ( busInfoPtr );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice provide bus info failed thErr = %u.\n", (unsigned int) status ) );
		goto ReleaseBusInfoAndBail;
		
	}
		
	// Get the socket type
	fATASocketType = busInfoPtr->getSocketType ( );
	STATUS_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice socket type = %d.\n", ( UInt8 ) fATASocketType ) );
	
	// Set the media icon to PC-Card if it is a PC Card device
	if ( fATASocketType == kPCCardSocket )
	{
		
		OSDictionary *	dict = OSDictionary::withCapacity ( 2 );
		
		if ( dict != NULL )
		{
			
			OSString *	identifier 		= OSString::withCString ( kIOATABlockStorageIdentifierKey );
			OSString *	resourceFile	= OSString::withCString ( kPCCardIconFileKey );
			
			if ( ( identifier != NULL ) && ( resourceFile != NULL ) )
			{
			
				dict->setObject ( kCFBundleIdentifierKey, identifier );
				dict->setObject ( kIOBundleResourceFileKey, resourceFile );
				
				setProperty ( kIOMediaIconKey, dict );
			
			}
			
			if ( identifier != NULL )
				identifier->release ( );
			
			if ( resourceFile != NULL )
				resourceFile->release ( );
			
			dict->release ( );
			
		}
		
	}
	
	// Execute an ATA device identify
	status = identifyATADevice ( );
	if ( status != kIOReturnSuccess )
	{
		goto ReleaseBusInfoAndBail;		
	}
	
	// Parse the identify data
	tempWord = fDeviceIdentifyData[kATAIdentifyCommandSetSupported];
	
	// Check for S.M.A.R.T.
	if ( ( tempWord & kATASupportsSMARTMask ) == kATASupportsSMARTMask )
		fSupportedFeatures |= kIOATAFeatureSMART;
	
	// Check for power management
	if ( ( tempWord & kATASupportsPowerManagementMask ) == kATASupportsPowerManagementMask )
		fSupportedFeatures |= kIOATAFeaturePowerManagement;
	
	// Check for write cache
	if ( ( tempWord & kATASupportsWriteCacheMask ) == kATASupportsWriteCacheMask )
		fSupportedFeatures |= kIOATAFeatureWriteCache;
	
	tempWord = fDeviceIdentifyData[kATAIdentifyCommandSetSupported2];
	if ( ( tempWord & kATADataIsValidMask ) == 0x4000 )
	{
		
		// Check for APM
		if ( ( tempWord & kATASupportsAdvancedPowerManagementMask ) == kATASupportsAdvancedPowerManagementMask )
			fSupportedFeatures |= kIOATAFeatureAdvancedPowerManagement;
		
		// Check for CF
		if ( ( tempWord & kATASupportsCompactFlashMask ) == kATASupportsCompactFlashMask )
			fSupportedFeatures |= kIOATAFeatureCompactFlash;
		
	}
	
	// Sanity check. CHS is rarely used, but it is still good to do things correctly.
	if ( fDeviceIdentifyData[kATAIdentifyDriveCapabilities] & kLBASupportedMask )
		fUseLBAAddressing = true;
	
	// Check for ExtendedLBA (48-bit) support from the bus.
	fUseExtendedLBA = busInfoPtr->supportsExtendedLBA ( );
	
	// Mask with drive support
	fUseExtendedLBA &= IOATADevConfig::sDriveSupports48BitLBA ( fDeviceIdentifyData );
	if ( fUseExtendedLBA )
	{
		
		maxBlocks = busInfoPtr->maxBlocksExtended ( );
		
		// 48-bit LBA supported
		fSupportedFeatures |= kIOATAFeature48BitLBA;
		
	}
	
	else
	{
		
		// Not using 48-bit LBA, so use normal constraints
		maxBlocks = kIOATAMaximumBlockCount8Bit;
		
	}
	
	maxSize = ( UInt64 ) maxBlocks * ( UInt64 ) kATADefaultSectorSize;
	
	// Publish some constraints
	setProperty ( kIOMaximumBlockCountReadKey, maxBlocks, 64 );
	setProperty ( kIOMaximumBlockCountWriteKey, maxBlocks, 64 );
	setProperty ( kIOMaximumSegmentCountReadKey, maxSize / PAGE_SIZE, 64 );
	setProperty ( kIOMaximumSegmentCountWriteKey, maxSize / PAGE_SIZE, 64 );
	
	deviceConfigPtr = IOATADevConfig::atadevconfig ( );
	assert ( deviceConfigPtr != NULL );
	
	// Get the device config from ATA Controller
	status = fATADevice->provideConfig ( deviceConfigPtr );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice provideConfig returned an error = %u.\n", (unsigned int) status ) );
		goto ReleaseBusInfoAndBail;
		
	}
	
	// Pass the bus information and device identify info to have ATA Controller choose
	// the best config for this device (if it can)
	status = deviceConfigPtr->initWithBestSelection ( fDeviceIdentifyData, busInfoPtr );
	if ( status != kIOReturnSuccess )
	{
		goto ReleaseBusInfoAndBail;		
	}
	
	// Select the config it gives us
	status = fATADevice->selectConfig ( deviceConfigPtr );
	if ( status != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice selectConfig returned error = %u.\n", (unsigned int) status ) );
		
	}
	
	// Store the PIOModes, DMAModes and UltraDMAModes supported
	fPIOMode 		= deviceConfigPtr->getPIOMode ( );
	fDMAMode		= deviceConfigPtr->getDMAMode ( );
	fUltraDMAMode 	= deviceConfigPtr->getUltraMode ( );
	
	// Add any more configuration info we need (write cache enable, power management, etc.)
	status = configureATADevice ( );
	STATUS_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice configureATADevice returned status = %u.\n", (unsigned int) status ) );
	
	
ReleaseBusInfoAndBail:
	
	
	if ( busInfoPtr != NULL )
	{
		
		STATUS_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice releasing bus info.\n" ) );
		busInfoPtr->release ( );
		busInfoPtr = NULL;
		
	}
	
	if ( deviceConfigPtr != NULL )
	{
		
		STATUS_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice releasing device config.\n" ) );
		deviceConfigPtr->release ( );
		deviceConfigPtr = NULL;
		
	}
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::identifyAndConfigureATADevice returning status = %u.\n", (unsigned int) status ) );
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е configureATADevice	-	Configures the ATA Device
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::configureATADevice ( void )
{
	
	ATAConfigData *		configData;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::configureATADevice entering.\n" ) );
	
	configData = ( ATAConfigData * ) fConfigurationCommand->refCon;
	
	configData->self 		= this;
	configData->state 		= kPIOTransferModeSetup;

	// Commands issued by the configuration state machine can complete before
	// executeCommand() returns (notably with QEMU's PIIX IDE controller). Arm
	// the syncer before starting the chain so an immediate final callback is
	// not erased by a later reinit(), which would leave start() blocked forever.
	configData->syncer->reinit ( );

	sATAConfigStateMachine ( fConfigurationCommand );
	configData->syncer->wait ( false );
	
	return kIOReturnSuccess;
	
}


//--------------------------------------------------------------------------------------
//	е reconfigureATADevice	-	Reconfigures the ATA Device
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::reconfigureATADevice ( void )
{
		
	// There is a window here on a machine with both device0 and device1 ATA
	// devices where if a device is not ready and a bus reset occurs, and the
	// other driver hasn't allocated resources yet but is instantiated, it
	// could cause a panic. We check to make sure the fConfigurationCommand
	// instance variable is not NULL to make sure all resources necessary
	// for reconfiguration have been allocated before proceeding with
	// the reconfiguration.
	
	if ( fConfigurationCommand != NULL )
	{
	
		// We are on a callout from the ATA controller and need to execute
		// all of these commands synchronously.	
		setPIOTransferMode ( true );
		setDMATransferMode ( true );
		
		// Check if a client has previously set the APM level. If so,
		// reconfigure the device with that value.
		if ( fAPMLevel != 0xFF )
		{
			setAdvancedPowerManagementLevel ( fAPMLevel, true );
		}
		
		ataCommandSetFeatures ( kATAEnableReadAhead,
								0,
								0,
								0,
								0,
								mATAFlagImmediate,
								true );
		
		ataCommandSetFeatures (
							kATAEnableWriteCache,
							0,
							0,
							0,
							0,
							mATAFlagImmediate,
							true );
		
	}
		
	return kIOReturnSuccess;
	
}


//--------------------------------------------------------------------------------------
//	е setPIOTransferMode	-	Configures the ATA Device's PIO Transfer mode
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::setPIOTransferMode ( bool forceSync )
{
	
	IOReturn		status 		= kIOReturnSuccess;
	UInt8			mode		= 0;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::setPIOTransferMode entering.\n" ) );
	
	// Always set to highest transfer mode
	mode = sConvertHighestBitToNumber ( fPIOMode );
	
	// PIO transfer mode is capped at 4 for now in the ATA-5 spec. If a device supports
	// more than mode 4 it has to at least support mode 4. We might not get the best
	// performance out of the drive, but it will work until we update to latest spec.
	if ( mode > 4 )
	{
		
		STATUS_LOG ( ( "IOATABlockStorageDriver::setPIOTransferMode mode > 4 = %u.\n", (unsigned int) mode ) );
		mode = 4;
		
	}
	
	status = ataCommandSetFeatures ( kATASetTransferMode,
									 ( kATAEnablePIOModeMask | mode ),
									 0,
									 0,
									 0,
									 mATAFlagImmediate,
									 forceSync );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::setPIOTransferMode exiting with error = %u.\n", (unsigned int) status ) );
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е setDMATransferMode	-	Configures the ATA Device's DMA Transfer mode
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::setDMATransferMode ( bool forceSync )
{
	
	IOReturn		status 			= kIOReturnSuccess;
	UInt8			mode			= 0;
	UInt8 			sectorCount		= 0;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::SetDMATransferMode entering.\n" ) );
	
	// Always set to highest transfer mode
	if ( fUltraDMAMode )
	{
		
		// Device supports UltraDMA
		STATUS_LOG ( ( "IOATABlockStorageDriver::SetDMATransferMode choosing UltraDMA.\n" ) );
		mode = sConvertHighestBitToNumber ( fUltraDMAMode );
		
		// Ultra DMA is capped at 8 for now in the ATA-5 spec. If a device supports
		// more than mode 8 it MUST at least support mode 8.
		if ( mode > 8 )
			mode = 8;
		
		sectorCount = ( kATAEnableUltraDMAModeMask | mode );
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "IOATABlockStorageDriver::SetDMATransferMode choosing DMA.\n" ) );
		mode = sConvertHighestBitToNumber ( fDMAMode );
		
		// MultiWord DMA is capped at 2 for now in the ATA-5 spec. If a device supports
		// more than mode 2 it MUST at least support mode 2. We might not get the best
		// performance out of the drive, but it will work until we update to latest spec.
		if ( mode > 2 )
			mode = 2;
		
		sectorCount	= ( kATAEnableMultiWordDMAModeMask | mode );
		
	}
	
	status = ataCommandSetFeatures ( kATASetTransferMode,
									 sectorCount,
									 0,
									 0,
									 0,
									 mATAFlagImmediate,
									 forceSync );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::SetDMATransferMode exiting with error = %u.\n", (unsigned int) status ) );
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е setAdvancedPowerManagementLevel	-	Configures the ATA Device's APM Level mode
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::setAdvancedPowerManagementLevel ( UInt8 level, bool forceSync )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::setAdvancedPowerManagementLevel entering.\n" ) );
	
	fAPMLevel = level;
	
	// First, see if we need to do the operation at all
	if ( ( fSupportedFeatures & kIOATAFeatureAdvancedPowerManagement ) == 0 )
		return status;
	
	status = ataCommandSetFeatures ( kATAEnableAPM,
									 fAPMLevel,
									 0,
									 0,
									 0,
									 mATAFlagImmediate,
									 forceSync );
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::setAdvancedPowerManagementLevel exiting with error = %u.\n", (unsigned int) status ) );
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е sATAVoidCallback	-	callback that does nothing
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sATAVoidCallback ( IOATACommand * cmd )
{
	return;
}


//--------------------------------------------------------------------------------------
//	е sATAConfigStateMachine	-	state machine for configuration commands
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sATAConfigStateMachine ( IOATACommand * cmd )
{
	
	ATAConfigData *					configData;
	IOATABlockStorageDriver_PD *		driver;
		
	STATUS_LOG ( ( "IOATABlockStorageDriver::sATAConfigStateMachine entering\n" ) );	
	
	configData 	= ( ATAConfigData * ) cmd->refCon;
	driver		= configData->self;
	
	switch ( configData->state )
	{
		
		case kPIOTransferModeSetup:
			configData->state = kPIOTransferModeDone;
			driver->setPIOTransferMode ( false );
			break;
			
		case kPIOTransferModeDone:
			if ( ( driver->fUltraDMAMode != 0 ) || ( driver->fDMAMode != 0 ) )
			{
				configData->state = kDMATransferModeDone;
				driver->setDMATransferMode ( false );
				break;
			}			
		
		// Intentional fall through in case there is no DMA mode available
		
		case kDMATransferModeDone:
		{
			configData->state = kReadAheadEnableDone;
			driver->ataCommandSetFeatures ( kATAEnableReadAhead,
											0,
											0,
											0,
											0,
											mATAFlagImmediate,
											false );
		}	
			break;
			
		case kReadAheadEnableDone:
		{
			configData->state = kWriteCacheEnableDone;
			driver->ataCommandSetFeatures ( kATAEnableWriteCache,
											0,
											0,
											0,
											0,
											mATAFlagImmediate,
											false );
		}	
			break;
			
		case kWriteCacheEnableDone:
			// we're done with configuration
			configData->syncer->signal ( kIOReturnSuccess, false );
			break;
			
		default:
			PANIC_NOW ( ( "sATAPIConfigStateMachine unexpected state\n" ) );
			break;
		
	}
		
}


//--------------------------------------------------------------------------------------
//	е resetATADevice	-	Sends a device reset command to the device
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::resetATADevice ( void )
{
	
	IOReturn		status		= kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::resetATADevice entering.\n" ) );
	SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::resetATADevice executing reset.\n" ) );
	
	if ( fResetInProgress )
		return kIOReturnNotPermitted;
	
	fResetInProgress = true;
	
	// Zero the command object
	fResetCommand->zeroCommand ( );
	
	// Start filling in the command
	fResetCommand->setUnit ( fATAUnitID );	
	fResetCommand->setTimeoutMS ( kATATimeout45Seconds );
	fResetCommand->setOpcode ( kATAFnBusReset );
	fResetCommand->setFlags ( mATAFlagImmediate );
	fResetCommand->setCallbackPtr ( &IOATABlockStorageDriver_PD::sHandleReset );
	
	fResetCommand->refCon = ( void * ) this;
	
	status = fATADevice->executeCommand ( fResetCommand );
	if ( status != kATANoErr )
	{
		
		// There was a problem selecting the device for reset. This is
		// where we panic the system and let them know that the ATA
		// device has wedged. Eventually, we will make this better so
		// that non-backing store devices will be able to tear down the
		// driver stack instead of panic'ing the system.
		panic ( "ATA Disk: Error when attempting to reset device, status = 0x%08x\n", status );
		
	}
	
	fResetInProgress = false;
	
	return status;
	
}


#pragma mark -
#pragma mark Static Routines


//--------------------------------------------------------------------------------------
//	е sHandleCommandCompletion		-	This routine is called by our provider when a
//										command processing has completed
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sHandleCommandCompletion ( IOATACommand * cmd )
{
	
	ATAClientData		clientData 			= { 0 };
	UInt32				result				= kATANoErr;
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::sHandleCommandCompletion entering.\n" ) );
	
	assert ( cmd != NULL );
	
	bcopy ( cmd->refCon, &clientData, sizeof ( clientData ) );
	
	result = cmd->getResult ( );
	
	// Return IOReturn for sync commands.
	switch ( result )
	{
		
		case kATANoErr:
			STATUS_LOG ( ( "IOATABlockStorageDriver::sHandleCommandCompletion actual xfer = %u.\n", (unsigned int) cmd->getActualTransfer ( ) ) );
			clientData.returnCode = kIOReturnSuccess;
			break;
			
		case kATATimeoutErr:
		case kATAErrDevBusy:
			
			// Reset the device because the device is hung
			clientData.self->resetATADevice ( );			
			
			// Intentional fall through
		
		case kATADeviceError:
			
			ERROR_LOG ( ( "IOATABlockStorageDriver::sHandleCommandCompletion result = %u.\n", (unsigned int)result ) );
			
			// Reissue the command if the max number of retries is greater
			// than zero, else return an error
			if ( clientData.maxRetries > 0 )
			{
				sReissueCommandFromClientData ( cmd );
				return;
			}				
			break;
			
		default:
			break;
		
	}
	
	clientData.self->fNumCommandsOutstanding--;
	
	if ( clientData.isSync )
	{
		
		clientData.returnCode = result;
		// For sync commands, unblock the client thread.
		assert ( clientData.completion.syncLock );
		clientData.completion.syncLock->signal ( );
		
	}
	
	else
	{
		
		UInt64	bytesTransferred = 0;
		
		if ( clientData.self == NULL )
		{
			panic ( "ATA controller called me and my completion data is NULL" );
		}
		
		bytesTransferred = ( UInt64 ) cmd->getActualTransfer ( );
		
		// Async command processing is complete, re-enqueue command
		clientData.self->returnATACommandObject ( cmd );
		
		// Signal the completion routine that the request has been completed.
		IOStorage::complete ( &clientData.completion.async,
							  result,
							  bytesTransferred );
		
	}
	
	STATUS_LOG ( ( "IOATABlockStorageDriver::sHandleCommandCompletion exiting.\n" ) );
	
}


//---------------------------------------------------------------------------
// е sHandleSimpleSyncTransaction - callback routine for simple synchronous
//									commands
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sHandleSimpleSyncTransaction ( IOATACommand * cmd )
{
	
	IOReturn		status;
	IOSyncer *		syncer;
		
	syncer	= ( IOSyncer * ) cmd->refCon;
	status	= cmd->getResult ( );
		
	syncer->signal ( status, false );
		
}


//---------------------------------------------------------------------------
// е sHandleReset - callback routine for reset commands
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sHandleReset ( IOATACommand * cmd )
{
	
	IOATABlockStorageDriver_PD *	self;
	
	if ( cmd->getResult ( ) == kATANoErr )
	{
		
		self = ( IOATABlockStorageDriver_PD * ) cmd->refCon;
		self->fWakeUpResetOccurred = true;
		
	}
	
}


//--------------------------------------------------------------------------------------
//	е sConvertBlockToCHSAddress	-	Converts a block to a valid CHS address and stuffs
//									it into the command
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sConvertBlockToCHSAddress ( IOATACommand *	cmd,
													 UInt32 		block,
													 UInt32 		heads,
													 UInt32 		sectors,
													 ataUnitID 		unitID )
{

	ATAClientData *		clientData;
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	UInt16	startingCylinder 	= block / ( sectors * heads );
	UInt16	whichHead			= ( block / sectors ) % heads;
	
	cmd->setSectorNumber ( ( block % sectors ) + 1 );	// Sectors range from 1╔n
	cmd->setCylLo ( ( UInt8 ) startingCylinder );
	cmd->setCylHi ( ( UInt8 ) ( startingCylinder >> 8 ) );
	cmd->setDevice_Head ( ( ( ( UInt8 ) unitID ) << 4 ) | ( whichHead & mATAHeadNumber ) );
	
	// Save extra state data
	clientData->sectorNumber 	= ( block % sectors ) + 1;
	clientData->cylLow			= ( UInt8 ) startingCylinder;
	clientData->cylHigh			= ( UInt8 ) ( startingCylinder >> 8 );
	
}


//--------------------------------------------------------------------------------------
//	е sReissueCommandFromClientData	-	Reissue a transaction
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sReissueCommandFromClientData ( IOATACommand * cmd )
{
	
	ATAClientData *		clientData;
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	if ( clientData == NULL )
	{
		
		PANIC_NOW ( ( "No client data associated with this command.\n" ) );
		return;
		
	}
	
	clientData->maxRetries--;
	
	// Zero the command
	cmd->zeroCommand ( );
	
	// Rebuild the ATA command based on saved register state
	cmd->setOpcode 				( clientData->opCode );
	cmd->setFlags 				( clientData->flags );
	cmd->setUnit 				( clientData->self->fATAUnitID );
	cmd->setTimeoutMS 			( clientData->timeout );
	cmd->setCallbackPtr			( &IOATABlockStorageDriver_PD::sHandleCommandCompletion );
	cmd->setRegMask				( clientData->regMask );
	cmd->setBuffer 				( clientData->buffer );
	cmd->setPosition 			( clientData->bufferOffset );
	cmd->setByteCount 			( clientData->numberOfBlocks * kATADefaultSectorSize );
	cmd->setTransferChunkSize 	( kATADefaultSectorSize );
	cmd->setFeatures 			( clientData->featuresReg );
	cmd->setSectorCount 		( clientData->sectorCount );	
	cmd->setSectorNumber 		( clientData->sectorNumber );	
	cmd->setCylLo 				( clientData->cylLow );
	cmd->setCylHi 				( clientData->cylHigh );
	cmd->setDevice_Head			( clientData->sdhReg );
	cmd->setCommand 			( clientData->command );

	if ( clientData->useExtendedLBA )
	{
		
		IOExtendedLBA *		extLBA = cmd->getExtendedLBA ( );
		
		extLBA->setLBALow16 ( clientData->lbaLow16 );
		extLBA->setLBAMid16 ( clientData->lbaMid16 );
		extLBA->setLBAHigh16 ( clientData->lbaHigh16 );
		extLBA->setSectorCount16 ( clientData->sectorCount16 );
		extLBA->setFeatures16 ( clientData->features16 );
		extLBA->setDevice ( clientData->device );
		extLBA->setCommand ( clientData->command16 );
		
	}
	
	clientData->self->fATADevice->executeCommand ( cmd );
	
	STATUS_LOG ( ( "%ssReissueCommandFromClientData%s\n", "\033[33m", "\033[0m" ) );
	
}


//--------------------------------------------------------------------------------------
//	е sSetCommandBuffers	-	Builds an ATA command based on passed in arguments
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sSetCommandBuffers ( IOATACommand *		cmd,
											  IOMemoryDescriptor * 	buffer,
											  IOByteCount			offset,
											  IOByteCount			numBlocks )
{
	
	ATAClientData *		clientData;
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	cmd->setBuffer 				( buffer );
	cmd->setPosition 			( offset );
	cmd->setByteCount 			( numBlocks * kATADefaultSectorSize );
	cmd->setTransferChunkSize 	( kATADefaultSectorSize );
	
	clientData->buffer 			= buffer;
	clientData->bufferOffset 	= (UInt32) offset;
	clientData->numberOfBlocks 	= (UInt32) numBlocks;
	
}


//--------------------------------------------------------------------------------------
//	е sSaveStateData	-	Saves state data for executing commands
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver_PD::sSaveStateData ( IOATACommand * cmd )
{
	
	ATAClientData *		clientData;
	
	clientData = ( ATAClientData * ) cmd->refCon;
	
	clientData->sectorCount		= cmd->getSectorCount ( );
	clientData->sectorNumber	= cmd->getSectorNumber ( );
	clientData->cylHigh			= cmd->getCylHi ( );
	clientData->cylLow			= cmd->getCylLo ( );
	clientData->sdhReg			= cmd->getDevice_Head ( );
	
}


//--------------------------------------------------------------------------------------
//	е sValidateIdentifyData	-	Validates identify data by checksumming it
//--------------------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver_PD::sValidateIdentifyData ( UInt8 * deviceIdentifyData )
{
	
	IOReturn	status		= kIOReturnSuccess;
	UInt8		checkSum 	= 0;
	UInt32		index		= 0;
	UInt16		offset		= kATAIdentifyIntegrity * sizeof ( UInt16 );
	
	if ( deviceIdentifyData[offset] == kChecksumValidCookie )
	{
		
		for ( index = 0; index < 512; index++ )
			checkSum += deviceIdentifyData[index];
		
		if ( checkSum != 0 )
		{
			
			IOLog ( "ATA Disk: Identify data is incorrect - bad checksum\n" );
			
		}
		
	}
	
	return status;
	
}
