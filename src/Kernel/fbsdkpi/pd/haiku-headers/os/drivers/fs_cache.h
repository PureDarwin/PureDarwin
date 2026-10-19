/*
 * Copyright 2004-2020, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FS_CACHE_H
#define _FS_CACHE_H

/*! File System File and Block Caches */


#include <fs_interface.h>


/* transaction events */
enum {
	TRANSACTION_WRITTEN	= 0x01,
	TRANSACTION_ABORTED	= 0x02,
	TRANSACTION_ENDED	= 0x04,
	TRANSACTION_IDLE	= 0x08
};

/* file map modes */
enum {
	FILE_MAP_CACHE_ON_DEMAND	= 0x01,	/* default mode */
	FILE_MAP_CACHE_ALL			= 0x02
};

typedef void (*transaction_notification_hook)(int32 id, int32 event,
	void *data);

#ifdef __cplusplus
extern "C" {
#endif

/* transactions */
extern int32 cache_start_transaction(void *cache);
extern status_t cache_sync_transaction(void *cache, int32 id);
extern status_t cache_end_transaction(void *cache, int32 id,
					transaction_notification_hook hook, void *data);
extern status_t cache_abort_transaction(void *cache, int32 id);
extern int32 cache_detach_sub_transaction(void *cache, int32 id,
					transaction_notification_hook hook, void *data);
extern status_t cache_abort_sub_transaction(void *cache, int32 id);
extern status_t cache_start_sub_transaction(void *cache, int32 id);
extern status_t cache_add_transaction_listener(void *cache, int32 id,
					int32 events, transaction_notification_hook hook,
					void *data);
extern status_t cache_remove_transaction_listener(void *cache, int32 id,
					transaction_notification_hook hook, void *data);
extern status_t cache_next_block_in_transaction(void *cache, int32 id,
					bool mainOnly, long *_cookie, off_t *_blockNumber,
					void **_data, void **_unchangedData);
extern int32 cache_blocks_in_transaction(void *cache, int32 id);
extern int32 cache_blocks_in_main_transaction(void *cache, int32 id);
extern int32 cache_blocks_in_sub_transaction(void *cache, int32 id);
extern bool cache_has_block_in_transaction(void* cache, int32 id, off_t blockNumber);

/* block cache */
extern void block_cache_delete(void *cache, bool allowWrites);
extern void *block_cache_create(int fd, off_t numBlocks, size_t blockSize,
					bool readOnly);
extern status_t block_cache_sync(void *cache);
extern status_t block_cache_sync_etc(void *cache, off_t blockNumber,
					size_t numBlocks);
extern void block_cache_discard(void *cache, off_t blockNumber,
					size_t numBlocks);
extern status_t block_cache_make_writable(void *cache, off_t blockNumber,
					int32 transaction);
extern status_t block_cache_get_writable_etc(void *cache, off_t blockNumber,
					int32 transaction, void** _block);
extern void *block_cache_get_writable(void *cache, off_t blockNumber,
					int32 transaction);
extern void *block_cache_get_empty(void *cache, off_t blockNumber,
					int32 transaction);
extern status_t block_cache_get_etc(void *cache, off_t blockNumber,
					const void** _block);
extern const void *block_cache_get(void *cache, off_t blockNumber);
extern status_t block_cache_set_dirty(void *cache, off_t blockNumber,
					bool isDirty, int32 transaction);
extern void block_cache_put(void *cache, off_t blockNumber);
extern status_t block_cache_prefetch(void* cache, off_t blockNumber, size_t* _numBlocks);

/* file cache */
extern void *file_cache_create(dev_t mountID, ino_t vnodeID, off_t size);
extern void file_cache_delete(void *cacheRef);
extern void file_cache_enable(void *cacheRef);
extern bool file_cache_is_enabled(void *cacheRef);
extern status_t file_cache_disable(void *cacheRef);
extern status_t file_cache_set_size(void *cacheRef, off_t size);
extern status_t file_cache_sync(void *cache);

extern status_t file_cache_read(void *cacheRef, off_t offset,
					void *bufferBase, size_t *_size);
extern status_t file_cache_write(void *cacheRef, off_t offset,
					const void *buffer, size_t *_size);

/* file map */
extern void *file_map_create(dev_t mountID, ino_t vnodeID, off_t size);
extern void file_map_delete(void *map);
extern void file_map_set_size(void *map, off_t size);
extern void file_map_invalidate(void *map, off_t offset, off_t size);
extern status_t file_map_set_mode(void *map, uint32 mode);
extern status_t file_map_translate(void *map, off_t offset, size_t size,
					struct file_io_vec *vecs, size_t *_count, size_t align);

/* entry cache */
extern status_t entry_cache_add(dev_t mountID, ino_t dirID, const char* name,
					ino_t nodeID);
extern status_t entry_cache_add_missing(dev_t mountID, ino_t dirID,
					const char* name);
extern status_t entry_cache_remove(dev_t mountID, ino_t dirID,
					const char* name);

#ifdef __cplusplus
}
#endif

#endif	/* _FS_CACHE_H */
