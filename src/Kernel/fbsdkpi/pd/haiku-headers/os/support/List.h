/*
 * Copyright 2001-2009 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BE_LIST_H
#define _BE_LIST_H


#include <SupportDefs.h>


class BList {
public:
								BList(int32 blockSize = 20);
								BList(const BList& other);
	virtual						~BList();

			BList&				operator=(const BList& other);
			bool				operator==(const BList& other) const;
			bool				operator!=(const BList& other) const;

	// Adding and removing items
			bool				AddItem(void* item, int32 index);
			bool				AddItem(void* item);
			bool				AddList(const BList* list, int32 index);
			bool				AddList(const BList* list);

			bool				RemoveItem(void* item);
			void*				RemoveItem(int32 index);
			bool				RemoveItems(int32 index, int32 count);
			bool				ReplaceItem(int32 index, void* item);

			void				MakeEmpty();

	// Reorder items
			void				SortItems(int (*compareFunc)(const void*,
									const void*));
			bool				SwapItems(int32 indexA, int32 indexB);
			bool				MoveItem(int32 from, int32 to);

	// Retrieve items
			void*				ItemAt(int32 index) const;
			void*				FirstItem() const;
			void*				ItemAtFast(int32 index) const;
									// does not check the array bounds!

			void*				LastItem() const;
			void*				Items() const;

	// Query
			bool				HasItem(void* item) const;
			bool				HasItem(const void* item) const;
			int32				IndexOf(void* item) const;
			int32				IndexOf(const void* item) const;
			int32				CountItems() const;
			bool				IsEmpty() const;

	// Iteration
			void				DoForEach(bool (*func)(void* item));
			void				DoForEach(bool (*func)(void* item,
									void* arg2), void* arg2);

private:
	virtual	void				_ReservedList1();
	virtual	void				_ReservedList2();

			bool				_ResizeArray(int32 count);

private:
			void**				fObjectList;
			int32				fPhysicalSize;
			int32				fItemCount;
			int32				fBlockSize;
			int32				fResizeThreshold;

			uint32				_reserved[1];
};


#endif // _BE_LIST_H
