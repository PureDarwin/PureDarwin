/*
 * Copyright 2009 Colin Günther, coling@gmx.de
 * Copyright 2018, Haiku, Inc.
 * All rights reserved. Distributed under the terms of the MIT license.
 */


/*! Implementation of a number allocator.*/


extern "C" {
#include <sys/mutex.h>

#include "unit.h"
}

#include <stdlib.h>
#include <util/RadixBitmap.h>


#define ID_STORE_FULL -1


extern struct mtx gIdStoreLock;


struct unrhdr*
new_unrhdr(int low, int high, struct mtx* mutex)
{
	struct unrhdr* idStore;
	uint32 maxIdCount = high - low + 1;

	KASSERT(low <= high,
		("ID-Store: use error: %s(%u, %u)", __func__, low, high));

	idStore = (unrhdr*)malloc(sizeof *idStore);
	if (idStore == NULL)
		return NULL;

	idStore->idBuffer = radix_bitmap_create(maxIdCount);
	if (idStore->idBuffer == NULL) {
		free(idStore);
		return NULL;
	}

	if (mutex)
		idStore->storeMutex = mutex;
	else
		idStore->storeMutex = &gIdStoreLock;

	idStore->idBias = low;

	return idStore;
}


void
delete_unrhdr(struct unrhdr* idStore)
{
	KASSERT(idStore != NULL,
		("ID-Store: %s: NULL pointer as argument.", __func__));

	mtx_lock(idStore->storeMutex);

	radix_bitmap_destroy(idStore->idBuffer);
	mtx_unlock(idStore->storeMutex);

	free(idStore);
	idStore = NULL;
}


int
alloc_unr(struct unrhdr* idStore)
{
	int id;

	KASSERT(idStore != NULL,
		("ID-Store: %s: NULL pointer as argument.", __func__));

	mtx_lock(idStore->storeMutex);

	radix_slot_t slotIndex;
	id = ID_STORE_FULL;

	slotIndex = radix_bitmap_alloc(idStore->idBuffer, 1);
	if (slotIndex != RADIX_SLOT_NONE)
		id = slotIndex + idStore->idBias;

	mtx_unlock(idStore->storeMutex);

	return id;
}


void
free_unr(struct unrhdr* idStore, u_int identity)
{
	KASSERT(idStore != NULL,
		("ID-Store: %s: NULL pointer as argument.", __func__));

	mtx_lock(idStore->storeMutex);

	uint32 slotIndex = (int32)identity - idStore->idBias;
	KASSERT(slotIndex >= 0, ("ID-Store: %s(%p, %u): second "
		"parameter is not in interval.", __func__, idStore, identity));

	radix_bitmap_dealloc(idStore->idBuffer, slotIndex, 1);

	mtx_unlock(idStore->storeMutex);
}
