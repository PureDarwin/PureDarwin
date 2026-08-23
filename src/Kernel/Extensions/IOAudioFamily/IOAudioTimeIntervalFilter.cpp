/*
 * Copyright (c) 1998-2014 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/IOLib.h>
#include <libkern/c++/OSObject.h>
#include "IOAudioTimeIntervalFilter.h"

#define super OSObject

OSDefineMetaClassAndAbstractStructors(IOAudioTimeIntervalFilter, OSObject)

OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 0);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 1);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 2);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 3);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 4);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 5);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 6);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 7);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 8);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 9);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 10);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 11);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 12);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 13);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 14);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilter, 15);

bool IOAudioTimeIntervalFilter::initFilter(uint32_t expectedInterval, uint32_t multiIntervalCount /* =1 */)
{
	bool result = false;
	mExpectedInterval = expectedInterval;
	mMultiIntervalCount = multiIntervalCount+1;
	mIntervalTimeHistoryPointer = 0;
	mFilterCount = 0;
	
	if ( super::init() )
	{
		mIntervalTimeHistory = (uint64_t*) IONewData ( uint64_t, mMultiIntervalCount);
		if ( NULL == mIntervalTimeHistory ) goto Exit;
		
		timeIntervalLock = IOLockAlloc();
		if ( NULL == timeIntervalLock ) goto Exit;
	}

	result = true;
Exit:
	return result;
}


void IOAudioTimeIntervalFilter::free()
{
	if ( timeIntervalLock )
	{
		IOLockFree ( timeIntervalLock );
		timeIntervalLock = NULL;
	}
	if ( mIntervalTimeHistory )
	{
		IODeleteData( mIntervalTimeHistory, uint64_t, mMultiIntervalCount);
	}
	
	super::free();
}

IOReturn IOAudioTimeIntervalFilter::reInitialiseFilter(uint32_t expectedInterval  /* =0 */, uint32_t multiIntervalCount /* =1 */)
{
	IOReturn result = kIOReturnError;

	if ( NULL == timeIntervalLock ) goto Exit;
	IOLockLock ( timeIntervalLock );

	if ( expectedInterval )
	{
		mExpectedInterval = expectedInterval;
	}
	else
	{
		// If the user didn't supply an expected interval, they are assuming the interval
		// has remained unchanged since the last time the filter was run

		// We can get our last interval through the history buffer
		
		if ( mFilterCount > 1 )
		{
			mExpectedInterval = mIntervalTimeHistory [ decCircularBufferPosition ( mIntervalTimeHistoryPointer ) ] - mIntervalTimeHistory [ decCircularBufferPosition ( mIntervalTimeHistoryPointer, 2 ) ];
		}
	}

	mIntervalTimeHistoryPointer = 0;
	mFilterCount = 0;

	if ( mIntervalTimeHistory )
	{
		IODeleteData ( mIntervalTimeHistory, uint64_t, mMultiIntervalCount);
		mIntervalTimeHistory = NULL;
	}

	mMultiIntervalCount = multiIntervalCount + 1;

	mIntervalTimeHistory = (uint64_t*) IONewData ( uint64_t, mMultiIntervalCount);
	if ( NULL == mIntervalTimeHistory ) goto Exit;
	
	result = kIOReturnSuccess;

Exit:
	if ( timeIntervalLock )
	{
		IOLockUnlock ( timeIntervalLock );
	}

	return result;
}



AbsoluteTime IOAudioTimeIntervalFilter::newTimePosition(AbsoluteTime rawSnapshotAT)
{
	int			n;
	uint64_t	rawSnapshot = rawSnapshotAT;
	uint64_t	filteredSnapshot = 0;
	int			prevPointer;
	
	if ( NULL == timeIntervalLock ) goto Exit;
	if ( NULL == mIntervalTimeHistory ) goto Exit;
	
	IOLockLock ( timeIntervalLock );

	prevPointer = mIntervalTimeHistoryPointer;

	if ( 0 == mFilterCount )
	{
		// The first iteration requires priming of the history
		mIntervalTimeHistory[mIntervalTimeHistoryPointer] = rawSnapshot;
		prevPointer = mIntervalTimeHistoryPointer;

		for (n=0; n<mMultiIntervalCount-1; n++)
		{
			int prevPrevPointer = decCircularBufferPosition(prevPointer);

			mIntervalTimeHistory[prevPrevPointer] = mIntervalTimeHistory[prevPointer] - mExpectedInterval;
			prevPointer = prevPrevPointer;
		}
	}


	filteredSnapshot = calculateNewTimePosition ( rawSnapshot );
	
	// Save the data in our history buffer
	mIntervalTimeHistory[mIntervalTimeHistoryPointer] = filteredSnapshot;

	// Increment our pointer
	mIntervalTimeHistoryPointer = incCircularBufferPosition( mIntervalTimeHistoryPointer );

	mFilterCount++;

	IOLockUnlock ( timeIntervalLock );

Exit:
	return *((AbsoluteTime*) &filteredSnapshot);
}


uint64_t IOAudioTimeIntervalFilter::getMultiIntervalTime(void)
{
	uint64_t value = 0;
	
	if ( NULL == timeIntervalLock ) goto Exit;
	if ( NULL == mIntervalTimeHistory ) goto Exit;

	IOLockLock ( timeIntervalLock );
	
	value = mIntervalTimeHistory [ decCircularBufferPosition ( mIntervalTimeHistoryPointer ) ] - mIntervalTimeHistory [ mIntervalTimeHistoryPointer ];
	
	IOLockUnlock ( timeIntervalLock );

Exit:
	return value;
}






#pragma mark --
#pragma mark IIR

#undef super
#define super IOAudioTimeIntervalFilter

OSDefineMetaClassAndStructors(IOAudioTimeIntervalFilterIIR, IOAudioTimeIntervalFilter)

OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 0);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 1);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 2);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 3);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 4);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 5);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 6);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 7);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 8);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 9);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 10);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 11);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 12);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 13);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 14);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterIIR, 15);

bool IOAudioTimeIntervalFilterIIR::initFilter(uint32_t expectedInterval, uint32_t multiIntervalCount /* =1 */, uint16_t filterCoef /* =4 */)
{
	bool result = IOAudioTimeIntervalFilter::initFilter(expectedInterval, multiIntervalCount);
	
	if ( result )
	{
		mIIRCoef = filterCoef;
	}
	
	return result;
}


uint64_t IOAudioTimeIntervalFilterIIR::calculateNewTimePosition(uint64_t rawSnapshot)
{
	const uint64_t	offset = uint64_t(mExpectedInterval) << mIIRCoef;
	uint64_t		filteredSnapshot;
	
	// Because our filter is initialised with a value prior to the rawSnapshot, there is a possibility that
	// there will be a negative number in our filter. The present math library does not support
	// signed numbers, we add an offset to our snapshot, and remove it from the resulting calculations

	rawSnapshot += offset;

	if ( 0 == mFilterCount )
	{
		// Initialise the filtered snapshot filter. 
		mFilteredSnapshot = (U128(rawSnapshot) -  ( U128( mExpectedInterval ) << mIIRCoef )) << mIIRCoef;
		IIR( &mFilteredSnapshot, U128(rawSnapshot) << mIIRCoef, mIIRCoef );
		
		U128 raw_offset = ( U128(rawSnapshot) << mIIRCoef ) - mFilteredSnapshot;
		
		// Intialise the filtered offset
		mFilteredOffset = UInt64mult(mExpectedInterval, ( 1 << mIIRCoef ) - 1 ) << mIIRCoef;
		
		IIR( &mFilteredOffset, raw_offset, mIIRCoef );
		filteredSnapshot = ((mFilteredSnapshot + mFilteredOffset) >> mIIRCoef).lo;
	}
	else
	{
		IIR( &mFilteredSnapshot, U128(rawSnapshot) << mIIRCoef, mIIRCoef );
		
		U128 raw_offset = ( U128(rawSnapshot) << mIIRCoef ) - mFilteredSnapshot;
		
		IIR( &mFilteredOffset, raw_offset, mIIRCoef );
		filteredSnapshot = ( (mFilteredSnapshot + mFilteredOffset) >> mIIRCoef ).lo;
	}
	
	filteredSnapshot -= offset;

	return filteredSnapshot;
}


void IOAudioTimeIntervalFilterIIR::IIR(U128* filterVal, U128 input, int shift)
{
	U128 x, y;
	
	// IIR of the form:
	//
	// filterVal = ( (2^shiftAmount - 1) / 2^shiftAmount) * filterVal + (1 / 2^shiftAmount) * input
	//

	x =  *filterVal >> shift;
	y = input >> shift;
	*filterVal = *filterVal - x + y;
}


#pragma mark --
#pragma mark FIR

#undef super
#define super IOAudioTimeIntervalFilter


OSDefineMetaClassAndStructors(IOAudioTimeIntervalFilterFIR, IOAudioTimeIntervalFilter)

OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 0);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 1);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 2);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 3);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 4);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 5);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 6);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 7);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 8);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 9);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 10);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 11);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 12);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 13);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 14);
OSMetaClassDefineReservedUnused(IOAudioTimeIntervalFilterFIR, 15);


bool IOAudioTimeIntervalFilterFIR::initFilter(uint32_t expectedInterval, uint32_t multiIntervalCount /* =1 */)
{
	bool result;
	static const uint64_t filterCoefficients[] = {1, 2, 4, 7, 10, 14, 19, 25, 31, 37, 43, 49, 54, 58, 62, 64, 64, 64, 62, 58, 54, 49, 43, 37, 31, 25, 19, 14, 10, 7, 4, 2, 1};

	mDataHistory = NULL;
	mDataOffsetHistory = NULL;
	mNumCoeffs = 0;

	result = IOAudioTimeIntervalFilter::initFilter(expectedInterval, multiIntervalCount);

	if ( result )
	{
		// For now, initialise the filter to our default
		mNumCoeffs = sizeof(filterCoefficients)/sizeof(filterCoefficients[0]);

		if ( kIOReturnSuccess != setNewFilter( mNumCoeffs, filterCoefficients, 10) )
			result = FALSE;
		
		mFilterWritePointer = 0;
	}

	return result;
}

IOReturn IOAudioTimeIntervalFilterFIR::setNewFilter(uint32_t numCoeffs, const uint64_t* filterCoefficients, uint32_t scale)
{
	IOReturn result = kIOReturnError;
	
	// Free up the previous buffers
	if ( mDataHistory )
	{
		IODeleteData ( mDataHistory, uint64_t, mNumCoeffs );
		mDataHistory = NULL;
	}
	if ( mDataOffsetHistory )
	{
		IODeleteData ( mDataOffsetHistory, uint64_t, mNumCoeffs );
		mDataOffsetHistory = NULL;
	}
	if ( mCoeffs )
	{
		IODeleteData ( mCoeffs, uint64_t, mNumCoeffs );
		mCoeffs = NULL;
	}
	
	mNumCoeffs = numCoeffs;

	mCoeffs = (uint64_t*) IONewData ( uint64_t, mNumCoeffs );
	if ( NULL == mCoeffs) goto Exit;
	
	memcpy(mCoeffs, filterCoefficients,  mNumCoeffs * sizeof(uint64_t));
	mFilterScale = scale;
	
	mDataHistory = (uint64_t*) IONewData ( uint64_t, mNumCoeffs );
	if ( NULL == mDataHistory) goto Exit;
	
	mDataOffsetHistory = (uint64_t*) IONewData ( uint64_t, mNumCoeffs );
	if ( NULL == mDataHistory) goto Exit;
	
	reInitialiseFilter ( mExpectedInterval, mMultiIntervalCount );

	result = kIOReturnSuccess;
Exit:
	return result;
}

IOReturn IOAudioTimeIntervalFilterFIR::reInitialiseFilter(uint32_t expectedInterval /* =0 */, uint32_t multiIntervalCount /* =1 */)
{
	IOReturn result = IOAudioTimeIntervalFilter::reInitialiseFilter ( expectedInterval, multiIntervalCount );
	
	mFilterWritePointer = 0;
	
	return result;
}


void IOAudioTimeIntervalFilterFIR::free()
{
	if ( mDataHistory )
	{
		IODeleteData ( mDataHistory, uint64_t, mNumCoeffs );
		mDataHistory = NULL;
	}
	if ( mDataOffsetHistory )
	{
		IODeleteData ( mDataOffsetHistory, uint64_t, mNumCoeffs );
		mDataOffsetHistory = NULL;
	}
	if ( mCoeffs )
	{
		IODeleteData ( mCoeffs, uint64_t, mNumCoeffs );
		mCoeffs = NULL;
	}
	
	IOAudioTimeIntervalFilter::free();
}


uint64_t IOAudioTimeIntervalFilterFIR::calculateNewTimePosition(uint64_t rawSnapshot)
{
	unsigned int	n;
	U128			filteredSnapshot;
	U128			filteredInterval;
	uint64_t		filteredSnapshotFinal;
	
	if ( 0 == mFilterCount )
	{
		// Initialise the filtered snapshot filter.
		for ( n = 0; n < mNumCoeffs; n++ )
		{
			mDataOffsetHistory [ ( mNumCoeffs - n ) % mNumCoeffs ] = (rawSnapshot - UInt64mult(n, mExpectedInterval)).lo;
		}

		filteredSnapshot = FIR( mDataOffsetHistory, rawSnapshot );
		
		U128 raw_offset = rawSnapshot - filteredSnapshot;
		
		// Intialise the filtered offset
		for ( n = 0; n < mNumCoeffs; n++ )
		{
			mDataHistory [ ( mNumCoeffs - n ) % mNumCoeffs ] = uint64_t(mExpectedInterval) * ( mNumCoeffs / 2);
		}
		
		filteredInterval = FIR( mDataHistory, raw_offset.lo );
		filteredSnapshotFinal = (filteredSnapshot + filteredInterval).lo;
	}
	else
	{
		filteredSnapshot = FIR( mDataOffsetHistory, rawSnapshot );

		U128 raw_offset = rawSnapshot - filteredSnapshot;
		
		filteredInterval = FIR( mDataHistory, raw_offset.lo );
		filteredSnapshotFinal = (filteredSnapshot + filteredInterval).lo;		
	}

	// Update the write pointer for the next iteration
	mFilterWritePointer = ( mFilterWritePointer + mNumCoeffs + 1 ) % mNumCoeffs;

	return filteredSnapshotFinal;
}


U128 IOAudioTimeIntervalFilterFIR::FIR(uint64_t *history, uint64_t input)
{
	U128 result128(0);
	unsigned int n;

	history [ mFilterWritePointer ] = input;

	for ( n = 0; n < mNumCoeffs; n++ )
	{
		result128 += UInt64mult ( mCoeffs [ n ] , history [ ( mNumCoeffs + mFilterWritePointer - n ) % mNumCoeffs ] );
	}

	return result128 >> mFilterScale;
}



