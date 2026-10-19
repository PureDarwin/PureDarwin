/*
 * Copyright 2003-2009, Ingo Weinhold <ingo_weinhold@gmx.de>.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_UTIL_AVL_TREE_MAP_H
#define _KERNEL_UTIL_AVL_TREE_MAP_H


#include <util/MallocFreeAllocator.h>
#include <util/AVLTreeBase.h>


// strategies
namespace AVLTreeMapStrategy {
	// key orders
	template<typename Value> class Ascending;
	template<typename Value> class Descending;

	//! Automatic node strategy (works like STL containers do)
	template <typename Key, typename Value,
			  typename KeyOrder = Ascending<Key>,
			  template <typename> class Allocator = MallocFreeAllocator>
	class Auto;

/*!	NodeStrategy must implement this interface:
	typename Node;
	inline Node* Allocate(const Key& key, const Value& value)
	inline void Free(Node* node)
	inline const Key GetKey(const Node* node) const
	inline Value& GetValue(Node* node) const
	inline AVLTreeNode* GetAVLTreeNode(Node* node) const
	inline Node* GetNode(AVLTreeNode* node) const
	inline int CompareKeyNode(const Key& a, const Node* b)
	inline int CompareNodes(const Node* a, const Node* b)
*/
}


// for convenience
#define _AVL_TREE_MAP_TEMPLATE_LIST template<typename Key, typename Value, \
	typename NodeStrategy>
#define _AVL_TREE_MAP_CLASS_NAME AVLTreeMap<Key, Value, NodeStrategy>


// AVLTreeMap
template<typename Key, typename Value,
		 typename NodeStrategy = AVLTreeMapStrategy::Auto<Key, Value> >
class AVLTreeMap : protected AVLTreeCompare {
private:
	typedef typename NodeStrategy::Node	Node;
	typedef _AVL_TREE_MAP_CLASS_NAME	Class;

public:
	class Iterator;
	class ConstIterator;

public:
								AVLTreeMap(const NodeStrategy& strategy
									= NodeStrategy());
	virtual						~AVLTreeMap();

	inline	int					Count() const	{ return fTree.Count(); }
	inline	bool				IsEmpty() const	{ return fTree.IsEmpty(); }
	inline	void				MakeEmpty();

			Node*				RootNode() const;

			Node*				Previous(Node* node) const;
			Node*				Next(Node* node) const;

	inline	Iterator			GetIterator();
	inline	ConstIterator		GetIterator() const;

	inline	Iterator			GetIterator(Node* node);
	inline	ConstIterator		GetIterator(Node* node) const;

			Iterator			Find(const Key& key);
			Iterator			FindClose(const Key& key, bool less);

			status_t			Insert(const Key& key, const Value& value,
									Iterator* iterator);
			status_t			Insert(const Key& key, const Value& value,
									Node** _node = NULL);
			status_t			Remove(const Key& key);
			status_t			Remove(Node* node);

			const NodeStrategy&	GetNodeStrategy() const	{ return fStrategy; }

protected:
	// AVLTreeCompare
	virtual	int					CompareKeyNode(const void* key,
									const AVLTreeNode* node);
	virtual	int					CompareNodes(const AVLTreeNode* node1,
									const AVLTreeNode* node2);

			void				_FreeTree(AVLTreeNode* node);

	// strategy shortcuts
	inline	Node*				_Allocate(const Key& key, const Value& value);
	inline	void				_Free(Node* node);
	inline	Key					_GetKey(Node* node) const;
	inline	Value&				_GetValue(Node* node) const;
	inline	AVLTreeNode*		_GetAVLTreeNode(const Node* node) const;
	inline	Node*				_GetNode(const AVLTreeNode* node) const;
	inline	int					_CompareKeyNode(const Key& a, const Node* b);
	inline	int					_CompareNodes(const Node* a, const Node* b);

protected:
			friend class Iterator;
			friend class ConstIterator;

			AVLTreeBase			fTree;
			NodeStrategy		fStrategy;

public:
	// Iterator
	// (need to implement it here, otherwise gcc 2.95.3 chokes)
	class Iterator : public ConstIterator {
	public:
		inline Iterator()
			: ConstIterator()
		{
		}

		inline Iterator(const Iterator& other)
			: ConstIterator(other)
		{
		}

		inline void Remove()
		{
			if (AVLTreeNode* node = ConstIterator::fTreeIterator.Remove()) {
				_AVL_TREE_MAP_CLASS_NAME* parent
					= const_cast<_AVL_TREE_MAP_CLASS_NAME*>(
						ConstIterator::fParent);
				parent->_Free(parent->_GetNode(node));
			}
		}

	private:
		inline Iterator(_AVL_TREE_MAP_CLASS_NAME* parent,
			const AVLTreeIterator& treeIterator)
			: ConstIterator(parent, treeIterator)
		{
		}

		friend class _AVL_TREE_MAP_CLASS_NAME;
	};
};


// ConstIterator
_AVL_TREE_MAP_TEMPLATE_LIST
class _AVL_TREE_MAP_CLASS_NAME::ConstIterator {
public:
	inline ConstIterator()
		: fParent(NULL),
		  fTreeIterator()
	{
	}

	inline ConstIterator(const ConstIterator& other)
		: fParent(other.fParent),
		  fTreeIterator(other.fTreeIterator)
	{
	}

	inline bool HasCurrent() const
	{
		return fTreeIterator.Current();
	}

	inline Key CurrentKey()
	{
		if (AVLTreeNode* node = fTreeIterator.Current())
			return fParent->_GetKey(fParent->_GetNode(node));
		return Key();
	}

	inline Value Current()
	{
		if (AVLTreeNode* node = fTreeIterator.Current())
			return fParent->_GetValue(fParent->_GetNode(node));
		return Value();
	}

	inline Value* CurrentValuePointer()
	{
		if (AVLTreeNode* node = fTreeIterator.Current())
			return &fParent->_GetValue(fParent->_GetNode(node));
		return NULL;
	}

	inline Node* CurrentNode()
	{
		if (AVLTreeNode* node = fTreeIterator.Current())
			return fParent->_GetNode(node);
		return NULL;
	}

	inline bool HasNext() const
	{
		return fTreeIterator.HasNext();
	}

	inline Value Next()
	{
		if (AVLTreeNode* node = fTreeIterator.Next())
			return fParent->_GetValue(fParent->_GetNode(node));
		return Value();
	}

	inline Value* NextValuePointer()
	{
		if (AVLTreeNode* node = fTreeIterator.Next())
			return &fParent->_GetValue(fParent->_GetNode(node));
		return NULL;
	}

	inline Value Previous()
	{
		if (AVLTreeNode* node = fTreeIterator.Previous())
			return fParent->_GetValue(fParent->_GetNode(node));
		return Value();
	}

	inline ConstIterator& operator=(const ConstIterator& other)
	{
		fParent = other.fParent;
		fTreeIterator = other.fTreeIterator;
		return *this;
	}

protected:
	inline ConstIterator(const _AVL_TREE_MAP_CLASS_NAME* parent,
		const AVLTreeIterator& treeIterator)
	{
		fParent = parent;
		fTreeIterator = treeIterator;
	}

	friend class _AVL_TREE_MAP_CLASS_NAME;

	const _AVL_TREE_MAP_CLASS_NAME*	fParent;
	AVLTreeIterator					fTreeIterator;
};


// constructor
_AVL_TREE_MAP_TEMPLATE_LIST
_AVL_TREE_MAP_CLASS_NAME::AVLTreeMap(const NodeStrategy& strategy)
	: fTree(this),
	  fStrategy(strategy)
{
}


// destructor
_AVL_TREE_MAP_TEMPLATE_LIST
_AVL_TREE_MAP_CLASS_NAME::~AVLTreeMap()
{
}


// MakeEmpty
_AVL_TREE_MAP_TEMPLATE_LIST
inline void
_AVL_TREE_MAP_CLASS_NAME::MakeEmpty()
{
	AVLTreeNode* root = fTree.Root();
	_FreeTree(root);
	fTree.MakeEmpty();
}


// RootNode
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Node*
_AVL_TREE_MAP_CLASS_NAME::RootNode() const
{
	if (AVLTreeNode* root = fTree.Root())
		return _GetNode(root);
	return NULL;
}


// Previous
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Node*
_AVL_TREE_MAP_CLASS_NAME::Previous(Node* node) const
{
	if (node == NULL)
		return NULL;

	AVLTreeNode* treeNode = fTree.Previous(_GetAVLTreeNode(node));
	return treeNode != NULL ? _GetNode(treeNode) : NULL;
}


// Next
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Node*
_AVL_TREE_MAP_CLASS_NAME::Next(Node* node) const
{
	if (node == NULL)
		return NULL;

	AVLTreeNode* treeNode = fTree.Next(_GetAVLTreeNode(node));
	return treeNode != NULL ? _GetNode(treeNode) : NULL;
}


// GetIterator
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Iterator
_AVL_TREE_MAP_CLASS_NAME::GetIterator()
{
	return Iterator(this, fTree.GetIterator());
}


// GetIterator
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::ConstIterator
_AVL_TREE_MAP_CLASS_NAME::GetIterator() const
{
	return ConstIterator(this, fTree.GetIterator());
}


// GetIterator
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Iterator
_AVL_TREE_MAP_CLASS_NAME::GetIterator(Node* node)
{
	return Iterator(this, fTree.GetIterator(_GetAVLTreeNode(node)));
}


// GetIterator
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::ConstIterator
_AVL_TREE_MAP_CLASS_NAME::GetIterator(Node* node) const
{
	return ConstIterator(this, fTree.GetIterator(_GetAVLTreeNode(node)));
}


// Find
_AVL_TREE_MAP_TEMPLATE_LIST
typename _AVL_TREE_MAP_CLASS_NAME::Iterator
_AVL_TREE_MAP_CLASS_NAME::Find(const Key& key)
{
	if (AVLTreeNode* node = fTree.Find(&key))
		return Iterator(this, fTree.GetIterator(node));
	return Iterator();
}


// FindClose
_AVL_TREE_MAP_TEMPLATE_LIST
typename _AVL_TREE_MAP_CLASS_NAME::Iterator
_AVL_TREE_MAP_CLASS_NAME::FindClose(const Key& key, bool less)
{
	if (AVLTreeNode* node = fTree.FindClosest(&key, less))
		return Iterator(this, fTree.GetIterator(node));
	return Iterator();
}


// Insert
_AVL_TREE_MAP_TEMPLATE_LIST
status_t
_AVL_TREE_MAP_CLASS_NAME::Insert(const Key& key, const Value& value,
	Iterator* iterator)
{
	// allocate a node
	Node* userNode = _Allocate(key, value);
	if (!userNode)
		return B_NO_MEMORY;

	// insert node
	AVLTreeNode* node = _GetAVLTreeNode(userNode);
	status_t error = fTree.Insert(node);
	if (error != B_OK) {
		_Free(userNode);
		return error;
	}

	if (iterator)
		*iterator = Iterator(this, fTree.GetIterator(node));

	return B_OK;
}


// Insert
_AVL_TREE_MAP_TEMPLATE_LIST
status_t
_AVL_TREE_MAP_CLASS_NAME::Insert(const Key& key, const Value& value,
	Node** _node)
{
	// allocate a node
	Node* userNode = _Allocate(key, value);
	if (!userNode)
		return B_NO_MEMORY;

	// insert node
	AVLTreeNode* node = _GetAVLTreeNode(userNode);
	status_t error = fTree.Insert(node);
	if (error != B_OK) {
		_Free(userNode);
		return error;
	}

	if (_node != NULL)
		*_node = userNode;

	return B_OK;
}


// Remove
_AVL_TREE_MAP_TEMPLATE_LIST
status_t
_AVL_TREE_MAP_CLASS_NAME::Remove(const Key& key)
{
	AVLTreeNode* node = fTree.Remove(&key);
	if (!node)
		return B_ENTRY_NOT_FOUND;

	_Free(_GetNode(node));
	return B_OK;
}


// Remove
_AVL_TREE_MAP_TEMPLATE_LIST
status_t
_AVL_TREE_MAP_CLASS_NAME::Remove(Node* node)
{
	if (!fTree.Remove(node))
		return B_ENTRY_NOT_FOUND;

	_Free(node);
	return B_OK;
}


// CompareKeyNode
_AVL_TREE_MAP_TEMPLATE_LIST
int
_AVL_TREE_MAP_CLASS_NAME::CompareKeyNode(const void* key,
	const AVLTreeNode* node)
{
	return _CompareKeyNode(*(const Key*)key, _GetNode(node));
}


// CompareNodes
_AVL_TREE_MAP_TEMPLATE_LIST
int
_AVL_TREE_MAP_CLASS_NAME::CompareNodes(const AVLTreeNode* node1,
	const AVLTreeNode* node2)
{
	return _CompareNodes(_GetNode(node1), _GetNode(node2));
}


// _Allocate
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Node*
_AVL_TREE_MAP_CLASS_NAME::_Allocate(const Key& key, const Value& value)
{
	return fStrategy.Allocate(key, value);
}


// _Free
_AVL_TREE_MAP_TEMPLATE_LIST
inline void
_AVL_TREE_MAP_CLASS_NAME::_Free(Node* node)
{
	fStrategy.Free(node);
}


// _GetKey
_AVL_TREE_MAP_TEMPLATE_LIST
inline Key
_AVL_TREE_MAP_CLASS_NAME::_GetKey(Node* node) const
{
	return fStrategy.GetKey(node);
}


// _GetValue
_AVL_TREE_MAP_TEMPLATE_LIST
inline Value&
_AVL_TREE_MAP_CLASS_NAME::_GetValue(Node* node) const
{
	return fStrategy.GetValue(node);
}


// _GetAVLTreeNode
_AVL_TREE_MAP_TEMPLATE_LIST
inline AVLTreeNode*
_AVL_TREE_MAP_CLASS_NAME::_GetAVLTreeNode(const Node* node) const
{
	return fStrategy.GetAVLTreeNode(const_cast<Node*>(node));
}


// _GetNode
_AVL_TREE_MAP_TEMPLATE_LIST
inline typename _AVL_TREE_MAP_CLASS_NAME::Node*
_AVL_TREE_MAP_CLASS_NAME::_GetNode(const AVLTreeNode* node) const
{
	return fStrategy.GetNode(const_cast<AVLTreeNode*>(node));
}


// _CompareKeyNode
_AVL_TREE_MAP_TEMPLATE_LIST
inline int
_AVL_TREE_MAP_CLASS_NAME::_CompareKeyNode(const Key& a, const Node* b)
{
	return fStrategy.CompareKeyNode(a, b);
}


// _CompareNodes
_AVL_TREE_MAP_TEMPLATE_LIST
inline int
_AVL_TREE_MAP_CLASS_NAME::_CompareNodes(const Node* a, const Node* b)
{
	return fStrategy.CompareNodes(a, b);
}


// _FreeTree
_AVL_TREE_MAP_TEMPLATE_LIST
void
_AVL_TREE_MAP_CLASS_NAME::_FreeTree(AVLTreeNode* node)
{
	if (node) {
		_FreeTree(node->left);
		_FreeTree(node->right);
		_Free(_GetNode(node));
	}
}


// #pragma mark - strategy parameters

// Ascending
namespace AVLTreeMapStrategy {
template<typename Value>
class Ascending {
public:
	inline int operator()(const Value &a, const Value &b) const
	{
		if (a < b)
			return -1;
		else if (a > b)
			return 1;
		return 0;
	}
};
}


// Descending
namespace AVLTreeMapStrategy {
template<typename Value>
class Descending {
public:
	inline int operator()(const Value &a, const Value &b) const
	{
		if (a < b)
			return -1;
		else if (a > b)
			return 1;
		return 0;
	}
};
}


// #pragma mark - strategies


// Auto
namespace AVLTreeMapStrategy {
template <typename Key, typename Value, typename KeyOrder,
		  template <typename> class Allocator>
class Auto {
public:
	struct Node : AVLTreeNode {
		Node(const Key &key, const Value &value)
			: AVLTreeNode(),
			  key(key),
			  value(value)
		{
		}

		Key		key;
		Value	value;
	};

	inline Node* Allocate(const Key& key, const Value& value)
	{
		Node* result = fAllocator.Allocate();
		if (result)
			fAllocator.Construct(result, key, value);
		return result;
	}

	inline void Free(Node* node)
	{
		fAllocator.Destruct(node);
		fAllocator.Deallocate(node);
	}

	inline const Key& GetKey(const Node* node) const
	{
		return node->key;
	}

	inline Value& GetValue(Node* node) const
	{
		return node->value;
	}

	inline AVLTreeNode* GetAVLTreeNode(Node* node) const
	{
		return node;
	}

	inline Node* GetNode(AVLTreeNode* node) const
	{
		return static_cast<Node*>(node);
	}

	inline int CompareKeyNode(const Key& a, const Node* b) const
	{
		return fCompare(a, GetKey(b));
	}

	inline int CompareNodes(const Node* a, const Node* b) const
	{
		return fCompare(GetKey(a), GetKey(b));
	}

private:
	KeyOrder		fCompare;
	Allocator<Node>	fAllocator;
};
}

#endif	// _KERNEL_UTIL_AVL_TREE_MAP_H
