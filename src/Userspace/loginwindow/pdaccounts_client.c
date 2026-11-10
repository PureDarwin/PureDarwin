// pdaccounts_client.c - asking pdaccountsd for the account changes only root may make
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "pdaccounts_client.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int
write_all(int fd, const void *buf, size_t len)
{
	const char *p = buf;

	while (len > 0) {
		ssize_t n = write(fd, p, len);

		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

static int
read_all(int fd, void *buf, size_t len)
{
	char *p = buf;

	while (len > 0) {
		ssize_t n = read(fd, p, len);

		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		p += n;
		len -= (size_t)n;
	}
	return 0;
}

static int
write_chunk(int fd, const void *bytes, size_t len)
{
	uint32_t n = (uint32_t)len;

	return write_all(fd, &n, sizeof(n)) == 0 && write_all(fd, bytes, len) == 0 ? 0 : -1;
}

int
pdac_write(int fd, const pdac_item *items, int count)
{
	uint32_t n = (uint32_t)count;

	if (write_all(fd, &n, sizeof(n)) != 0)
		return -1;
	for (int i = 0; i < count; i++) {
		size_t len = items[i].value == NULL ? 0 : items[i].len ? items[i].len : strlen(items[i].value);

		if (write_chunk(fd, items[i].key, strlen(items[i].key)) != 0 || write_chunk(fd, items[i].value ? items[i].value : "", len) != 0)
			return -1;
	}
	return 0;
}

static char *
read_chunk(int fd, size_t *total, size_t *len)
{
	uint32_t n;
	char *buf;

	if (read_all(fd, &n, sizeof(n)) != 0 || n > PDAC_MAX_BYTES || *total + n > PDAC_MAX_BYTES)
		return NULL;
	*total += n;
	if ((buf = malloc((size_t)n + 1)) == NULL)
		return NULL;
	if (read_all(fd, buf, n) != 0) {
		free(buf);
		return NULL;
	}
	buf[n] = '\0';
	*len = n;
	return buf;
}

int
pdac_read(int fd, pdac_message *m)
{
	uint32_t count;
	size_t total = 0, klen;

	memset(m, 0, sizeof(*m));
	if (read_all(fd, &count, sizeof(count)) != 0 || count > PDAC_MAX_ITEMS)
		return -1;
	for (uint32_t i = 0; i < count; i++) {
		m->keys[i] = read_chunk(fd, &total, &klen);
		m->values[i] = m->keys[i] ? read_chunk(fd, &total, &m->lens[i]) : NULL;
		if (m->values[i] == NULL) {
			free(m->keys[i]);
			m->keys[i] = NULL;
			pdac_free(m);
			return -1;
		}
		m->count++;
	}
	return 0;
}

void
pdac_free(pdac_message *m)
{
	for (int i = 0; i < m->count; i++) {
		// passwords pass through here, so nothing lingers in freed memory
		memset(m->values[i], 0, m->lens[i]);
		free(m->keys[i]);
		free(m->values[i]);
	}
	m->count = 0;
}

const char *
pdac_get_bytes(const pdac_message *m, const char *key, size_t *len)
{
	for (int i = 0; i < m->count; i++) {
		if (strcmp(m->keys[i], key) == 0) {
			if (len)
				*len = m->lens[i];
			return m->values[i];
		}
	}
	return NULL;
}

const char *
pdac_get(const pdac_message *m, const char *key)
{
	size_t len;
	const char *v = pdac_get_bytes(m, key, &len);

	// a string never carries a NUL inside it
	return v != NULL && strlen(v) == len ? v : NULL;
}

int
pdac_request(const pdac_item *items, int count, pdac_message *reply)
{
	struct sockaddr_un sun = { .sun_family = AF_UNIX };
	int fd, ret = -1;

	strlcpy(sun.sun_path, PDAC_SOCKET, sizeof(sun.sun_path));
	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		return -1;
	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0 && pdac_write(fd, items, count) == 0 && pdac_read(fd, reply) == 0)
		ret = 0;
	close(fd);
	return ret;
}

int
pdac_call(const pdac_item *items, int count, char *message, size_t len)
{
	pdac_message reply;
	const char *status, *text;
	int ok;

	if (pdac_request(items, count, &reply) != 0) {
		snprintf(message, len, "The account service is not running.");
		return -1;
	}
	status = pdac_get(&reply, "status");
	text = pdac_get(&reply, "message");
	ok = status != NULL && strcmp(status, "ok") == 0;
	snprintf(message, len, "%s", text ? text : ok ? "" : "The account service refused.");
	pdac_free(&reply);
	return ok ? 0 : -1;
}
