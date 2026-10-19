/*
 * Copyright 2012, Jonathan Schleifer <js@webkeks.org>. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef	_SUPPORT_STACKORHEAPARRAY_H
#define	_SUPPORT_STACKORHEAPARRAY_H

#include <cstddef>
#include <new>

template <typename Type, int StackSize>
class BStackOrHeapArray {
public:
	BStackOrHeapArray(size_t count)
	{
		if (count > StackSize)
			fData = new(std::nothrow) Type[count];
		else
			fData = fStackData;
	}

	~BStackOrHeapArray()
	{
		if (fData != fStackData)
			delete[] fData;
	}

	bool IsValid() const
	{
		return fData != NULL;
	}

	operator Type*()
	{
		return fData;
	}

private:
	Type	fStackData[StackSize];
	Type*	fData;
};

#endif
