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

#ifndef __OS_HASH_TABLE_H__
#define __OS_HASH_TABLE_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <IOKit/IOLocks.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Structs
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

typedef struct __OSHashEntry
{
	__OSHashEntry *	next;
	__OSHashEntry *	prev;
	UInt32			hashValue;
	void *			object;
} __OSHashEntry;

typedef struct __OSHashEntryBucket
{
	__OSHashEntry *	firstEntry;
	UInt32			chainDepth;
} __OSHashEntryBucket;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// This is a private class for internal implementation that should not be
// subclassed by anyone outside IOSCSITargetDeviceHashTable. Eventually, it
// might make its way into libkern if it proves to be a general enough solution.
// 
// This class handles all hash table generic stuff such as inserting/removing
// items, growing or shrinking the table dynamically based on the number of
// entries and/or chain depth. It currently uses the Fowler/Noll/Vo (FNV) hash
// algorithm.
// 
// In the future, we might want to use a different hash algorithm more explictly
// tailored to the 8 bytes of EUI-64/Node World Wide Name, but that
// behavior can be overriden in the IOSCSITargetDeviceHashTable subclass anyway.
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class __OSHashTable
{
	
	static const UInt32	kDefaultStartSize 	= 8;
	static const UInt32 kScaleFactor		= 2;
	
public:
	
	__OSHashTable ( void );
	__OSHashTable ( const UInt32 startSize );
	virtual ~__OSHashTable ( void );
	
	virtual UInt32	Hash ( OSData * data ) const;
	virtual UInt32	Hash ( OSString * string ) const;
	
protected:
	
	void		InsertHashEntry ( __OSHashEntry * entry );
	void		RemoveHashEntry ( __OSHashEntry * entry );
	void		Rehash ( void );
	
	// Table lock/unlock.
	inline void	Lock ( void ) { IORecursiveLockLock ( fTableLock ); }
	inline void	Unlock ( void ) { IORecursiveLockUnlock ( fTableLock ); }
	
private:
	
	// Must call below functions with lock held.
	__OSHashEntry *	SingleList ( void ) const;
	void RehashList ( __OSHashEntry * listHead );
	
	UInt32	FindFirstBucketWithEntries ( void ) const;
	UInt32	FindNextBucketWithEntries ( __OSHashEntryBucket ** 	bucket,
										UInt32					startLocation ) const;
	
	IORecursiveLock *				fTableLock;
	
protected:
	
	UInt32							fSize;
	UInt32							fEntries;
	UInt32							fMaxChainDepth;
	__OSHashEntryBucket *			fTable;
	
};


#endif  /* __OS_HASH_TABLE_H__ */