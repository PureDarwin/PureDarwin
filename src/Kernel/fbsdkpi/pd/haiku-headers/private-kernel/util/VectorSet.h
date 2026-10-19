/*
 * Copyright 2003, Ingo Weinhold, ingo_weinhold@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _VECTOR_SET_H
#define _VECTOR_SET_H

#include <stdlib.h>
#include <string.h>

#include <SupportDefs.h>

#include <util/kernel_cpp.h>
#include <Vector.h>

// element orders
namespace VectorSetOrder {
	template<typename Value> class Ascending;
	template<typename Value> class Descending;
}

// for convenience
#define _VECTOR_SET_TEMPLATE_LIST template<typename Value, \
										   typename ElementOrder>
#define _VECTOR_SET_CLASS_NAME VectorSet<Value, ElementOrder>
#define _VECTOR_SET_CLASS_TYPE typename VectorSet<Value, ElementOrder>

/*!
	\class VectorSet
	\brief A generic vector-based set implementation.

	The elements of the set are ordered according to the supplied
	compare function object. Default is ascending order.
*/
template<typename Value,
		 typename ElementOrder = VectorSetOrder::Ascending<Value> >
class VectorSet {
private:
	typedef Vector<Value>							ElementVector;

public:
	typedef typename ElementVector::Iterator		Iterator;
	typedef typename ElementVector::ConstIterator	ConstIterator;

private:
	static const size_t				kDefaultChunkSize = 10;
	static const size_t				kMaximalChunkSize = 1024 * 1024;

public:
	VectorSet(size_t chunkSize = kDefaultChunkSize);
// TODO: Copy constructor, assignment operator.
	~VectorSet();

	status_t Insert(const Value &value, bool replace = true);

	int32 Remove(const Value &value);
	Iterator Erase(const Iterator &iterator);

	inline int32 Count() const;
	inline bool IsEmpty() const;
	void MakeEmpty();

	inline Iterator Begin();
	inline ConstIterator Begin() const;
	inline Iterator End();
	inline ConstIterator End() const;
	inline Iterator Null();
	inline ConstIterator Null() const;

	Iterator Find(const Value &value);
	ConstIterator Find(const Value &value) const;
	Iterator FindClose(const Value &value, bool less);
	ConstIterator FindClose(const Value &value, bool less) const;

private:
	int32 _FindInsertionIndex(const Value &value, bool &exists) const;

private:
	ElementVector	fElements;
	ElementOrder	fCompare;
};


// VectorSet

// constructor
/*!	\brief Creates an empty set.
	\param chunkSize The granularity for the underlying vector's capacity,
		   i.e. the minimal number of elements the capacity grows or shrinks
		   when necessary.
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_NAME::VectorSet(size_t chunkSize)
	: fElements(chunkSize)
{
}

// destructor
/*!	\brief Frees all resources associated with the object.

	The contained elements are destroyed. Note, that, if the element
	type is a pointer type, only the pointer is destroyed, not the object
	it points to.
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_NAME::~VectorSet()
{
}

// Insert
/*!	\brief Inserts a copy of the the supplied value.

	If an element with the supplied value is already in the set, the
	operation will not fail, but return \c B_OK. \a replace specifies
	whether the element shall be replaced with the supplied in such a case.
	Otherwise \a replace is ignored.

	\param value The value to be inserted.
	\param replace If the an element with this value does already exist and
		   \a replace is \c true, the element is replaced, otherwise left
		   untouched.
	\return
	- \c B_OK: Everything went fine.
	- \c B_NO_MEMORY: Insufficient memory for this operation.
*/
_VECTOR_SET_TEMPLATE_LIST
status_t
_VECTOR_SET_CLASS_NAME::Insert(const Value &value, bool replace)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(value, exists);
	if (exists) {
		if (replace)
			fElements[index] = value;
		return B_OK;
	}
	return fElements.Insert(value, index);
}

// Remove
/*!	\brief Removes the element with the supplied value.
	\param value The value of the element to be removed.
	\return The number of removed occurrences, i.e. \c 1, if the set
			contained an element with the value, \c 0 otherwise.
*/
_VECTOR_SET_TEMPLATE_LIST
int32
_VECTOR_SET_CLASS_NAME::Remove(const Value &value)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(value, exists);
	if (!exists)
		return 0;
	fElements.Erase(index);
	return 1;
}

// Erase
/*!	\brief Removes the element at the given position.
	\param iterator An iterator referring to the element to be removed.
	\return An iterator referring to the element succeeding the removed
			one (End(), if it was the last element that has been
			removed), or Null(), if \a iterator was an invalid iterator
			(in this case including End()).
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_TYPE::Iterator
_VECTOR_SET_CLASS_NAME::Erase(const Iterator &iterator)
{
	return fElements.Erase(iterator);
}

// Count
/*!	\brief Returns the number of elements the set contains.
	\return The number of elements the set contains.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
int32
_VECTOR_SET_CLASS_NAME::Count() const
{
	return fElements.Count();
}

// IsEmpty
/*!	\brief Returns whether the set is empty.
	\return \c true, if the set is empty, \c false otherwise.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
bool
_VECTOR_SET_CLASS_NAME::IsEmpty() const
{
	return fElements.IsEmpty();
}

// MakeEmpty
/*!	\brief Removes all elements from the set.
*/
_VECTOR_SET_TEMPLATE_LIST
void
_VECTOR_SET_CLASS_NAME::MakeEmpty()
{
	fElements.MakeEmpty();
}

// Begin
/*!	\brief Returns an iterator referring to the beginning of the set.

	If the set is not empty, Begin() refers to its first element,
	otherwise it is equal to End() and must not be dereferenced!

	\return An iterator referring to the beginning of the set.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
_VECTOR_SET_CLASS_TYPE::Iterator
_VECTOR_SET_CLASS_NAME::Begin()
{
	return fElements.Begin();
}

// Begin
/*!	\brief Returns an iterator referring to the beginning of the set.

	If the set is not empty, Begin() refers to its first element,
	otherwise it is equal to End() and must not be dereferenced!

	\return An iterator referring to the beginning of the set.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
_VECTOR_SET_CLASS_TYPE::ConstIterator
_VECTOR_SET_CLASS_NAME::Begin() const
{
	return fElements.Begin();
}

// End
/*!	\brief Returns an iterator referring to the end of the set.

	The position identified by End() is the one succeeding the last
	element, i.e. it must not be dereferenced!

	\return An iterator referring to the end of the set.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
_VECTOR_SET_CLASS_TYPE::Iterator
_VECTOR_SET_CLASS_NAME::End()
{
	return fElements.End();
}

// End
/*!	\brief Returns an iterator referring to the end of the set.

	The position identified by End() is the one succeeding the last
	element, i.e. it must not be dereferenced!

	\return An iterator referring to the end of the set.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
_VECTOR_SET_CLASS_TYPE::ConstIterator
_VECTOR_SET_CLASS_NAME::End() const
{
	return fElements.End();
}

// Null
/*!	\brief Returns an invalid iterator.

	Null() is used as a return value, if something went wrong. It must
	neither be incremented or decremented nor dereferenced!

	\return An invalid iterator.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
_VECTOR_SET_CLASS_TYPE::Iterator
_VECTOR_SET_CLASS_NAME::Null()
{
	return fElements.Null();
}

// Null
/*!	\brief Returns an invalid iterator.

	Null() is used as a return value, if something went wrong. It must
	neither be incremented or decremented nor dereferenced!

	\return An invalid iterator.
*/
_VECTOR_SET_TEMPLATE_LIST
inline
_VECTOR_SET_CLASS_TYPE::ConstIterator
_VECTOR_SET_CLASS_NAME::Null() const
{
	return fElements.Null();
}

// Find
/*!	\brief Returns an iterator referring to the element with the
		   specified value.
	\param value The value of the element to be found.
	\return An iterator referring to the found element, or End(), if the
			set doesn't contain any element with the given value.
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_TYPE::Iterator
_VECTOR_SET_CLASS_NAME::Find(const Value &value)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(value, exists);
	if (!exists)
		return fElements.End();
	return fElements.IteratorForIndex(index);
}

// Find
/*!	\brief Returns an iterator referring to the element with the
		   specified value.
	\param value The value of the element to be found.
	\return An iterator referring to the found element, or End(), if the
			set doesn't contain any element with the given value.
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_TYPE::ConstIterator
_VECTOR_SET_CLASS_NAME::Find(const Value &value) const
{
	bool exists = false;
	int32 index = _FindInsertionIndex(value, exists);
	if (!exists)
		return fElements.End();
	return fElements.IteratorForIndex(index);
}

// FindClose
/*!	\brief Returns an iterator referring to the element with a value closest
		   to the specified one.

	If the set contains an element with the specified value, an iterator
	to it is returned. Otherwise \a less indicates whether an iterator to
	the directly smaller or greater element shall be returned.

	If \a less is \c true and the first element in the set has a greater
	value than the specified one, End() is returned. Similarly, when \a less
	is \c false and the last element is smaller. Find() invoked on an empty
	set always returns End().

	Note, that the element order used for the set is specified as template
	argument to the class. Default is ascending order. Descending order
	inverts the meaning of \a less, i.e. if \c true, greater values will
	be returned, since they are smaller ones according to the order.

	\param value The value of the element to be found.
	\return An iterator referring to the found element, or End(), if the
			set doesn't contain any element with the given value or a close
			one according to \a less.
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_TYPE::Iterator
_VECTOR_SET_CLASS_NAME::FindClose(const Value &value, bool less)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(value, exists);
	// If not found, the index _FindInsertionIndex() returns will point to
	// an element with a greater value or to End(). So, no special handling
	// is needed for !less.
	if (exists || !less)
		return fElements.IteratorForIndex(index);
	// An element with a smaller value is desired. The previous one (if any)
	// will do.
	if (index > 0)
		return fElements.IteratorForIndex(index - 1);
	return fElements.End();
}

// FindClose
/*!	\brief Returns an iterator referring to the element with a value closest
		   to the specified one.

	If the set contains an element with the specified value, an iterator
	to it is returned. Otherwise \a less indicates whether an iterator to
	the directly smaller or greater element shall be returned.

	If \a less is \c true and the first element in the set has a greater
	value than the specified one, End() is returned. Similarly, when \a less
	is \c false and the last element is smaller. Find() invoked on an empty
	set always returns End().

	Note, that the element order used for the set is specified as template
	argument to the class. Default is ascending order. Descending order
	inverts the meaning of \a less, i.e. if \c true, greater values will
	be returned, since they are smaller ones according to the order.

	\param value The value of the element to be found.
	\return An iterator referring to the found element, or End(), if the
			set doesn't contain any element with the given value or a close
			one according to \a less.
*/
_VECTOR_SET_TEMPLATE_LIST
_VECTOR_SET_CLASS_TYPE::ConstIterator
_VECTOR_SET_CLASS_NAME::FindClose(const Value &value, bool less) const
{
	bool exists = false;
	int32 index = _FindInsertionIndex(value, exists);
	// If not found, the index _FindInsertionIndex() returns will point to
	// an element with a greater value or to End(). So, no special handling
	// is needed for !less.
	if (exists || !less)
		return fElements.IteratorForIndex(index);
	// An element with a smaller value is desired. The previous one (if any)
	// will do.
	if (index > 0)
		return fElements.IteratorForIndex(index - 1);
	return fElements.End();
}

// _FindInsertionIndex
/*!	\brief Finds the index at which the element with the supplied value is
		   located or at which it would need to be inserted.
	\param value The value.
	\param exists Is set to \c true, if the set does already contain an
		   element with that value.
	\return The index at which the element with the supplied value is
			located or at which it would need to be inserted.
*/
_VECTOR_SET_TEMPLATE_LIST
int32
_VECTOR_SET_CLASS_NAME::_FindInsertionIndex(const Value &value,
											bool &exists) const
{
	// binary search
	int32 lower = 0;
	int32 upper = Count();
	while (lower < upper) {
		int32 mid = (lower + upper) / 2;
		int cmp = fCompare(fElements[mid], value);
		if (cmp < 0)
			lower = mid + 1;
		else
			upper = mid;
	}
	exists = (lower < Count() && fCompare(value, fElements[lower]) == 0);
	return lower;
}


// element orders

namespace VectorSetOrder {

// Ascending
/*!	\brief A compare function object implying and ascending order.

	The < operator must be defined on the template argument.
*/
template<typename Value>
class Ascending {
public:
	inline int operator()(const Value &a, const Value &b) const
	{
		if (a < b)
			return -1;
		else if (b < a)
			return 1;
		return 0;
	}
};

// Descending
/*!	\brief A compare function object implying and descending order.

	The < operator must be defined on the template argument.
*/
template<typename Value>
class Descending {
public:
	inline int operator()(const Value &a, const Value &b) const
	{
		if (a < b)
			return -1;
		else if (b < a)
			return 1;
		return 0;
	}
};

}	// namespace VectorSetOrder

#endif	// _VECTOR_SET_H
