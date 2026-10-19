/*
 * Copyright 2005, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef RING_BUFFER_H
#define RING_BUFFER_H


#include <OS.h>


struct iovec;


struct ring_buffer {
	int32		first;
	int32		in;
	int32		size;
	uint8		buffer[0];
};


// flags for create_ring_buffer_etc()
#define RING_BUFFER_INIT_FROM_BUFFER	0x01


#ifdef __cplusplus
extern "C" {
#endif

struct ring_buffer *create_ring_buffer(size_t size);
struct ring_buffer *create_ring_buffer_etc(void *memory, size_t size,
			uint32 flags);
void delete_ring_buffer(struct ring_buffer *buffer);

void ring_buffer_clear(struct ring_buffer *buffer);
size_t ring_buffer_readable(struct ring_buffer *buffer);
size_t ring_buffer_writable(struct ring_buffer *buffer);
void ring_buffer_flush(struct ring_buffer *buffer, size_t bytes);
size_t ring_buffer_read(struct ring_buffer *buffer, uint8 *data, ssize_t length);
size_t ring_buffer_write(struct ring_buffer *buffer, const uint8 *data, ssize_t length);
ssize_t ring_buffer_user_read(struct ring_buffer *buffer, uint8 *data, ssize_t length);
ssize_t ring_buffer_user_write(struct ring_buffer *buffer, const uint8 *data, ssize_t length);
size_t ring_buffer_peek(struct ring_buffer *buffer, size_t offset, void *data, size_t length);
ssize_t ring_buffer_user_peek(struct ring_buffer *buffer, size_t offset, void *data, ssize_t length);
int32 ring_buffer_get_vecs(struct ring_buffer *buffer, struct iovec *vecs);
size_t ring_buffer_move(struct ring_buffer *to, ssize_t length, struct ring_buffer *from);

#ifdef __cplusplus
}
#endif


#endif	/* RING_BUFFER_H */
