/*
 * Copyright 2003-2009, Ingo Weinhold <ingo_weinhold@gmx.de>.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_UTIL_AVL_TREE_BASE_H
#define _KERNEL_UTIL_AVL_TREE_BASE_H


#ifndef FS_SHELL
#	include <SupportDefs.h>
#else
#	include "fssh_api_wrapper.h"
#endif


class AVLTreeIterator;


struct AVLTreeNode {
	AVLTreeNode*	parent;
	AVLTreeNode*	left;
	AVLTreeNode*	right;
	int				balance_factor;
};


class AVLTreeCompare {
public:
	virtual						~AVLTreeCompare();

	virtual	int					CompareKeyNode(const void* key,
									const AVLTreeNode* node) = 0;
	virtual	int					CompareNodes(const AVLTreeNode* node1,
									const AVLTreeNode* node2) = 0;
};


class AVLTreeBase {
public:
								AVLTreeBase(AVLTreeCompare* compare);
								~AVLTreeBase();

	inline	int					Count() const	{ return fNodeCount; }
	inline	bool				IsEmpty() const	{ return (fNodeCount == 0); }
			void				MakeEmpty();

	inline	AVLTreeNode*		Root() const	{ return fRoot; }

	inline	AVLTreeNode*		LeftMost() const;
			AVLTreeNode*		LeftMost(AVLTreeNode* node) const;
	inline	AVLTreeNode*		RightMost() const;
			AVLTreeNode*		RightMost(AVLTreeNode* node) const;

			AVLTreeNode*		Previous(AVLTreeNode* node) const;
			AVLTreeNode*		Next(AVLTreeNode* node) const;

	inline	AVLTreeIterator		GetIterator() const;
	inline	AVLTreeIterator		GetIterator(AVLTreeNode* node) const;

			AVLTreeNode*		Find(const void* key) const;
			AVLTreeNode*		FindClosest(const void* key, bool less) const;

			status_t			Insert(AVLTreeNode* element);
			AVLTreeNode*		Remove(const void* key);
			bool				Remove(AVLTreeNode* element);

			void				CheckTree() const;

private:
			enum {
				NOT_FOUND		= -3,
				DUPLICATE		= -2,
				NO_MEMORY		= -1,
				OK				= 0,
				HEIGHT_CHANGED	= 1,

				LEFT			= -1,
				BALANCED		= 0,
				RIGHT			= 1,
			};

			// rotations
			void				_RotateRight(AVLTreeNode** nodeP);
			void				_RotateLeft(AVLTreeNode** nodeP);

			// insert
			int					_BalanceInsertLeft(AVLTreeNode** node);
			int					_BalanceInsertRight(AVLTreeNode** node);
			int					_Insert(AVLTreeNode* nodeToInsert);

			// remove
			int					_BalanceRemoveLeft(AVLTreeNode** node);
			int					_BalanceRemoveRight(AVLTreeNode** node);
			int					_RemoveRightMostChild(AVLTreeNode** node,
									AVLTreeNode** foundNode);
			int					_Remove(AVLTreeNode* node);

			int					_CheckTree(AVLTreeNode* parent,
									AVLTreeNode* node, int& _nodeCount) const;

			AVLTreeNode*		fRoot;
			int					fNodeCount;
			AVLTreeCompare*		fCompare;
};


// AVLTreeIterator
class AVLTreeIterator {
public:
	inline AVLTreeIterator()
		:
		fParent(NULL),
		fCurrent(NULL),
		fNext(NULL)
	{
	}

	inline AVLTreeIterator(const AVLTreeIterator& other)
		:
		fParent(other.fParent),
		fCurrent(other.fCurrent),
		fNext(other.fNext)
	{
	}

	inline AVLTreeNode* Current() const
	{
		return fCurrent;
	}

	inline bool HasNext() const
	{
		return fNext;
	}

	inline AVLTreeNode* Next()
	{
		fCurrent = fNext;

		if (fNext)
			fNext = fParent->Next(fNext);

		return fCurrent;
	}

	inline AVLTreeNode* Previous()
	{
		if (fCurrent) {
			fNext = fCurrent;
			fCurrent = fParent->Previous(fCurrent);
		} else if (fNext)
			fCurrent = fParent->Previous(fNext);

		return fCurrent;
	}

	inline AVLTreeNode* Remove()
	{
		if (!fCurrent)
			return NULL;

		AVLTreeNode* node = fCurrent;
		fCurrent = NULL;

		return (const_cast<AVLTreeBase*>(fParent)->Remove(node) ? node : NULL);
	}

	inline AVLTreeIterator& operator=(const AVLTreeIterator& other)
	{
		fParent = other.fParent;
		fCurrent = other.fCurrent;
		fNext = other.fNext;
		return *this;
	}

private:
	inline AVLTreeIterator(const AVLTreeBase* parent, AVLTreeNode* current,
			AVLTreeNode* next)
		:
		fParent(parent),
		fCurrent(current),
		fNext(next)
	{
	}

protected:
	friend class AVLTreeBase;

	const AVLTreeBase*	fParent;
	AVLTreeNode*		fCurrent;
	AVLTreeNode*		fNext;
};


inline AVLTreeNode*
AVLTreeBase::LeftMost() const
{
	return LeftMost(fRoot);
}


inline AVLTreeNode*
AVLTreeBase::RightMost() const
{
	return RightMost(fRoot);
}


// GetIterator
inline AVLTreeIterator
AVLTreeBase::GetIterator() const
{
	return AVLTreeIterator(this, NULL, LeftMost());
}


// GetIterator
inline AVLTreeIterator
AVLTreeBase::GetIterator(AVLTreeNode* node) const
{
	return AVLTreeIterator(this, node, Next(node));
}


#endif	// _KERNEL_UTIL_AVL_TREE_BASE_H
