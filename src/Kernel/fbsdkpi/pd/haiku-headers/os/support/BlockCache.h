/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BLOCK_CACHE_H
#define _BLOCK_CACHE_H


#include <SupportDefs.h>


enum {
	B_OBJECT_CACHE = 0,
	B_MALLOC_CACHE = 1
};

class BBlockCache {
public:
						BBlockCache(uint32 blockCount, size_t blockSize,
							uint32 allocationType);
	virtual				~BBlockCache();

			void*		Get(size_t blockSize);
			void		Save(void *pointer, size_t blockSize);

private:
	virtual	void		_ReservedBlockCache1();
	virtual	void		_ReservedBlockCache2();

						BBlockCache(const BBlockCache &);
						BBlockCache	&operator=(const BBlockCache &);

private:
	struct _FreeBlock;

	_FreeBlock*		fFreeList;
	const size_t	fBlockSize;
	void*			(*fAlloc)(size_t size);
	void			(*fFree)(void *pointer);

	pthread_mutex_t fLock;
	int32			fFreeBlocks;
	int32			fBlockCount;

	uint32 _reserved[6];
};


#endif	// _BLOCK_CACHE_H
