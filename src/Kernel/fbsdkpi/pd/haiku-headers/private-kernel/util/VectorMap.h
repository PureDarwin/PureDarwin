/*
 * Copyright 2003, Ingo Weinhold, ingo_weinhold@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _VECTOR_MAP_H
#define _VECTOR_MAP_H

#include <stdlib.h>
#include <string.h>

#include <SupportDefs.h>

#include <util/kernel_cpp.h>
#include <util/KernelUtilsOrder.h>
#include <util/Vector.h>

namespace VectorMapEntryStrategy {
	// Pair
	template<typename Key, typename Value,
			 typename KeyOrder = KernelUtilsOrder::Ascending<Key> > class Pair;
	// ImplicitKey
	template<typename Key, typename Value, typename GetKey,
			 typename KeyOrder = KernelUtilsOrder::Ascending<Key> >
			 class ImplicitKey;
}

template<typename Entry, typename Parent, typename EntryIterator>
	class VectorMapIterator;
template<typename _Key, typename _Value, typename Entry, typename Parent>
	class VectorMapEntry;

// for convenience
#define _VECTOR_MAP_TEMPLATE_LIST template<typename Key, typename Value, \
										   typename EntryStrategy>
#define _VECTOR_MAP_CLASS_NAME VectorMap<Key, Value, EntryStrategy>
#define _VECTOR_MAP_CLASS_TYPE typename VectorMap<Key, Value, EntryStrategy>

/*!
	\class VectorMap
	\brief A generic vector-based map implementation.

	The map entries are ordered according to the supplied
	compare function object. Default is ascending order.

	Note that VectorMap::Entry is not the same class as EntryStrategy::Entry.
	It is light-weight class, an object of which is returned when a iterator
	is dereferenced. It features a Key() and a Value() method returning
	references to the entry's key/value. This allows EntryStrategy::Entry
	to be an arbitrary class, not needing to implement a certain interface.
*/
template<typename Key, typename Value,
		 typename EntryStrategy = VectorMapEntryStrategy::Pair<Key, Value> >
class VectorMap {
private:
	typedef _VECTOR_MAP_CLASS_NAME							Class;
	typedef typename EntryStrategy::Entry					_Entry;
	typedef typename EntryStrategy::KeyReference			KeyReference;
	typedef Vector<_Entry>									ElementVector;

public:
	typedef VectorMapEntry<KeyReference, Value, _Entry, Class>
															Entry;
	typedef VectorMapEntry<KeyReference, const Value, const _Entry,
						   const Class>						ConstEntry;
	typedef VectorMapIterator<Entry, Class, typename ElementVector::Iterator>
															Iterator;
	typedef VectorMapIterator<ConstEntry, const Class, typename ElementVector::ConstIterator>
															ConstIterator;

private:
	static const size_t				kDefaultChunkSize = 10;
	static const size_t				kMaximalChunkSize = 1024 * 1024;

public:
	VectorMap(size_t chunkSize = kDefaultChunkSize);
// TODO: Copy constructor, assignment operator.
	~VectorMap();

	status_t Insert(const Key &key, const Value &value);
	status_t Put(const Key &key, const Value &value);
	Value &Get(const Key &key);
	const Value &Get(const Key &key) const;

	int32 Remove(const Key &key);
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

	Iterator Find(const Key &key);
	ConstIterator Find(const Key &key) const;
	Iterator FindClose(const Key &key, bool less);
	ConstIterator FindClose(const Key &key, bool less) const;

private:
	int32 _FindInsertionIndex(const Key &key, bool &exists) const;

private:
//	friend class Entry;
//	friend class ConstEntry;
	friend class VectorMapEntry<KeyReference, Value, _Entry, Class>;
	friend class VectorMapEntry<KeyReference, const Value, const _Entry,
		const Class>;

	ElementVector	fElements;
	EntryStrategy	fEntryStrategy;
};


// VectorMapEntry
template<typename KeyReference, typename _Value, typename Entry,
		 typename Parent>
class VectorMapEntry {
private:
	typedef VectorMapEntry<KeyReference, _Value, Entry, Parent>	Class;

public:
	VectorMapEntry()
		: fParent(NULL), fEntry(NULL) {}

	inline KeyReference Key() const
	{
		return fParent->fEntryStrategy.GetKey(*fEntry);
	}

	inline _Value &Value() const
	{
		return fParent->fEntryStrategy.GetValue(*fEntry);
	}

	inline const Class *operator->() const
	{
		return this;
	}

// private
public:
	VectorMapEntry(Parent *parent, Entry *entry)
		: fParent(parent), fEntry(entry) {}

private:
	const Parent	*fParent;
	Entry			*fEntry;
};


// VectorMapIterator
template<typename Entry, typename Parent, typename EntryIterator>
class VectorMapIterator {
private:
	typedef VectorMapIterator<Entry, Parent, EntryIterator>	Iterator;

public:
	inline VectorMapIterator()
		: fParent(NULL),
		  fIterator()
	{
	}

	inline VectorMapIterator(
		const Iterator &other)
		: fParent(other.fParent),
		  fIterator(other.fIterator)
	{
	}

	inline Iterator &operator++()
	{
		++fIterator;
		return *this;
	}

	inline Iterator operator++(int)
	{
		Iterator it(*this);
		++*this;
		return it;
	}

	inline Iterator &operator--()
	{
		--fIterator;
		return *this;
	}

	inline Iterator operator--(int)
	{
		Iterator it(*this);
		--*this;
		return it;
	}

	inline Iterator &operator=(const Iterator &other)
	{
		fParent = other.fParent;
		fIterator = other.fIterator;
		return *this;
	}

	inline bool operator==(const Iterator &other) const
	{
		return (fParent == other.fParent && fIterator == other.fIterator);
	}

	inline bool operator!=(const Iterator &other) const
	{
		return !(*this == other);
	}

	inline Entry operator*() const
	{
		return Entry(fParent, &*fIterator);
	}

	inline Entry operator->() const
	{
		return Entry(fParent, &*fIterator);
	}

	inline operator bool() const
	{
		return fIterator;
	}

// private
public:
	inline VectorMapIterator(Parent *parent, const EntryIterator &iterator)
		: fParent(parent),
		  fIterator(iterator)
	{
	}

	inline EntryIterator &GetIterator()
	{
		return fIterator;
	}

	inline const EntryIterator &GetIterator() const
	{
		return fIterator;
	}

protected:
	Parent			*fParent;
	EntryIterator	fIterator;
};


// VectorMap

// constructor
/*!	\brief Creates an empty map.
	\param chunkSize The granularity for the underlying vector's capacity,
		   i.e. the minimal number of elements the capacity grows or shrinks
		   when necessary.
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_NAME::VectorMap(size_t chunkSize)
	: fElements(chunkSize)
{
}

// destructor
/*!	\brief Frees all resources associated with the object.

	The contained keys and values are destroyed. Note, that for pointer
	types only the pointer is destroyed, not the object it points to.
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_NAME::~VectorMap()
{
}

// Insert
/*!	\brief Associates a key with a value.

	If there is already a value associated with the key, the old entry
	is replaced.

	\param key The key to which a value shall be associated.
	\param value The value to be associated with the key.
	\return
	- \c B_OK: Everything went fine.
	- \c B_NO_MEMORY: Insufficient memory for this operation.
	- \c B_BAD_VALUE: The map's EntryStrategy requires some special
	  relationship between key and value, that \a key and \a value haven't
	  (doesn't apply to the default strategy).
*/
_VECTOR_MAP_TEMPLATE_LIST
status_t
_VECTOR_MAP_CLASS_NAME::Insert(const Key &key, const Value &value)
{
	if (!fEntryStrategy.AreCompatible(key, value))
		return B_BAD_VALUE;
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	if (exists) {
		fElements[index] = fEntryStrategy.MakeEntry(key, value);
		return B_OK;
	}
	return fElements.Insert(fEntryStrategy.MakeEntry(key, value), index);
}

// Put
/*!	\brief Equivalent to Insert().
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
status_t
_VECTOR_MAP_CLASS_NAME::Put(const Key &key, const Value &value)
{
	return Insert(key, value);
}

// Get
/*!	\brief Returns the value associated with a given key.

	\note Invoking this method for a key not know to the map is dangerous!
		  The behavior is unspecified. It may even crash.

	\param key The key to be looked up.
	\return The value associated with \a key.
*/
_VECTOR_MAP_TEMPLATE_LIST
Value &
_VECTOR_MAP_CLASS_NAME::Get(const Key &key)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	if (!exists)
		return fEntryStrategy.GetValue(fElements[0]);
	return fEntryStrategy.GetValue(fElements[index]);
}

// Get
/*!	\brief Returns the value associated with a given key.

	\note Invoking this method for a key not know to the map is dangerous!
		  The behavior is unspecified. It may even crash.

	\param key The key to be looked up.
	\return The value associated with \a key.
*/
_VECTOR_MAP_TEMPLATE_LIST
const Value &
_VECTOR_MAP_CLASS_NAME::Get(const Key &key) const
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	if (!exists)
		return fEntryStrategy.GetValue(fElements[0]);
	return fEntryStrategy.GetValue(fElements[index]);
}

// Remove
/*!	\brief Removes the entry with the supplied key.
	\param key The key to be removed.
	\return The number of removed occurrences, i.e. \c 1, if the map
			contained an entry with that key, \c 0 otherwise.
*/
_VECTOR_MAP_TEMPLATE_LIST
int32
_VECTOR_MAP_CLASS_NAME::Remove(const Key &key)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	if (!exists)
		return 0;
	fElements.Erase(index);
	return 1;
}

// Erase
/*!	\brief Removes the entry at the given position.
	\param iterator An iterator referring to the entry to be removed.
	\return An iterator referring to the entry succeeding the removed
			one (End(), if it was the last element that has been
			removed), or Null(), if \a iterator was an invalid iterator
			(in this case including End()).
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_TYPE::Iterator
_VECTOR_MAP_CLASS_NAME::Erase(const Iterator &iterator)
{
	return Iterator(this, fElements.Erase(iterator.GetIterator()));
}

// Count
/*!	\brief Returns the number of entry the map contains.
	\return The number of entries the map contains.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
int32
_VECTOR_MAP_CLASS_NAME::Count() const
{
	return fElements.Count();
}

// IsEmpty
/*!	\brief Returns whether the map is empty.
	\return \c true, if the map is empty, \c false otherwise.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
bool
_VECTOR_MAP_CLASS_NAME::IsEmpty() const
{
	return fElements.IsEmpty();
}

// MakeEmpty
/*!	\brief Removes all entries from the map.
*/
_VECTOR_MAP_TEMPLATE_LIST
void
_VECTOR_MAP_CLASS_NAME::MakeEmpty()
{
	fElements.MakeEmpty();
}

// Begin
/*!	\brief Returns an iterator referring to the beginning of the map.

	If the map is not empty, Begin() refers to its first entry,
	otherwise it is equal to End() and must not be dereferenced!

	\return An iterator referring to the beginning of the map.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
_VECTOR_MAP_CLASS_TYPE::Iterator
_VECTOR_MAP_CLASS_NAME::Begin()
{
	return Iterator(this, fElements.Begin());
}

// Begin
/*!	\brief Returns an iterator referring to the beginning of the map.

	If the map is not empty, Begin() refers to its first entry,
	otherwise it is equal to End() and must not be dereferenced!

	\return An iterator referring to the beginning of the map.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
_VECTOR_MAP_CLASS_TYPE::ConstIterator
_VECTOR_MAP_CLASS_NAME::Begin() const
{
	return ConstIterator(this, fElements.Begin());
}

// End
/*!	\brief Returns an iterator referring to the end of the map.

	The position identified by End() is the one succeeding the last
	entry, i.e. it must not be dereferenced!

	\return An iterator referring to the end of the map.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
_VECTOR_MAP_CLASS_TYPE::Iterator
_VECTOR_MAP_CLASS_NAME::End()
{
	return Iterator(this, fElements.End());
}

// End
/*!	\brief Returns an iterator referring to the end of the map.

	The position identified by End() is the one succeeding the last
	entry, i.e. it must not be dereferenced!

	\return An iterator referring to the end of the map.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
_VECTOR_MAP_CLASS_TYPE::ConstIterator
_VECTOR_MAP_CLASS_NAME::End() const
{
	return ConstIterator(this, fElements.End());
}

// Null
/*!	\brief Returns an invalid iterator.

	Null() is used as a return value, if something went wrong. It must
	neither be incremented or decremented nor dereferenced!

	\return An invalid iterator.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
_VECTOR_MAP_CLASS_TYPE::Iterator
_VECTOR_MAP_CLASS_NAME::Null()
{
	return Iterator(this, fElements.Null());
}

// Null
/*!	\brief Returns an invalid iterator.

	Null() is used as a return value, if something went wrong. It must
	neither be incremented or decremented nor dereferenced!

	\return An invalid iterator.
*/
_VECTOR_MAP_TEMPLATE_LIST
inline
_VECTOR_MAP_CLASS_TYPE::ConstIterator
_VECTOR_MAP_CLASS_NAME::Null() const
{
	return ConstIterator(this, fElements.Null());
}

// Find
/*!	\brief Returns an iterator referring to the entry with the
		   specified key.
	\param key The key of the entry to be found.
	\return An iterator referring to the found entry, or End(), if the
			map doesn't contain any entry with the given value.
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_TYPE::Iterator
_VECTOR_MAP_CLASS_NAME::Find(const Key &key)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	if (!exists)
		return End();
	return Iterator(this, fElements.IteratorForIndex(index));
}

// Find
/*!	\brief Returns an iterator referring to the entry with the
		   specified key.
	\param key The key of the entry to be found.
	\return An iterator referring to the found entry, or End(), if the
			map doesn't contain any entry with the given value.
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_TYPE::ConstIterator
_VECTOR_MAP_CLASS_NAME::Find(const Key &key) const
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	if (!exists)
		return End();
	return ConstIterator(this, fElements.IteratorForIndex(index));
}

// FindClose
/*!	\brief Returns an iterator referring to the entry with a key closest
		   to the specified one.

	If the map contains an entry with the specified key, an iterator
	to it is returned. Otherwise \a less indicates whether an iterator to
	the entry with an directly smaller or greater key shall be returned.

	If \a less is \c true and the first entry in the map has a greater
	key than the specified one, End() is returned. Similarly, when \a less
	is \c false and the last entry's key is smaller. Find() invoked on an
	empty map always returns End().

	Note, that the key order used for the set is specified as template
	argument to the class. Default is ascending order. Descending order
	inverts the meaning of \a less, i.e. if \c true, greater values will
	be returned, since they are smaller ones according to the order.

	\param value The key of the entry to be found.
	\return An iterator referring to the found entry, or End(), if the
			map doesn't contain any entry with the given key or a close
			one according to \a less.
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_TYPE::Iterator
_VECTOR_MAP_CLASS_NAME::FindClose(const Key &key, bool less)
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	// If not found, the index _FindInsertionIndex() returns will point to
	// an element with a greater value or to End(). So, no special handling
	// is needed for !less.
	if (exists || !less)
		return Iterator(this, fElements.IteratorForIndex(index));
	// An element with a smaller value is desired. The previous one (if any)
	// will do.
	if (index > 0)
		return Iterator(this, fElements.IteratorForIndex(index - 1));
	return End();
}

// FindClose
/*!	\brief Returns an iterator referring to the entry with a key closest
		   to the specified one.

	If the map contains an entry with the specified key, an iterator
	to it is returned. Otherwise \a less indicates whether an iterator to
	the entry with an directly smaller or greater key shall be returned.

	If \a less is \c true and the first entry in the map has a greater
	key than the specified one, End() is returned. Similarly, when \a less
	is \c false and the last entry's key is smaller. Find() invoked on an
	empty map always returns End().

	Note, that the key order used for the set is specified as template
	argument to the class. Default is ascending order. Descending order
	inverts the meaning of \a less, i.e. if \c true, greater values will
	be returned, since they are smaller ones according to the order.

	\param value The key of the entry to be found.
	\return An iterator referring to the found entry, or End(), if the
			map doesn't contain any entry with the given key or a close
			one according to \a less.
*/
_VECTOR_MAP_TEMPLATE_LIST
_VECTOR_MAP_CLASS_TYPE::ConstIterator
_VECTOR_MAP_CLASS_NAME::FindClose(const Key &key, bool less) const
{
	bool exists = false;
	int32 index = _FindInsertionIndex(key, exists);
	// If not found, the index _FindInsertionIndex() returns will point to
	// an element with a greater value or to End(). So, no special handling
	// is needed for !less.
	if (exists || !less)
		return ConstIterator(this, fElements.IteratorForIndex(index));
	// An element with a smaller value is desired. The previous one (if any)
	// will do.
	if (index > 0)
		return ConstIterator(this, fElements.IteratorForIndex(index - 1));
	return End();
}

// _FindInsertionIndex
/*!	\brief Finds the index at which the entry with the supplied key is
		   located or at which it would need to be inserted.
	\param key The key.
	\param exists Is set to \c true, if the map does already contain an
		   entry with that key, to \c false otherwise.
	\return The index at which the entry with the supplied key is
			located or at which it would need to be inserted.
*/
_VECTOR_MAP_TEMPLATE_LIST
int32
_VECTOR_MAP_CLASS_NAME::_FindInsertionIndex(const Key &key,
											bool &exists) const
{
	// binary search
	int32 lower = 0;
	int32 upper = Count();
	while (lower < upper) {
		int32 mid = (lower + upper) / 2;
		int cmp = fEntryStrategy.Compare(fEntryStrategy.GetKey(fElements[mid]),
															   key);
		if (cmp < 0)
			lower = mid + 1;
		else
			upper = mid;
	}
	exists = (lower < Count() && fEntryStrategy.Compare(key,
		fEntryStrategy.GetKey(fElements[lower])) == 0);
	return lower;
}


// entry strategies

namespace VectorMapEntryStrategy {

// Pair
template<typename Key, typename Value, typename KeyOrder>
class Pair {
public:
	typedef const Key	&KeyReference;

	class Entry {
	public:
		Entry(const Key &key, const Value &value)
			: key(key), value(value) {}

		Key		key;
		Value	value;
	};

	inline KeyReference GetKey(const Entry &entry) const
	{
		return entry.key;
	}

	inline const Value &GetValue(const Entry &entry) const
	{
		return entry.value;
	}

	inline Value &GetValue(Entry &entry) const
	{
		return entry.value;
	}

	inline Entry MakeEntry(const Key &key, const Value &value) const
	{
		return Entry(key, value);
	}

	inline bool AreCompatible(const Key &, const Value &) const
	{
		return true;
	}

	inline int Compare(const Key &a, const Key &b) const
	{
		return fCompare(a, b);
	}

private:
	KeyOrder	fCompare;
};

// ImplicitKey
template<typename Key, typename Value, typename _GetKey, typename KeyOrder>
class ImplicitKey {
public:
	typedef Key			KeyReference;
	typedef Value		Entry;

	inline KeyReference GetKey(const Entry &entry) const
	{
		return fGetKey(entry);
	}

	inline const Value &GetValue(const Entry &entry) const
	{
		return entry;
	}

	inline Value &GetValue(Entry &entry) const
	{
		return entry;
	}

	inline Entry MakeEntry(const Key &, const Value &value) const
	{
		return value;
	}

	inline bool AreCompatible(const Key &key, const Value &value) const
	{
		return (fGetKey(value) == key);
	}

	inline int Compare(const Key &a, const Key &b) const
	{
		return fCompare(a, b);
	}

private:
	KeyOrder	fCompare;
	_GetKey		fGetKey;
};

}	// VectorMapEntryStrategy

#endif	// _VECTOR_MAP_H
