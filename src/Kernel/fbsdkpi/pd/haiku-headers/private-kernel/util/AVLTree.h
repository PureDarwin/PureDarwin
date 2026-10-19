/*
 * Copyright 2003-2009, Ingo Weinhold <ingo_weinhold@gmx.de>.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_UTIL_AVL_TREE_H
#define _KERNEL_UTIL_AVL_TREE_H


#include <util/AVLTreeBase.h>


/*
	To be implemented by the definition:

	typedef int	Key;
	typedef Foo	Value;

	AVLTreeNode*		GetAVLTreeNode(Value* value) const;
	Value*				GetValue(AVLTreeNode* node) const;
	int					Compare(const Key& a, const Value* b) const;
	int					Compare(const Value* a, const Value* b) const;
*/



template<typename Definition>
class AVLTree : protected AVLTreeCompare {
private:
	typedef typename Definition::Key	Key;
	typedef typename Definition::Value	Value;

public:
	class Iterator;
	class ConstIterator;

public:
								AVLTree();
								AVLTree(const Definition& definition);
	virtual						~AVLTree();

	inline	int					Count() const	{ return fTree.Count(); }
	inline	bool				IsEmpty() const	{ return fTree.IsEmpty(); }
	inline	void				Clear();

			Value*				RootNode() const;

			Value*				Previous(Value* value) const;
			Value*				Next(Value* value) const;

			Value*				LeftMost() const;
			Value*				LeftMost(Value* value) const;
			Value*				RightMost() const;
			Value*				RightMost(Value* value) const;

	inline	Iterator			GetIterator();
	inline	ConstIterator		GetIterator() const;

	inline	Iterator			GetIterator(Value* value);
	inline	ConstIterator		GetIterator(Value* value) const;

			Value*				Find(const Key& key) const;
			Value*				FindClosest(const Key& key, bool less) const;

			status_t			Insert(Value* value, Iterator* iterator = NULL);
			Value*				Remove(const Key& key);
			bool				Remove(Value* key);

			void				CheckTree() const	{ fTree.CheckTree(); }

protected:
	// AVLTreeCompare
	virtual	int					CompareKeyNode(const void* key,
									const AVLTreeNode* node);
	virtual	int					CompareNodes(const AVLTreeNode* node1,
									const AVLTreeNode* node2);

	// definition shortcuts
	inline	AVLTreeNode*		_GetAVLTreeNode(Value* value) const;
	inline	Value*				_GetValue(const AVLTreeNode* node) const;
	inline	int					_Compare(const Key& a, const Value* b);
	inline	int					_Compare(const Value* a, const Value* b);

protected:
			friend class Iterator;
			friend class ConstIterator;

			AVLTreeBase			fTree;
			Definition			fDefinition;

public:
	// (need to implement it here, otherwise gcc 2.95.3 chokes)
	class Iterator : public ConstIterator {
	public:
		inline Iterator()
			:
			ConstIterator()
		{
		}

		inline Iterator(const Iterator& other)
			:
			ConstIterator(other)
		{
		}

		inline void Remove()
		{
			ConstIterator::fTreeIterator.Remove();
		}

	private:
		inline Iterator(AVLTree<Definition>* parent,
			const AVLTreeIterator& treeIterator)
			: ConstIterator(parent, treeIterator)
		{
		}

		friend class AVLTree<Definition>;
	};
};


template<typename Definition>
class AVLTree<Definition>::ConstIterator {
public:
	inline ConstIterator()
		:
		fParent(NULL),
		fTreeIterator()
	{
	}

	inline ConstIterator(const ConstIterator& other)
		:
		fParent(other.fParent),
		fTreeIterator(other.fTreeIterator)
	{
	}

	inline bool HasCurrent() const
	{
		return fTreeIterator.Current();
	}

	inline Value* Current()
	{
		if (AVLTreeNode* node = fTreeIterator.Current())
			return fParent->_GetValue(node);
		return NULL;
	}

	inline bool HasNext() const
	{
		return fTreeIterator.HasNext();
	}

	inline Value* Next()
	{
		if (AVLTreeNode* node = fTreeIterator.Next())
			return fParent->_GetValue(node);
		return NULL;
	}

	inline Value* Previous()
	{
		if (AVLTreeNode* node = fTreeIterator.Previous())
			return fParent->_GetValue(node);
		return NULL;
	}

	inline ConstIterator& operator=(const ConstIterator& other)
	{
		fParent = other.fParent;
		fTreeIterator = other.fTreeIterator;
		return *this;
	}

protected:
	inline ConstIterator(const AVLTree<Definition>* parent,
		const AVLTreeIterator& treeIterator)
	{
		fParent = parent;
		fTreeIterator = treeIterator;
	}

	friend class AVLTree<Definition>;

	const AVLTree<Definition>*	fParent;
	AVLTreeIterator				fTreeIterator;
};


template<typename Definition>
AVLTree<Definition>::AVLTree()
	:
	fTree(this),
	fDefinition()
{
}


template<typename Definition>
AVLTree<Definition>::AVLTree(const Definition& definition)
	:
	fTree(this),
	fDefinition(definition)
{
}


template<typename Definition>
AVLTree<Definition>::~AVLTree()
{
}


template<typename Definition>
inline void
AVLTree<Definition>::Clear()
{
	fTree.MakeEmpty();
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::RootNode() const
{
	if (AVLTreeNode* root = fTree.Root())
		return _GetValue(root);
	return NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::Previous(Value* value) const
{
	if (value == NULL)
		return NULL;

	AVLTreeNode* node = fTree.Previous(_GetAVLTreeNode(value));
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::Next(Value* value) const
{
	if (value == NULL)
		return NULL;

	AVLTreeNode* node = fTree.Next(_GetAVLTreeNode(value));
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::LeftMost() const
{
	AVLTreeNode* node = fTree.LeftMost();
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::LeftMost(Value* value) const
{
	if (value == NULL)
		return NULL;

	AVLTreeNode* node = fTree.LeftMost(_GetAVLTreeNode(value));
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::RightMost() const
{
	AVLTreeNode* node = fTree.RightMost();
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::RightMost(Value* value) const
{
	if (value == NULL)
		return NULL;

	AVLTreeNode* node = fTree.RightMost(_GetAVLTreeNode(value));
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
inline typename AVLTree<Definition>::Iterator
AVLTree<Definition>::GetIterator()
{
	return Iterator(this, fTree.GetIterator());
}


template<typename Definition>
inline typename AVLTree<Definition>::ConstIterator
AVLTree<Definition>::GetIterator() const
{
	return ConstIterator(this, fTree.GetIterator());
}


template<typename Definition>
inline typename AVLTree<Definition>::Iterator
AVLTree<Definition>::GetIterator(Value* value)
{
	return Iterator(this, fTree.GetIterator(_GetAVLTreeNode(value)));
}


template<typename Definition>
inline typename AVLTree<Definition>::ConstIterator
AVLTree<Definition>::GetIterator(Value* value) const
{
	return ConstIterator(this, fTree.GetIterator(_GetAVLTreeNode(value)));
}


template<typename Definition>
typename AVLTree<Definition>::Value*
AVLTree<Definition>::Find(const Key& key) const
{
	if (AVLTreeNode* node = fTree.Find(&key))
		return _GetValue(node);
	return NULL;
}


template<typename Definition>
typename AVLTree<Definition>::Value*
AVLTree<Definition>::FindClosest(const Key& key, bool less) const
{
	if (AVLTreeNode* node = fTree.FindClosest(&key, less))
		return _GetValue(node);
	return NULL;
}


template<typename Definition>
status_t
AVLTree<Definition>::Insert(Value* value, Iterator* iterator)
{
	AVLTreeNode* node = _GetAVLTreeNode(value);
	status_t error = fTree.Insert(node);
	if (error != B_OK)
		return error;

	if (iterator != NULL)
		*iterator = Iterator(this, fTree.GetIterator(node));

	return B_OK;
}


template<typename Definition>
typename AVLTree<Definition>::Value*
AVLTree<Definition>::Remove(const Key& key)
{
	AVLTreeNode* node = fTree.Remove(&key);
	return node != NULL ? _GetValue(node) : NULL;
}


template<typename Definition>
bool
AVLTree<Definition>::Remove(Value* value)
{
	return fTree.Remove(_GetAVLTreeNode(value));
}


template<typename Definition>
int
AVLTree<Definition>::CompareKeyNode(const void* key,
	const AVLTreeNode* node)
{
	return _Compare(*(const Key*)key, _GetValue(node));
}


template<typename Definition>
int
AVLTree<Definition>::CompareNodes(const AVLTreeNode* node1,
	const AVLTreeNode* node2)
{
	return _Compare(_GetValue(node1), _GetValue(node2));
}


template<typename Definition>
inline AVLTreeNode*
AVLTree<Definition>::_GetAVLTreeNode(Value* value) const
{
	return fDefinition.GetAVLTreeNode(value);
}


template<typename Definition>
inline typename AVLTree<Definition>::Value*
AVLTree<Definition>::_GetValue(const AVLTreeNode* node) const
{
	return fDefinition.GetValue(const_cast<AVLTreeNode*>(node));
}


template<typename Definition>
inline int
AVLTree<Definition>::_Compare(const Key& a, const Value* b)
{
	return fDefinition.Compare(a, b);
}


template<typename Definition>
inline int
AVLTree<Definition>::_Compare(const Value* a, const Value* b)
{
	return fDefinition.Compare(a, b);
}


#endif	// _KERNEL_UTIL_AVL_TREE_H
