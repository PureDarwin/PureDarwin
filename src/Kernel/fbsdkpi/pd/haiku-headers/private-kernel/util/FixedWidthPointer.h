/*
 * Copyright 2012, Alex Smith, alex@alex-smith.me.uk.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_UTIL_FIXED_WIDTH_POINTER_H
#define KERNEL_UTIL_FIXED_WIDTH_POINTER_H


#include <SupportDefs.h>


/*!
	\class FixedWidthPointer
	\brief Pointer class with fixed size (64-bit) storage.

	This class is a pointer-like class that uses a fixed size 64-bit storage.
	This is used to make kernel_args compatible (i.e. the same size) for both
	32-bit and 64-bit kernels.
*/
template<typename Type>
class FixedWidthPointer {
public:
	Type * Pointer() const
	{
		return (Type*)(addr_t)fValue;
	}

	operator Type*() const
	{
		return Pointer();
	}

	Type& operator*() const
	{
		return *Pointer();
	}

	Type* operator->() const
	{
		return Pointer();
	}

	Type& operator[](size_t i) const
	{
		return Pointer()[i];
	}

	FixedWidthPointer& operator=(const FixedWidthPointer& p)
	{
		fValue = p.fValue;
		return *this;
	}

	FixedWidthPointer& operator=(Type* p)
	{
		fValue = (addr_t)p;
		return *this;
	}

	/*!
		Get the 64-bit pointer value.
		\return Pointer address.
	*/
	uint64 Get() const
	{
		return fValue;
	}

	/*!
		Set the 64-bit pointer value.
		\param addr New address for the pointer.
	*/
	void SetTo(uint64 addr)
	{
		fValue = addr;
	}

private:
	uint64 fValue;
} _PACKED;


// Specialization for void pointers, can be converted to another pointer type.
template<>
class FixedWidthPointer<void> {
public:
	void * Pointer() const
	{
		return (void*)(addr_t)fValue;
	}

	operator void*() const
	{
		return Pointer();
	}

	FixedWidthPointer& operator=(const FixedWidthPointer& p)
	{
		fValue = p.fValue;
		return *this;
	}

	FixedWidthPointer& operator=(void* p)
	{
		fValue = (addr_t)p;
		return *this;
	}

	uint64 Get() const
	{
		return fValue;
	}

	void SetTo(uint64 addr)
	{
		fValue = addr;
	}

private:
	uint64 fValue;
} _PACKED;


template<typename Type>
inline bool
operator==(const FixedWidthPointer<Type>& a, const Type* b)
{
	return a.Get() == (addr_t)b;
}


template<typename Type>
inline bool
operator!=(const FixedWidthPointer<Type>& a, const Type* b)
{
	return a.Get() != (addr_t)b;
}


template<typename Type>
inline bool
operator==(const FixedWidthPointer<Type>& a, Type* b)
{
	return a.Get() == (addr_t)b;
}


template<typename Type>
inline bool
operator!=(const FixedWidthPointer<Type>& a, Type* b)
{
	return a.Get() != (addr_t)b;
}


#endif	/* KERNEL_UTIL_FIXED_WIDTH_POINTER_H */
