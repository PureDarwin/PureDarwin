/*
 * Copyright 2009 Colin Günther, coling@gmx.de
 * All Rights Reserved. Distributed under the terms of the MIT License.
 */
#ifndef UNIT_H_
#define UNIT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <compat/sys/systm.h>


struct radix_bitmap;
struct unrhdr {
	struct radix_bitmap*	idBuffer;
	struct mtx*				storeMutex;
	int32					idBias;
};


status_t _new_unrhdr_buffer(struct unrhdr*, uint32);
void _delete_unrhdr_buffer_locked(struct unrhdr*);
int _alloc_unr_locked(struct unrhdr*);
void _free_unr_locked(struct unrhdr*, u_int);

#ifdef __cplusplus
}
#endif

#endif /* UNIT_H_ */
