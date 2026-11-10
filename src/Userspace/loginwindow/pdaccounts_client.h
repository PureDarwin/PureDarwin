// pdaccounts_client.h - asking pdaccountsd for the account changes only root may make
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PDACCOUNTS_CLIENT_H
#define PDACCOUNTS_CLIENT_H

#include <stddef.h>

#define PDAC_SOCKET "/var/run/pdaccountsd.sock"
#define PDAC_MAX_ITEMS 256
#define PDAC_MAX_BYTES (4 << 20)

// one key and its value, the value may be binary
typedef struct {
	const char *key;
	const void *value;
	size_t len;
} pdac_item;

// a message as read off the socket, every value NUL-terminated after its length
typedef struct {
	int count;
	char *keys[PDAC_MAX_ITEMS];
	char *values[PDAC_MAX_ITEMS];
	size_t lens[PDAC_MAX_ITEMS];
} pdac_message;

// each item is a 4-byte length and the bytes, keys and values in turn
int pdac_write(int fd, const pdac_item *items, int count);
int pdac_read(int fd, pdac_message *m);
void pdac_free(pdac_message *m);

// a list reply carries one "user" item per account, its fields split by this
#define PDAC_FIELD_SEP '\x1f'

// a string value, NULL when the key is missing
const char *pdac_get(const pdac_message *m, const char *key);
const char *pdac_get_bytes(const pdac_message *m, const char *key, size_t *len);

// sends a request and reads the whole reply, -1 when the daemon couldn't be reached
int pdac_request(const pdac_item *items, int count, pdac_message *reply);

// sends a request, 0 on success, otherwise -1 with the daemon's reason in message
int pdac_call(const pdac_item *items, int count, char *message, size_t len);

#endif
