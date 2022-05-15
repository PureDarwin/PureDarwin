/*
 * Copyright (c) 2003-2011 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "table.h"
#include "notify_internal.h"

void _nc_table_init(table_t *t, size_t key_offset) {
	os_set_init(&t->set, NULL);
	t->key_offset = key_offset;
}

void _nc_table_init_n(table_n_t *t, size_t key_offset) {
	os_set_init(&t->set, NULL);
	t->key_offset = key_offset;
}

void _nc_table_init_64(table_64_t *t, size_t key_offset) {
	os_set_init(&t->set, NULL);
	t->key_offset = key_offset;
}

void _nc_table_insert(table_t *t, char **key) {
	os_set_insert(&t->set, (void *)key);
}

void _nc_table_insert_n(table_n_t *t, uint32_t *key)  {
	os_set_insert(&t->set, (void *)key);
}

void _nc_table_insert_64(table_64_t *t, uint64_t *key)  {
	os_set_insert(&t->set, (void *)key);
}

void *_nc_table_find(table_t *t, const char *key) {
	void *offset_result = os_set_find(&t->set, key);
	return (offset_result != NULL) ? (void *)((uintptr_t)offset_result - (uintptr_t)t->key_offset) : NULL;
}

void *_nc_table_find_n(table_n_t *t, uint32_t key) {
	void *offset_result = os_set_find(&t->set, key);
	return (offset_result != NULL) ? (void *)((uintptr_t)offset_result - (uintptr_t)t->key_offset)  : NULL;
}

void *_nc_table_find_64(table_64_t *t, uint64_t key) {
	void *offset_result = os_set_find(&t->set, key);
	return (offset_result != NULL) ? (void *)((uintptr_t)offset_result - (uintptr_t)t->key_offset)  : NULL;
}

void _nc_table_delete(table_t *t, const char *key) {
	(void)os_set_delete(&t->set, key);
}

void _nc_table_delete_n(table_n_t *t, uint32_t key) {
	(void)os_set_delete(&t->set, key);
}

void _nc_table_delete_64(table_64_t *t, uint64_t key) {
	(void)os_set_delete(&t->set, key);
}

typedef bool (^payload_handler_t) (void *);

void _nc_table_foreach(table_t *t, OS_NOESCAPE payload_handler_t handler) {
	os_set_foreach(&t->set, ^bool (const char **_ptr) {
		return handler((void *)((uintptr_t)_ptr - (uintptr_t)t->key_offset));
	});
}

void _nc_table_foreach_n(table_n_t *t, OS_NOESCAPE payload_handler_t handler) {
	os_set_foreach(&t->set, ^bool (uint32_t *_ptr) {
		return handler((void *)((uintptr_t)_ptr - (uintptr_t)t->key_offset));
	});
}

void _nc_table_foreach_64(table_64_t *t,OS_NOESCAPE payload_handler_t handler) {
	os_set_foreach(&t->set, ^bool (uint64_t *_ptr) {
		return handler((void *)((uintptr_t)_ptr - (uintptr_t)t->key_offset));
	});
}
