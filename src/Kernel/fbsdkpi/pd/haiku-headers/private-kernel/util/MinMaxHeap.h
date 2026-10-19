/*
 * Copyright 2013 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Paweł Dziepak, pdziepak@quarnos.org
 */
#ifndef KERNEL_UTIL_MIN_MAX_HEAP_H
#define KERNEL_UTIL_MIN_MAX_HEAP_H


#include <debug.h>

#include <SupportDefs.h>


template<typename Element, typename Key>
struct MinMaxHeapLink {
						MinMaxHeapLink();

			bool		fMinTree;
			int			fIndex;
			Key			fKey;
};

template<typename Element, typename Key>
class MinMaxHeapLinkImpl {
private:
	typedef MinMaxHeapLink<Element, Key> Link;

public:
	inline	Link*		GetMinMaxHeapLink();

private:
			Link		fMinMaxHeapLink;
};

template<typename Element, typename Key>
class MinMaxHeapStandardGetLink {
private:
	typedef MinMaxHeapLink<Element, Key> Link;

public:
	inline	Link*		operator()(Element* element) const;
};

template<typename Element, typename Key,
	MinMaxHeapLink<Element, Key> Element::*LinkMember>
class MinMaxHeapMemberGetLink {
private:
	typedef MinMaxHeapLink<Element, Key> Link;

public:
	inline	Link*		operator()(Element* element) const;
};

template<typename Key>
class MinMaxHeapCompare {
public:
	inline	bool		operator()(Key a, Key b);
};

#define MIN_MAX_HEAP_TEMPLATE_LIST	\
	template<typename Element, typename Key, typename Compare, typename GetLink>
#define MIN_MAX_HEAP_CLASS_NAME	MinMaxHeap<Element, Key, Compare, GetLink>

template<typename Element, typename Key,
	typename Compare = MinMaxHeapCompare<Key>,
	typename GetLink = MinMaxHeapStandardGetLink<Element, Key> >
class MinMaxHeap {
public:
						MinMaxHeap();
						MinMaxHeap(int initialSize);
						~MinMaxHeap();

	inline	Element*	PeekMinimum(int32 index = 0) const;
	inline	Element*	PeekMaximum(int32 index = 0) const;

	static	const Key&	GetKey(Element* element);

	inline	void		ModifyKey(Element* element, Key newKey);

	inline	void		RemoveMinimum();
	inline	void		RemoveMaximum();

	inline	status_t	Insert(Element* element, Key key);

private:
			status_t	_GrowHeap(int minimalSize = 0);

			void		_MoveUp(MinMaxHeapLink<Element, Key>* link);
			void		_MoveDown(MinMaxHeapLink<Element, Key>* link);
			bool		_ChangeTree(MinMaxHeapLink<Element, Key>* link);

			void		_RemoveLast(bool minTree);

			Element**	fMinElements;
			int			fMinLastElement;

			Element**	fMaxElements;
			int			fMaxLastElement;

			int			fSize;

	static	Compare		sCompare;
	static	GetLink		sGetLink;
};


#if KDEBUG
template<typename Element, typename Key>
MinMaxHeapLink<Element, Key>::MinMaxHeapLink()
	:
	fIndex(-1)
{
}
#else
template<typename Element, typename Key>
MinMaxHeapLink<Element, Key>::MinMaxHeapLink()
{
}
#endif


template<typename Element, typename Key>
MinMaxHeapLink<Element, Key>*
MinMaxHeapLinkImpl<Element, Key>::GetMinMaxHeapLink()
{
	return &fMinMaxHeapLink;
}


template<typename Element, typename Key>
MinMaxHeapLink<Element, Key>*
MinMaxHeapStandardGetLink<Element, Key>::operator()(Element* element) const
{
	return element->GetMinMaxHeapLink();
}


template<typename Element, typename Key,
	MinMaxHeapLink<Element, Key> Element::*LinkMember>
MinMaxHeapLink<Element, Key>*
MinMaxHeapMemberGetLink<Element, Key, LinkMember>::operator()(
	Element* element) const
{
	return &(element->*LinkMember);
}


template<typename Key>
bool
MinMaxHeapCompare<Key>::operator()(Key a, Key b)
{
	return a < b;
}


MIN_MAX_HEAP_TEMPLATE_LIST
MIN_MAX_HEAP_CLASS_NAME::MinMaxHeap()
	:
	fMinElements(NULL),
	fMinLastElement(0),
	fMaxElements(NULL),
	fMaxLastElement(0),
	fSize(0)
{
}


MIN_MAX_HEAP_TEMPLATE_LIST
MIN_MAX_HEAP_CLASS_NAME::MinMaxHeap(int initialSize)
	:
	fMinElements(NULL),
	fMinLastElement(0),
	fMaxElements(NULL),
	fMaxLastElement(0),
	fSize(0)
{
	_GrowHeap(initialSize);
}


MIN_MAX_HEAP_TEMPLATE_LIST
MIN_MAX_HEAP_CLASS_NAME::~MinMaxHeap()
{
	free(fMinElements);
}


MIN_MAX_HEAP_TEMPLATE_LIST
Element*
MIN_MAX_HEAP_CLASS_NAME::PeekMinimum(int32 index) const
{
	if (index < fMinLastElement)
		return fMinElements[index];
	else if (index - fMinLastElement < fMaxLastElement) {
		return fMaxElements[fMaxLastElement - (index - fMinLastElement) - 1];
	}

	return NULL;
}


MIN_MAX_HEAP_TEMPLATE_LIST
Element*
MIN_MAX_HEAP_CLASS_NAME::PeekMaximum(int32 index) const
{
	if (index < fMaxLastElement)
		return fMaxElements[index];
	else if (index - fMaxLastElement < fMinLastElement) {
		return fMinElements[fMinLastElement - (index - fMaxLastElement) - 1];
	}

	return NULL;
}


MIN_MAX_HEAP_TEMPLATE_LIST
const Key&
MIN_MAX_HEAP_CLASS_NAME::GetKey(Element* element)
{
	return sGetLink(element)->fKey;
}


MIN_MAX_HEAP_TEMPLATE_LIST
void
MIN_MAX_HEAP_CLASS_NAME::ModifyKey(Element* element, Key newKey)
{
	MinMaxHeapLink<Element, Key>* link = sGetLink(element);

	Key oldKey = link->fKey;
	link->fKey = newKey;

	if (!sCompare(newKey, oldKey) && !sCompare(oldKey, newKey))
		return;

	if (sCompare(newKey, oldKey) ^ !link->fMinTree)
		_MoveUp(link);
	else
		_MoveDown(link);
}


MIN_MAX_HEAP_TEMPLATE_LIST
void
MIN_MAX_HEAP_CLASS_NAME::RemoveMinimum()
{
	if (fMinLastElement == 0) {
		ASSERT(fMaxLastElement == 1);
		RemoveMaximum();
		return;
	}

#if KDEBUG
	Element* element = PeekMinimum();
	MinMaxHeapLink<Element, Key>* link = sGetLink(element);
	ASSERT(link->fIndex != -1);
	link->fIndex = -1;
#endif

	_RemoveLast(true);
}


MIN_MAX_HEAP_TEMPLATE_LIST
void
MIN_MAX_HEAP_CLASS_NAME::RemoveMaximum()
{
	if (fMaxLastElement == 0) {
		ASSERT(fMinLastElement == 1);
		RemoveMinimum();
		return;
	}

#if KDEBUG
	Element* element = PeekMaximum();
	MinMaxHeapLink<Element, Key>* link = sGetLink(element);
	ASSERT(link->fIndex != -1);
	link->fIndex = -1;
#endif

	_RemoveLast(false);
}


MIN_MAX_HEAP_TEMPLATE_LIST
status_t
MIN_MAX_HEAP_CLASS_NAME::Insert(Element* element, Key key)
{
	if (min_c(fMinLastElement, fMaxLastElement) == fSize) {
		ASSERT(max_c(fMinLastElement, fMaxLastElement) == fSize);
		status_t result = _GrowHeap();
		if (result != B_OK)
			return result;
	}

	ASSERT(fMinLastElement < fSize || fMaxLastElement < fSize);

	MinMaxHeapLink<Element, Key>* link = sGetLink(element);

	ASSERT(link->fIndex == -1);

	link->fMinTree = fMinLastElement < fMaxLastElement;

	int& lastElement = link->fMinTree ? fMinLastElement : fMaxLastElement;
	Element** tree = link->fMinTree ? fMinElements : fMaxElements;

	tree[lastElement] = element;
	link->fIndex = lastElement++;
	link->fKey = key;

	if (!_ChangeTree(link))
		_MoveUp(link);

	return B_OK;
}


MIN_MAX_HEAP_TEMPLATE_LIST
status_t
MIN_MAX_HEAP_CLASS_NAME::_GrowHeap(int minimalSize)
{
	minimalSize = minimalSize % 2 == 0 ? minimalSize : minimalSize + 1;
	int newSize = max_c(max_c(fSize * 4, 4), minimalSize);

	size_t arraySize = newSize * sizeof(Element*);
	Element** newBuffer
		= reinterpret_cast<Element**>(realloc(fMinElements, arraySize));
	if (newBuffer == NULL)
		return B_NO_MEMORY;
	fMinElements = newBuffer;

	newBuffer += newSize / 2;
	if (fMaxLastElement > 0)
		memcpy(newBuffer, fMinElements + fSize, fSize * sizeof(Element*));
	fMaxElements = newBuffer;

	fSize = newSize / 2;
	return B_OK;
}


MIN_MAX_HEAP_TEMPLATE_LIST
void
MIN_MAX_HEAP_CLASS_NAME::_MoveUp(MinMaxHeapLink<Element, Key>* link)
{
	Element** tree = link->fMinTree ? fMinElements : fMaxElements;
	while (true) {
		if (link->fIndex <= 0)
			break;

		int parent = (link->fIndex - 1) / 2;
		bool isSmaller = sCompare(link->fKey, sGetLink(tree[parent])->fKey);
		if (isSmaller ^ !link->fMinTree) {
			ASSERT(sGetLink(tree[parent])->fIndex == parent);
			sGetLink(tree[parent])->fIndex = link->fIndex;

			Element* element = tree[link->fIndex];
			tree[link->fIndex] = tree[parent];
			tree[parent] = element;

			link->fIndex = parent;
		} else
			break;
	}
}


MIN_MAX_HEAP_TEMPLATE_LIST
void
MIN_MAX_HEAP_CLASS_NAME::_MoveDown(MinMaxHeapLink<Element, Key>* link)
{
	int current;

	int lastElement = link->fMinTree ? fMinLastElement : fMaxLastElement;
	Element** tree = link->fMinTree ? fMinElements : fMaxElements;
	while (true) {
		current = link->fIndex;

		int child = 2 * link->fIndex + 1;
		if (child < lastElement) {
			bool isSmaller = sCompare(sGetLink(tree[child])->fKey, link->fKey);
			if (isSmaller ^ !link->fMinTree)
				current = child;
		}

		child = 2 * link->fIndex + 2;
		if (child < lastElement) {
			bool isSmaller = sCompare(sGetLink(tree[child])->fKey,
					sGetLink(tree[current])->fKey);
			if (isSmaller ^ !link->fMinTree)
				current = child;
		}

		if (link->fIndex == current)
			break;

		ASSERT(sGetLink(tree[current])->fIndex == current);
		sGetLink(tree[current])->fIndex = link->fIndex;

		Element* element = tree[link->fIndex];
		tree[link->fIndex] = tree[current];
		tree[current] = element;

		link->fIndex = current;
	}

	if (2 * link->fIndex + 1 >= lastElement)
		_ChangeTree(link);
}


MIN_MAX_HEAP_TEMPLATE_LIST
bool
MIN_MAX_HEAP_CLASS_NAME::_ChangeTree(MinMaxHeapLink<Element, Key>* link)
{
	int otherLastElement = link->fMinTree ? fMaxLastElement : fMinLastElement;

	Element** currentTree = link->fMinTree ? fMinElements : fMaxElements;
	Element** otherTree = link->fMinTree ? fMaxElements : fMinElements;

	if (otherLastElement <= 0) {
		ASSERT(link->fMinTree ? fMinLastElement : fMaxLastElement == 1);
		return false;
	}

	ASSERT((link->fIndex - 1) / 2 < otherLastElement);

	Element* predecessor;
	if (2 * link->fIndex + 1 < otherLastElement) {
		predecessor = otherTree[2 * link->fIndex + 1];
		ASSERT(sGetLink(predecessor)->fIndex == 2 * link->fIndex + 1);
	} else if (link->fIndex < otherLastElement) {
		predecessor = otherTree[link->fIndex];
		ASSERT(sGetLink(predecessor)->fIndex == link->fIndex);
	} else {
		predecessor = otherTree[(link->fIndex - 1) / 2];
		ASSERT(sGetLink(predecessor)->fIndex == (link->fIndex - 1) / 2);
	}
	MinMaxHeapLink<Element, Key>* predecessorLink = sGetLink(predecessor);

	bool isSmaller = sCompare(predecessorLink->fKey, link->fKey);
	if (isSmaller ^ !link->fMinTree) {
		Element* element = currentTree[link->fIndex];
		currentTree[link->fIndex] = otherTree[predecessorLink->fIndex];
		otherTree[predecessorLink->fIndex] = element;

		int index = link->fIndex;
		link->fIndex = predecessorLink->fIndex;
		predecessorLink->fIndex = index;

		predecessorLink->fMinTree = !predecessorLink->fMinTree;
		link->fMinTree = !link->fMinTree;

		_MoveUp(link);
		return true;
	}

	return false;
}


MIN_MAX_HEAP_TEMPLATE_LIST
void
MIN_MAX_HEAP_CLASS_NAME::_RemoveLast(bool minTree)
{
	bool deleteMin = fMaxLastElement < fMinLastElement;

	Element** tree = deleteMin ? fMinElements : fMaxElements;
	int& lastElement = deleteMin ? fMinLastElement : fMaxLastElement;

	ASSERT(lastElement > 0);
	lastElement--;
	if (lastElement == 0 && deleteMin == minTree)
		return;

	Element* element = tree[lastElement];

	if (minTree)
		fMinElements[0] = element;
	else
		fMaxElements[0] = element;

	MinMaxHeapLink<Element, Key>* link = sGetLink(element);
	link->fIndex = 0;
	link->fMinTree = minTree;
	_MoveDown(link);
}


MIN_MAX_HEAP_TEMPLATE_LIST
Compare MIN_MAX_HEAP_CLASS_NAME::sCompare;

MIN_MAX_HEAP_TEMPLATE_LIST
GetLink MIN_MAX_HEAP_CLASS_NAME::sGetLink;


#endif	// KERNEL_UTIL_MIN_MAX_HEAP_H

