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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "IOSCSITargetDeviceHashTable.h"
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSITargetHashTable"

#if DEBUG
#define SCSI_TARGET_HASH_TABLE_DEBUGGING_LEVEL				0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_TARGET_HASH_TABLE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_TARGET_HASH_TABLE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_TARGET_HASH_TABLE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x; IOSleep(1)
#else
#define STATUS_LOG(x)
#endif


#define super __OSHashTable


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Globals
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

static IOSCSITargetDeviceHashTable gSCSITargetDeviceHashTable;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	GetSharedInstance - Gets pointer to global hash table	   [PUBLIC][STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOSCSITargetDeviceHashTable *
IOSCSITargetDeviceHashTable::GetSharedInstance ( void )
{
	return &gSCSITargetDeviceHashTable;
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	IsProviderPathToExistingTarget -
//	
//	Checks if a node is undiscovered yet. If so, adds it to the global hash
//	table, else it adds another path to the target. Returns true if node is a
//	new PATH, false if it is a new target device.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSITargetDeviceHashTable::IsProviderPathToExistingTarget (
									IOSCSITargetDevice *		newTarget,
									IOSCSIProtocolServices *	provider,
									UInt32						hashValue )
{
	
	bool						isNewPath		= false;
	__OSHashEntry *				newEntry		= NULL;
	__OSHashEntryBucket *		bucket			= NULL;
	__OSHashEntry *				existingEntry	= NULL;
	IOSCSITargetDevice *		existingTarget	= NULL;
	
	STATUS_LOG ( ( "+IOSCSITargetDeviceHashTable::IsProviderPathToExistingTarget\n" ) );
	
	// Allocate the OSHashEntry here. You don't want to allocate while holding
	// the table lock, as allocations may block.
	newEntry = IONew ( __OSHashEntry, 1 );
	require_nonzero ( newEntry, ErrorExit );
	
	// Initialize the OSHashEntry structure.
	newEntry->hashValue 	= hashValue;
	newEntry->next			= NULL;
	newEntry->prev			= NULL;
	newEntry->object		= newTarget;
	hashValue 				= hashValue % fSize;
	
	STATUS_LOG ( ( "hashValue = 0x%08x\n", ( unsigned int ) hashValue ) );
	
	// Get the table lock.
	Lock ( );
	
	// Find the hash bucket.
	bucket			= &fTable[hashValue];
	existingEntry	= bucket->firstEntry;
	
	// Walk the list of hash entries in this bucket (if any). Check if any entries
	// match our entry. If so, add a path to the existing entry and bail. Else,
	// we insert the new entry into the table and associate the hash entry with the
	// target device.
	if ( existingEntry != NULL )
	{
		
		// Get the object for this entry.
		existingTarget = ( IOSCSITargetDevice * ) existingEntry->object;
		
		// Check the target object for existence.
		while ( existingTarget != NULL )
		{
			
			STATUS_LOG ( ( "Looking for target device\n" ) );
			
			OSObject *	id1 = NULL;
			OSObject *	id2 = NULL;
			
			id1 = existingTarget->GetNodeUniqueIdentifier ( );
			id2 = newTarget->GetNodeUniqueIdentifier ( );
			
			// See if the node unique identifiers are the same. We make the assumption that any
			// path to the same target device should get the same INQUIRY page 83h and page 80h
			// information from that target. So, the node unique identifiers would be equal for
			// any two paths to the same node.
			if ( ( id1 != NULL ) && ( id2 != NULL ) && ( id1->isEqualTo ( id2 ) ) )
			{
				
				// They match. Add the path.
				STATUS_LOG ( ( "Adding path to target\n" ) );
				existingTarget->AddPath ( provider );
				isNewPath = true;
				break;
				
			}
			
			// They didn't match. See if we should keep traversing this bucket.
			if ( existingEntry->next == NULL )
				break;
			
			// Advance to next entry.
			existingEntry 	= existingEntry->next;
			existingTarget	= ( IOSCSITargetDevice * ) existingEntry->object;
			
		}
		
	}
	
	// Is this a new target device?
	if ( isNewPath == false )
	{
		
		// Yes, this is a new target device. Insert it into the table.
		STATUS_LOG ( ( "Adding new target to hash table\n" ) );
		newTarget->SetHashEntry ( newEntry );
		InsertHashEntry ( newEntry );
		
	}
	
	Unlock ( );
	
	// Did we add a path to an existing target?
	if ( isNewPath == true )
	{
		
		// Yes. Free the memory for the hash entry, since we'll use the
		// one in existence already.
		IODelete ( newEntry, __OSHashEntry, 1 );
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-IOSCSITargetDeviceHashTable::IsProviderPathToExistingTarget: isNewPath = %s\n", isNewPath ? "true" : "false" ) );
	
	return isNewPath;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	DestroyHashReference -  Removes a hash entry from the table when a target
//					  		device is freed.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSITargetDeviceHashTable::DestroyHashReference ( void * oldEntry )
{
	
	STATUS_LOG ( ( "+IOSCSITargetDeviceHashTable::DestroyHashReference\n" ) );
	
	super::RemoveHashEntry ( ( __OSHashEntry * ) oldEntry );
	IODelete ( oldEntry, __OSHashEntry, 1 );
	
	STATUS_LOG ( ( "-IOSCSITargetDeviceHashTable::DestroyHashReference\n" ) );
	
}