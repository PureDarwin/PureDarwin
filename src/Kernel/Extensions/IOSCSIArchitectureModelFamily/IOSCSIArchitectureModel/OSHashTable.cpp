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

#include "OSHashTable.h"
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <IOKit/IOLib.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"OSHashTable"

#if DEBUG
#define OS_HASH_TABLE_DEBUGGING_LEVEL						0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( OS_HASH_TABLE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( OS_HASH_TABLE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( OS_HASH_TABLE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


// FNV (Fowler/Noll/Vo) Prime (32-bit) constant
#define kFNV_32_PRIME ((UInt32) 0x01000193UL)


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Hash - Does FNV hash on passed in OSData bytes.					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
__OSHashTable::Hash ( OSData * data ) const
{
	
	const UInt8 *	bytes 	= NULL;
	UInt32			hash	= 0;
	UInt32			length	= 0;
	
	STATUS_LOG ( ( "+__OSHashTable::Hash(data)\n" ) );
	
	require_nonzero ( data, ErrorExit );
	
	bytes = ( const UInt8 * ) data->getBytesNoCopy ( );
	require_nonzero ( bytes, ErrorExit );
	
	length = data->getLength ( );
	require_nonzero ( length, ErrorExit );
	
	// Perform hash
	while ( length != 0 )
	{
		
		hash *= kFNV_32_PRIME;
		hash ^= *bytes++;
		
		length--;
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-__OSHashTable::Hash(data)\n" ) );
	
	return hash;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Hash - Does FNV hash on passed in OSString.						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
__OSHashTable::Hash ( OSString * string ) const
{
	
	const UInt8 *	bytes 	= NULL;
	UInt32			hash	= 0;
	UInt32			c		= 0;
	
	STATUS_LOG ( ( "+__OSHashTable::Hash(string)\n" ) );
	
	require_nonzero ( string, ErrorExit );
	
	bytes = ( const UInt8 * ) string->getCStringNoCopy ( );
	require_nonzero ( bytes, ErrorExit );
	
	// Perform hash
	c = *bytes;
	while ( c != 0 )
	{
		
		hash *= kFNV_32_PRIME;
		hash ^= c;
		
		bytes++;
		c = *bytes;
		
	}
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-__OSHashTable::Hash(string)\n" ) );
	
	return hash;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Default Constructor
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

__OSHashTable::__OSHashTable ( void )
{
	
	STATUS_LOG ( ( "+__OSHashTable::__OSHashTable(void)\n" ) );
	
	fTableLock = IORecursiveLockAlloc ( );
	
	fSize 			= kDefaultStartSize;
	fEntries		= 0;
	fMaxChainDepth	= 0;
	
	fTable = IONew ( __OSHashEntryBucket, fSize );
	bzero ( fTable, fSize * sizeof ( __OSHashEntryBucket ) );
	
	STATUS_LOG ( ( "-__OSHashTable::__OSHashTable(void)\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constructor
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

__OSHashTable::__OSHashTable ( UInt32 startSize )
{
	
	STATUS_LOG ( ( "+__OSHashTable::__OSHashTable(UInt32)\n" ) );
	
	fTableLock = IORecursiveLockAlloc ( );
		
	fSize 			= startSize;
	fEntries		= 0;
	fMaxChainDepth	= 0;
	
	fTable = IONew ( __OSHashEntryBucket, fSize );
	bzero ( fTable, fSize * sizeof ( __OSHashEntryBucket ) );
	
	STATUS_LOG ( ( "-__OSHashTable::__OSHashTable(UInt32)\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Destructor
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

__OSHashTable::~__OSHashTable ( void )
{
	
	STATUS_LOG ( ( "+__OSHashTable::~__OSHashTable\n" ) );
	
	if ( fTableLock != NULL )
	{
		
		IORecursiveLockFree ( fTableLock );
		fTableLock = NULL;
		
	}
	
	if ( fTable != NULL )
	{
		
		IODelete ( fTable, __OSHashEntryBucket, fSize );
		fTable = NULL;
		
	}
	
	STATUS_LOG ( ( "-__OSHashTable::~__OSHashTable\n" ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	InsertHashEntry - Called to insert an entry into the hash table.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
__OSHashTable::InsertHashEntry ( __OSHashEntry * newEntry )
{
	
	__OSHashEntryBucket * 	header		= NULL;
	UInt32					hashValue	= 0;
	
	Lock ( );
	
	hashValue = newEntry->hashValue % fSize;
	header = &fTable[hashValue];
	
	newEntry->next = header->firstEntry;
	newEntry->prev = NULL;
	
	if ( header->firstEntry != NULL )
	{
		header->firstEntry->prev = newEntry;
	}
	header->firstEntry = newEntry;
	header->chainDepth++;
	
	STATUS_LOG ( ( "__OSHashTable::InsertHashEntry, bucket = %ld, chainDepth = %ld\n", hashValue, header->chainDepth ) );
	
	fEntries++;
	if ( fEntries > ( fSize / 2 ) )
	{
		STATUS_LOG ( ( "Should grow hash table\n" ) );
	}
	
	Unlock ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	RemoveHashEntry - Called to remove an entry from the hash table.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
__OSHashTable::RemoveHashEntry ( __OSHashEntry * oldEntry )
{
	
	__OSHashEntry *			next 		= NULL;
	__OSHashEntry *			prev 		= NULL;
	__OSHashEntryBucket * 	header		= NULL;
	UInt32					hashValue	= 0;
	
	Lock ( );

	hashValue = oldEntry->hashValue % fSize;	
	header = &fTable[hashValue];
	
	prev = oldEntry->prev;
	next = oldEntry->next;
	
	if ( prev != NULL )
	{
		prev->next = next;
	}
	
	else
	{
		header->firstEntry = next;
	}
	
	if ( next != NULL )
	{
		next->prev = prev;
	}
	header->chainDepth--;
	
	STATUS_LOG ( ( "__OSHashTable::RemoveHashEntry, bucket = %ld, chainDepth = %ld\n", hashValue, header->chainDepth ) );
	
	fEntries--;
	if ( fEntries < ( fSize / 8 ) )
	{
		STATUS_LOG ( ( "Should shrink hash table\n" ) );
	}
	
	Unlock ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Rehash - Called to grow or shrink the hash table and balance it out.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
__OSHashTable::Rehash ( void )
{
	
	__OSHashEntry *				listHead			= NULL;
	__OSHashEntryBucket *		newTable			= NULL;
	__OSHashEntryBucket *		oldTable			= NULL;
	UInt32						newSize				= 0;
	UInt32						oldSize				= 0;
	
	// The Rehash() method does the following:
	// ¥ Allocate new memory associated with the table of pointers to buckets.
	// ¥ Make everything into a single doubly-linked list by making
	//	 the tail of each bucket point at the head of the next bucket over.
	// ¥ Rehash each entry and drop it in its proper bucket.
	// ¥ Delete the memory associated with the old table of pointers to buckets.
	
	// Easy check to find out if we're growing or shrinking.
	if ( fEntries < ( fSize / 8 ) )
	{
		newSize = fSize / kScaleFactor;
	}
	
	else
	{
		newSize = fSize * kScaleFactor;
	}
	
	// We now know the new table size. Attempt to allocate memory for new table of
	// pointers.
	newTable = IONew ( __OSHashEntryBucket, newSize );
	require_nonzero ( newTable, ErrorExit );
	
	bzero ( newTable, newSize * sizeof ( __OSHashEntryBucket ) );
	
	// Must hold the table lock to go further.
	Lock ( );
	
	// Rearrange in single list. Keep track of list head.
	listHead = SingleList ( );
	require_nonzero_action ( listHead,
							 ErrorExit,
							 Unlock ( );
							 IODelete ( newTable, __OSHashEntryBucket, newSize ) );
	
	// Switch the tables.
	oldTable 	= fTable;
	oldSize 	= fSize;
	fTable 		= newTable;
	fSize 		= newSize;
	fEntries	= 0;
	
	// Put all the items in the new table.
	RehashList ( listHead );
	
	// Drop the lock now, we don't need it anymore.
	Unlock ( );
	
	// Delete memory associated with old table.
	IODelete ( oldTable, __OSHashEntryBucket, oldSize );
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	SingleList - Arranges items into a single doubly-linked list.
//	NB: This method must be called with fTableLock held.			  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

__OSHashEntry *
__OSHashTable::SingleList ( void ) const
{
	
	__OSHashEntryBucket	* 		bucket				= NULL;
	__OSHashEntry *				firstBucket			= NULL;
	__OSHashEntry *				lastEntryInBucket	= NULL;
	UInt32						index				= 0;
	UInt32						bucketIndex			= 0;
	
	// Find the first bucket with objects in it.
	index = FindFirstBucketWithEntries ( );
	require ( ( index < fSize ), Exit );
	
	bucket = &fTable[index];
	firstBucket = bucket->firstEntry;
	require_nonzero ( firstBucket, Exit );
	
	// Loop over all buckets to connect the hash entries into one giant linked-list.
	for ( ; index < ( fSize - 1 ); index++ )
	{
		
		// Get the current bucket.
		bucket = &fTable[index];
		if ( bucket->chainDepth == 0 )
			continue;
		
		lastEntryInBucket = bucket->firstEntry;
		for ( bucketIndex = 0; bucketIndex < ( bucket->chainDepth - 1 ); bucketIndex++ )
		{
			
			lastEntryInBucket = lastEntryInBucket->next;
			check ( lastEntryInBucket );
			
		}
		
		// Get pointer to next bucket
		bucketIndex = FindNextBucketWithEntries ( &bucket, index + 1 );
		require ( ( bucketIndex < fSize ), Exit );
		require_nonzero ( bucket, Exit );
		
		lastEntryInBucket->next = bucket->firstEntry;
		bucket->firstEntry->prev = lastEntryInBucket;
		
	}
	
	
Exit:
	
	
	return firstBucket;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	RehashList - Drops the list of items into table via InsertHashEntry().
//	NB: This method must be called with fTableLock held.			  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
__OSHashTable::RehashList ( __OSHashEntry * listHead )
{
	
	__OSHashEntry	*	next	= NULL;
	__OSHashEntry	*	prev	= NULL;
	
	prev = listHead;
	next = prev->next;
	InsertHashEntry ( prev );
	
	while ( next != NULL )
	{
		
		prev = next;
		next = prev->next;
		InsertHashEntry ( prev );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FindFirstBucketWithEntries - Finds first bucket with entries.	  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
__OSHashTable::FindFirstBucketWithEntries ( void ) const
{
	
	UInt32 					index 	= 0;
	__OSHashEntryBucket *	bucket	= NULL;
	
	for ( index = 0; index < fSize; index++ )
	{
		
		bucket = &fTable[index];
		if ( bucket->chainDepth != 0 )
		{
			
			break;
			
		}
		
	}
	
	return index;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	FindNextBucketWithEntries - Finds next bucket with entries.		  [PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
__OSHashTable::FindNextBucketWithEntries (
					__OSHashEntryBucket ** 	bucket,
					UInt32					startLocation ) const
{
	
	UInt32 					index 		= 0;
	__OSHashEntryBucket *	localBucket = NULL;
	
	*bucket	= NULL;
	
	for ( index = startLocation; index < fSize; index++ )
	{
		
		localBucket = &fTable[index];
		if ( localBucket->chainDepth != 0 )
		{
			
			*bucket = localBucket;
			break;
			
		}
		
	}
	
	return index;
	
}