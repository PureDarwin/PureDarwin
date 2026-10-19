/*
 * Copyright 2013-2022, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 *		Augustin Cavalier <waddlesplash>
 */
#ifndef KERNEL_UTIL_BITMAP_H
#define KERNEL_UTIL_BITMAP_H


#ifdef _KERNEL_MODE
#	include <debug.h>
#else
#	include <Debug.h>
#endif

#include <SupportDefs.h>

namespace BKernel {

class Bitmap {
public:
						Bitmap(size_t bitCount = 0);
						~Bitmap();

			status_t	InitCheck();

			status_t	Resize(size_t bitCount);
			void		Shift(ssize_t bitCount);

	inline	bool		Get(size_t index) const;
	inline	void		Set(size_t index);
	inline	void		Clear(size_t index);

			void		SetRange(size_t index, size_t count);
			void		ClearRange(size_t index, size_t count);

			ssize_t		GetLowestClear(size_t fromIndex = 0) const;
			ssize_t		GetLowestContiguousClear(size_t count, size_t fromIndex = 0) const;
			ssize_t		GetHighestSet() const;

private:
			size_t		fElementsCount;
			size_t		fSize;
			addr_t*		fBits;

	static	const int	kBitsPerElement = (sizeof(addr_t) * 8);
};


bool
Bitmap::Get(size_t index) const
{
	ASSERT(index < fSize);

	const size_t kArrayElement = index / kBitsPerElement;
	const addr_t kBitMask = addr_t(1) << (index % kBitsPerElement);
	return fBits[kArrayElement] & kBitMask;
}


void
Bitmap::Set(size_t index)
{
	ASSERT(index < fSize);

	const size_t kArrayElement = index / kBitsPerElement;
	const addr_t kBitMask = addr_t(1) << (index % kBitsPerElement);
	fBits[kArrayElement] |= kBitMask;
}


void
Bitmap::Clear(size_t index)
{
	ASSERT(index < fSize);

	const size_t kArrayElement = index / kBitsPerElement;
	const addr_t kBitMask = addr_t(1) << (index % kBitsPerElement);
	fBits[kArrayElement] &= ~addr_t(kBitMask);
}

} // namespace BKernel


using BKernel::Bitmap;


#endif	// KERNEL_UTIL_BITMAP_H

