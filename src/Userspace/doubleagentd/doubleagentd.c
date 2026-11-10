// doubleagentd.c - the AppleDouble ._ files the kernel keeps xattrs in on filesystems without native ones
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// it parses and lays them out, the kernel does the value I/O at the offsets it returns
#include <errno.h>
#include <libkern/OSByteOrder.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/xattr.h>
#include <unistd.h>

#include <mach/doubleagent_types.h>
#include "doubleagent_migServer.h"

#define kDoubleAgentServiceName "com.apple.doubleagentd"

extern int fileport_makefd(mach_port_t port);

#define ADH_MAGIC          0x00051607
#define ADH_VERSION        0x00020000
#define ADH_MACOSX         "Mac OS X        "
#define AD_RESOURCE        2
#define AD_FINDERINFO      9

#define ATTR_HDR_MAGIC     0x41545452
#define ATTR_BUF_SIZE      4096
#define ATTR_MAX_HDR_SIZE  65536
#define ATTR_MAX_SIZE      INT32_MAX
#define ATTR_MAX_ATTRS     256
#define FINDERINFOSIZE     32

typedef struct apple_double_entry {
    uint32_t type;
    uint32_t offset;
    uint32_t length;
} __attribute__((aligned(2), packed)) apple_double_entry_t;

typedef struct apple_double_header {
    uint32_t magic;
    uint32_t version;
    uint32_t filler[4];
    uint16_t numEntries;
    apple_double_entry_t entries[2];
    uint8_t finfo[FINDERINFOSIZE];
    uint8_t pad[2];
} __attribute__((aligned(2), packed)) apple_double_header_t;

typedef struct attr_entry {
    uint32_t offset;
    uint32_t length;
    uint16_t flags;
    uint8_t namelen;
    uint8_t name[1];
} __attribute__((aligned(2), packed)) attr_entry_t;

typedef struct attr_header {
    apple_double_header_t appledouble;
    uint32_t magic;
    uint32_t debug_tag;
    uint32_t total_size;
    uint32_t data_start;
    uint32_t data_length;
    uint32_t reserved[3];
    uint16_t flags;
    uint16_t num_attrs;
} __attribute__((aligned(2), packed)) attr_header_t;

typedef struct rsrcfork_header {
    uint32_t fh_DataOffset;
    uint32_t fh_MapOffset;
    uint32_t fh_DataLength;
    uint32_t fh_MapLength;
    uint8_t systemData[112];
    uint8_t appData[128];
    uint32_t mh_DataOffset;
    uint32_t mh_MapOffset;
    uint32_t mh_DataLength;
    uint32_t mh_MapLength;
    uint32_t mh_Next;
    uint16_t mh_RefNum;
    uint8_t mh_Attr;
    uint8_t mh_InMemoryAttr;
    uint16_t mh_Types;
    uint16_t mh_Names;
    uint16_t typeCount;
} __attribute__((aligned(2), packed)) rsrcfork_header_t;

#define RF_FIRST_RESOURCE   256
#define RF_NULL_MAP_LENGTH  30
#define RF_EMPTY_TAG        "This resource fork intentionally left blank   "

#define ATTR_ALIGN 3L
#define ATTR_ENTRY_LENGTH(namelen) \
    ((sizeof(attr_entry_t) - 1 + (namelen) + ATTR_ALIGN) & (~ATTR_ALIGN))

// one xattr of the ATTR area, in host order
typedef struct da_attr {
    uint32_t offset;
    uint32_t length;
    uint16_t flags;
    uint8_t namelen;
    char name[256];
} da_attr_t;

// the parsed ._ file, in host order
typedef struct da_file {
    int fd;
    off_t size;
    uint8_t *raw;
    size_t rawsize;
    uint16_t num_entries;
    apple_double_entry_t entries[15];
    apple_double_entry_t *finderinfo;
    apple_double_entry_t *rsrcfork;
    bool empty_finderinfo;
    bool has_attrhdr;
    uint32_t debug_tag;
    uint32_t total_size;
    uint32_t data_start;
    uint32_t data_length;
    uint16_t attr_flags;
    uint16_t num_attrs;
    da_attr_t *attrs;
} da_file_t;

static const uint8_t empty_finfo[FINDERINFOSIZE];

static int pread_full(int fd, void *buf, size_t len, off_t off)
{
    uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = pread(fd, p, len, off);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return errno;
        if (n == 0) return EIO;
        p += n;
        len -= (size_t)n;
        off += n;
    }

    return 0;
}

static int pwrite_full(int fd, const void *buf, size_t len, off_t off)
{
    const uint8_t *p = buf;
    while (len > 0) {
        ssize_t n = pwrite(fd, p, len, off);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return errno;
        p += n;
        len -= (size_t)n;
        off += n;
    }

    return 0;
}

// moves len bytes at from to to, either direction, overlapping or not
static int move_data(int fd, off_t from, off_t to, size_t len)
{
    if (len == 0 || from == to) return 0;

    uint8_t buf[ATTR_BUF_SIZE];
    int error = 0;
    if (to > from) {
        size_t left = len;
        while (left > 0 && error == 0) {
            size_t chunk = left < sizeof(buf) ? left : sizeof(buf);
            left -= chunk;
            error = pread_full(fd, buf, chunk, from + (off_t)left);
            if (error == 0) error = pwrite_full(fd, buf, chunk, to + (off_t)left);
        }
    } else {
        size_t done = 0;
        while (done < len && error == 0) {
            size_t chunk = len - done < sizeof(buf) ? len - done : sizeof(buf);
            error = pread_full(fd, buf, chunk, from + (off_t)done);
            if (error == 0) error = pwrite_full(fd, buf, chunk, to + (off_t)done);
            done += chunk;
        }
    }

    return error;
}

static int zero_data(int fd, off_t off, size_t len)
{
    static const uint8_t zeros[ATTR_BUF_SIZE];
    int error = 0;
    while (len > 0 && error == 0) {
        size_t chunk = len < sizeof(zeros) ? len : sizeof(zeros);
        error = pwrite_full(fd, zeros, chunk, off);
        off += (off_t)chunk;
        len -= chunk;
    }

    return error;
}

static void init_empty_resource_fork(rsrcfork_header_t *rf)
{
    memset(rf, 0, sizeof(*rf));
    rf->fh_DataOffset = OSSwapHostToBigInt32(RF_FIRST_RESOURCE);
    rf->fh_MapOffset = OSSwapHostToBigInt32(RF_FIRST_RESOURCE);
    rf->fh_MapLength = OSSwapHostToBigInt32(RF_NULL_MAP_LENGTH);
    rf->mh_DataOffset = OSSwapHostToBigInt32(RF_FIRST_RESOURCE);
    rf->mh_MapOffset = OSSwapHostToBigInt32(RF_FIRST_RESOURCE);
    rf->mh_MapLength = OSSwapHostToBigInt32(RF_NULL_MAP_LENGTH);
    rf->mh_Types = OSSwapHostToBigInt16(RF_NULL_MAP_LENGTH - 2);
    rf->mh_Names = OSSwapHostToBigInt16(RF_NULL_MAP_LENGTH);
    rf->typeCount = OSSwapHostToBigInt16((uint16_t)-1);
    memcpy(rf->systemData, RF_EMPTY_TAG, sizeof(RF_EMPTY_TAG));
}

// a fresh ._ file: Finder Info holding an empty ATTR area, then an empty resource fork
static int create_xattrfile(int fd)
{
    uint8_t buf[ATTR_BUF_SIZE] = {};
    attr_header_t *xah = (attr_header_t *)buf;
    size_t rfsize = sizeof(rsrcfork_header_t);

    xah->appledouble.magic = OSSwapHostToBigInt32(ADH_MAGIC);
    xah->appledouble.version = OSSwapHostToBigInt32(ADH_VERSION);
    xah->appledouble.numEntries = OSSwapHostToBigInt16(2);
    xah->appledouble.entries[0].type = OSSwapHostToBigInt32(AD_FINDERINFO);
    xah->appledouble.entries[0].offset = OSSwapHostToBigInt32(offsetof(apple_double_header_t, finfo));
    xah->appledouble.entries[0].length = OSSwapHostToBigInt32(ATTR_BUF_SIZE - offsetof(apple_double_header_t, finfo) - rfsize);
    xah->appledouble.entries[1].type = OSSwapHostToBigInt32(AD_RESOURCE);
    xah->appledouble.entries[1].offset = OSSwapHostToBigInt32(ATTR_BUF_SIZE - rfsize);
    xah->appledouble.entries[1].length = OSSwapHostToBigInt32(rfsize);
    memcpy(xah->appledouble.filler, ADH_MACOSX, sizeof(xah->appledouble.filler));

    xah->magic = OSSwapHostToBigInt32(ATTR_HDR_MAGIC);
    xah->total_size = OSSwapHostToBigInt32(ATTR_BUF_SIZE - rfsize);
    xah->data_start = OSSwapHostToBigInt32(sizeof(attr_header_t));

    init_empty_resource_fork((rsrcfork_header_t *)(buf + ATTR_BUF_SIZE - rfsize));

    if (ftruncate(fd, 0) != 0) return errno;
    return pwrite_full(fd, buf, sizeof(buf), 0);
}

static void release_file(da_file_t *f)
{
    free(f->raw);
    free(f->attrs);
    f->raw = NULL;
    f->attrs = NULL;
}

// the AppleDouble header and its entries, as check_and_swap_apple_double_header did
static int parse_adhdr(da_file_t *f)
{
    if (f->rawsize < offsetof(apple_double_header_t, entries)) return ENOATTR;

    apple_double_header_t *h = (apple_double_header_t *)f->raw;
    if (OSSwapBigToHostInt32(h->magic) != ADH_MAGIC || OSSwapBigToHostInt32(h->version) != ADH_VERSION) return ENOATTR;

    f->num_entries = OSSwapBigToHostInt16(h->numEntries);
    if (f->num_entries < 1 || f->num_entries > 15) return ENOATTR;

    uint32_t header_end = offsetof(apple_double_header_t, entries) + f->num_entries * sizeof(apple_double_entry_t);
    if (f->rawsize < header_end) return ENOATTR;

    for (int i = 0; i < f->num_entries; i++) {
        apple_double_entry_t *e = &f->entries[i];
        e->type = OSSwapBigToHostInt32(h->entries[i].type);
        e->offset = OSSwapBigToHostInt32(h->entries[i].offset);
        e->length = OSSwapBigToHostInt32(h->entries[i].length);

        uint32_t end = e->offset + e->length;
        if (e->offset < header_end || end < e->offset || end > (uint64_t)f->size) return ENOATTR;

        for (int j = 0; j < i; j++) {
            if (end > f->entries[j].offset && f->entries[j].offset + f->entries[j].length > e->offset) return ENOATTR;
        }
    }

    return 0;
}

// the ATTR header and its entries, as check_and_swap_attrhdr did
static int parse_attrhdr(da_file_t *f)
{
    attr_header_t *ah = (attr_header_t *)f->raw;
    if (OSSwapBigToHostInt32(ah->magic) != ATTR_HDR_MAGIC) return EINVAL;

    f->debug_tag = OSSwapBigToHostInt32(ah->debug_tag);
    f->total_size = OSSwapBigToHostInt32(ah->total_size);
    f->data_start = OSSwapBigToHostInt32(ah->data_start);
    f->data_length = OSSwapBigToHostInt32(ah->data_length);
    f->attr_flags = OSSwapBigToHostInt16(ah->flags);
    f->num_attrs = OSSwapBigToHostInt16(ah->num_attrs);

    uint32_t end = f->data_start + f->data_length;
    if (f->total_size > f->finderinfo->offset + f->finderinfo->length ||
        f->data_start < sizeof(attr_header_t) || end < f->data_start || end > f->total_size)
        return EINVAL;
    if (f->data_start > f->rawsize || f->num_attrs > ATTR_MAX_ATTRS) return EINVAL;

    f->attrs = calloc(ATTR_MAX_ATTRS + 1, sizeof(da_attr_t));
    if (f->attrs == NULL) return ENOMEM;

    uint8_t *buf_end = f->raw + f->data_start;
    uint8_t *p = f->raw + sizeof(attr_header_t);
    uint32_t total_header_size = sizeof(attr_header_t);
    for (int i = 0; i < f->num_attrs; i++) {
        attr_entry_t *ae = (attr_entry_t *)p;
        if (p + sizeof(attr_entry_t) > buf_end) return EINVAL;
        if (&ae->name[ae->namelen] > buf_end || ae->namelen == 0) return EINVAL;
        if (strnlen((const char *)ae->name, ae->namelen) != (size_t)ae->namelen - 1) return EINVAL;

        da_attr_t *a = &f->attrs[i];
        a->offset = OSSwapBigToHostInt32(ae->offset);
        a->length = OSSwapBigToHostInt32(ae->length);
        a->flags = OSSwapBigToHostInt16(ae->flags);
        a->namelen = ae->namelen;
        memcpy(a->name, ae->name, ae->namelen);

        uint32_t aend = a->offset + a->length;
        if (aend < a->offset || aend > f->total_size || a->offset < f->data_start) return EINVAL;

        total_header_size += ATTR_ENTRY_LENGTH(ae->namelen);
        p += ATTR_ENTRY_LENGTH(ae->namelen);
    }
    if (f->data_start < total_header_size) return EINVAL;

    return 0;
}

// writes the AppleDouble header, the ATTR header and the entries, up to data_start
static int write_header(da_file_t *f)
{
    size_t len = f->has_attrhdr ? f->data_start : offsetof(apple_double_header_t, finfo);
    if (len > f->rawsize) return EINVAL;

    uint8_t *buf = calloc(1, len);
    if (buf == NULL) return ENOMEM;
    memcpy(buf, f->raw, len);

    apple_double_header_t *h = (apple_double_header_t *)buf;
    h->numEntries = OSSwapHostToBigInt16(f->num_entries);
    for (int i = 0; i < f->num_entries; i++) {
        h->entries[i].type = OSSwapHostToBigInt32(f->entries[i].type);
        h->entries[i].offset = OSSwapHostToBigInt32(f->entries[i].offset);
        h->entries[i].length = OSSwapHostToBigInt32(f->entries[i].length);
    }

    if (f->has_attrhdr) {
        attr_header_t *ah = (attr_header_t *)buf;
        ah->magic = OSSwapHostToBigInt32(ATTR_HDR_MAGIC);
        ah->debug_tag = OSSwapHostToBigInt32(f->debug_tag);
        ah->total_size = OSSwapHostToBigInt32(f->total_size);
        ah->data_start = OSSwapHostToBigInt32(f->data_start);
        ah->data_length = OSSwapHostToBigInt32(f->data_length);
        ah->flags = OSSwapHostToBigInt16(f->attr_flags);
        ah->num_attrs = OSSwapHostToBigInt16(f->num_attrs);

        memset(buf + sizeof(attr_header_t), 0, len - sizeof(attr_header_t));
        uint8_t *p = buf + sizeof(attr_header_t);
        for (int i = 0; i < f->num_attrs; i++) {
            da_attr_t *a = &f->attrs[i];
            attr_entry_t *ae = (attr_entry_t *)p;
            ae->offset = OSSwapHostToBigInt32(a->offset);
            ae->length = OSSwapHostToBigInt32(a->length);
            ae->flags = OSSwapHostToBigInt16(a->flags);
            ae->namelen = a->namelen;
            memcpy(ae->name, a->name, a->namelen);
            p += ATTR_ENTRY_LENGTH(a->namelen);
        }
    }

    int error = pwrite_full(f->fd, buf, len, 0);
    free(buf);

    return error;
}

// a plain AppleDouble file with a 32-byte Finder Info gets room for an ATTR area, as get_xattrinfo did
static int make_attr_area(da_file_t *f)
{
    apple_double_entry_t *fi = &f->entries[0];
    apple_double_entry_t *rf = &f->entries[1];
    uint32_t delta = ATTR_BUF_SIZE - (fi->offset + FINDERINFOSIZE);
    int error;

    if (f->rsrcfork == rf && rf->length != 0) {
        if (ftruncate(f->fd, f->size + delta) != 0) return errno;
        if ((error = move_data(f->fd, rf->offset, rf->offset + delta, rf->length))) return error;
        f->size += delta;
    } else {
        rsrcfork_header_t rfh;
        if (ftruncate(f->fd, rf->offset + delta) != 0) return errno;
        f->size = rf->offset + delta;
        delta -= sizeof(rsrcfork_header_t);
        init_empty_resource_fork(&rfh);
        if ((error = pwrite_full(f->fd, &rfh, sizeof(rfh), rf->offset + delta))) return error;
        rf->length = sizeof(rsrcfork_header_t);
        f->rsrcfork = rf;
    }
    if ((error = zero_data(f->fd, fi->offset + FINDERINFOSIZE, delta))) return error;
    fi->length += delta;
    rf->offset += delta;

    f->has_attrhdr = true;
    f->total_size = rf->offset;
    f->data_start = sizeof(attr_header_t);
    f->data_length = 0;
    f->attr_flags = 0;
    f->num_attrs = 0;
    f->attrs = calloc(ATTR_MAX_ATTRS + 1, sizeof(da_attr_t));
    if (f->attrs == NULL) return ENOMEM;

    return write_header(f);
}

// reads and checks the ._ file, setting prepares it for a write and creates it when empty
static int load_file(da_file_t *f, int fd, int64_t size, bool setting)
{
    memset(f, 0, sizeof(*f));
    f->fd = fd;
    f->size = size;

    int error;
    if (size == 0 && setting) {
        if ((error = create_xattrfile(fd))) return error;
        f->size = ATTR_BUF_SIZE;
    }
    if (f->size < (off_t)sizeof(apple_double_header_t)) return ENOATTR;

    f->rawsize = f->size < ATTR_MAX_HDR_SIZE ? (size_t)f->size : ATTR_MAX_HDR_SIZE;
    f->raw = calloc(1, ATTR_MAX_HDR_SIZE);
    if (f->raw == NULL) return ENOMEM;
    if ((error = pread_full(fd, f->raw, f->rawsize, 0))) return error;
    if ((error = parse_adhdr(f))) return error;

    for (int i = 0; i < f->num_entries; i++) {
        apple_double_entry_t *e = &f->entries[i];
        if (e->type == AD_FINDERINFO && e->length >= FINDERINFOSIZE) {
            if (e->offset + FINDERINFOSIZE > f->rawsize) return ENOATTR;
            f->finderinfo = e;
            f->empty_finderinfo = memcmp(f->raw + e->offset, empty_finfo, FINDERINFOSIZE) == 0;
        }
        if (e->type == AD_RESOURCE) {
            // empty forks, and the blank ones we made, only count when setting
            if (e->length == 0 && !setting) continue;
            if (e->length == sizeof(rsrcfork_header_t) && !setting) {
                uint8_t sys[sizeof(RF_EMPTY_TAG)];
                if (pread_full(fd, sys, sizeof(sys), e->offset + 16) != 0 ||
                    memcmp(sys, RF_EMPTY_TAG, sizeof(RF_EMPTY_TAG)) == 0)
                    continue;
            }
            f->rsrcfork = e;
        }
    }

    if (setting && f->num_entries == 2 && f->finderinfo == &f->entries[0] &&
        f->rsrcfork == &f->entries[1] && f->finderinfo->offset == offsetof(apple_double_header_t, finfo) &&
        f->finderinfo->length == FINDERINFOSIZE) {
        f->rawsize = ATTR_MAX_HDR_SIZE;
        return make_attr_area(f);
    }

    if (f->finderinfo == &f->entries[0] && f->finderinfo->offset == offsetof(apple_double_header_t, finfo) &&
        f->finderinfo->length >= sizeof(attr_header_t) - sizeof(apple_double_header_t)) {
        if (parse_attrhdr(f) == 0) {
            f->has_attrhdr = true;
        } else {
            free(f->attrs);
            f->attrs = NULL;
            f->num_attrs = 0;
        }
    }

    // room for the entries to grow into
    if (setting) f->rawsize = ATTR_MAX_HDR_SIZE;

    return 0;
}

static int find_attr(da_file_t *f, const char *name)
{
    if (!f->has_attrhdr) return -1;
    for (int i = 0; i < f->num_attrs; i++) {
        if (strcmp(f->attrs[i].name, name) == 0) return i;
    }

    return -1;
}

static int attr_count(da_file_t *f)
{
    int n = f->has_attrhdr ? f->num_attrs : 0;
    if (f->rsrcfork) n++;
    if (f->finderinfo && !f->empty_finderinfo) n++;

    return n;
}

static int remove_attr(da_file_t *f, int idx)
{
    da_attr_t *a = &f->attrs[idx];
    uint32_t entrylen = ATTR_ENTRY_LENGTH(a->namelen);
    uint32_t dataoff = a->offset;
    uint32_t datalen = a->length;
    uint32_t data_end = f->data_start + f->data_length;
    int error;

    if ((error = move_data(f->fd, f->data_start, f->data_start - entrylen, dataoff - f->data_start))) return error;
    if ((error = move_data(f->fd, dataoff + datalen, dataoff - entrylen, data_end - (dataoff + datalen)))) return error;
    if ((error = zero_data(f->fd, data_end - (datalen + entrylen), datalen + entrylen))) return error;

    memmove(&f->attrs[idx], &f->attrs[idx + 1], (f->num_attrs - idx - 1) * sizeof(da_attr_t));
    f->num_attrs--;
    f->data_start -= entrylen;
    f->data_length -= datalen;
    for (int i = 0; i < f->num_attrs; i++) {
        if (f->attrs[i].offset > dataoff) f->attrs[i].offset -= datalen;
        f->attrs[i].offset -= entrylen;
    }

    return write_header(f);
}

static int add_attr(da_file_t *f, const char *name, uint64_t datalen, uint64_t *offset)
{
    size_t namelen = strlen(name) + 1;
    uint32_t entrylen = ATTR_ENTRY_LENGTH(namelen);
    int error;

    if (namelen > UINT8_MAX) return EINVAL;
    if (f->num_attrs >= ATTR_MAX_ATTRS || f->data_start + entrylen > ATTR_MAX_HDR_SIZE) return ENOSPC;

    uint64_t freespace = f->total_size - (f->data_start + f->data_length);
    if (datalen + entrylen > freespace) {
        uint64_t grow = (datalen + entrylen - freespace + ATTR_BUF_SIZE - 1) & ~(uint64_t)(ATTR_BUF_SIZE - 1);
        if (f->total_size + grow > UINT32_MAX || (uint64_t)f->size + grow > UINT32_MAX) return E2BIG;

        if (ftruncate(f->fd, f->size + (off_t)grow) != 0) return errno;
        f->size += grow;

        // the resource fork moves out of the way
        if (f->rsrcfork) {
            if ((error = move_data(f->fd, f->rsrcfork->offset, f->rsrcfork->offset + grow, f->rsrcfork->length))) return error;
            f->rsrcfork->offset += grow;
        }
        f->finderinfo->length += grow;
        f->total_size += grow;
    }

    if ((error = move_data(f->fd, f->data_start, f->data_start + entrylen, f->data_length))) return error;
    f->data_start += entrylen;
    for (int i = 0; i < f->num_attrs; i++) f->attrs[i].offset += entrylen;

    da_attr_t *a = &f->attrs[f->num_attrs++];
    memset(a, 0, sizeof(*a));
    a->offset = f->data_start + f->data_length;
    a->length = (uint32_t)datalen;
    a->namelen = (uint8_t)namelen;
    memcpy(a->name, name, namelen);
    f->data_length += datalen;

    if ((error = write_header(f))) return error;
    *offset = a->offset;

    return 0;
}

static int open_port(mach_port_t file_port, int *fd)
{
    *fd = fileport_makefd(file_port);
    mach_port_deallocate(mach_task_self(), file_port);

    return *fd < 0 ? EIO : 0;
}

static bool is_name(const char *name, const char *special)
{
    return strncmp(name, special, strlen(special) + 1) == 0;
}

kern_return_t doubleagent_lookup_xattr(mach_port_t server, mach_port_t file_port, int64_t file_size,
    xattrname name, int *err, uint64_t *value_offset, uint64_t *value_length)
{
    da_file_t f;
    int fd;

    *value_offset = 0;
    *value_length = 0;
    if ((*err = open_port(file_port, &fd))) return KERN_SUCCESS;
    name[DA_XATTR_MAXNAMELEN] = '\0';

    int error = load_file(&f, fd, file_size, false);
    if (error == 0) {
        if (is_name(name, DA_XATTR_FINDERINFO_NAME)) {
            if (f.finderinfo == NULL || f.empty_finderinfo) {
                error = ENOATTR;
            } else {
                *value_offset = f.finderinfo->offset;
                *value_length = FINDERINFOSIZE;
            }
        } else if (is_name(name, DA_XATTR_RESOURCEFORK_NAME)) {
            if (f.rsrcfork == NULL) {
                error = ENOATTR;
            } else {
                *value_offset = f.rsrcfork->offset;
                *value_length = f.rsrcfork->length;
            }
        } else {
            int i = find_attr(&f, name);
            if (i < 0) {
                error = ENOATTR;
            } else {
                *value_offset = f.attrs[i].offset;
                *value_length = f.attrs[i].length;
            }
        }
    }

    release_file(&f);
    close(fd);
    *err = error;

    return KERN_SUCCESS;
}

static int allocate(da_file_t *f, const char *name, uint64_t size, uint32_t options, uint64_t *offset)
{
    if (is_name(name, DA_XATTR_FINDERINFO_NAME)) {
        bool exists = f->finderinfo && !f->empty_finderinfo;
        if (exists && (options & XATTR_CREATE)) return EEXIST;
        if (!exists && (options & XATTR_REPLACE)) return ENOATTR;
        if (f->finderinfo == NULL) return ENOATTR;
        *offset = f->finderinfo->offset;
        return 0;
    }

    if (is_name(name, DA_XATTR_RESOURCEFORK_NAME)) {
        if (f->rsrcfork == NULL) return ENOATTR;
        if (f->rsrcfork->length != 0 && (options & XATTR_CREATE)) return EEXIST;
        if (f->rsrcfork->length == 0 && (options & XATTR_REPLACE)) return ENOATTR;
        if (size > UINT32_MAX) return EINVAL;

        // the fork is the last entry, so it grows at the end of the file
        if (size > f->rsrcfork->length) {
            if (f->rsrcfork != &f->entries[f->num_entries - 1]) return ENOSPC;
            f->rsrcfork->length = (uint32_t)size;
            if (ftruncate(f->fd, (off_t)f->rsrcfork->offset + (off_t)size) != 0) return errno;
            int error = write_header(f);
            if (error) return error;
        }
        *offset = f->rsrcfork->offset;
        return 0;
    }

    if (size > ATTR_MAX_SIZE) return E2BIG;
    if (!f->has_attrhdr) return ENOATTR;

    int i = find_attr(f, name);
    if (i >= 0) {
        if (options & XATTR_CREATE) return EEXIST;
        if (f->attrs[i].length == size) {
            *offset = f->attrs[i].offset;
            return 0;
        }
        int error = remove_attr(f, i);
        if (error) return error;
    } else if (options & XATTR_REPLACE) {
        return ENOATTR;
    }

    return add_attr(f, name, size, offset);
}

kern_return_t doubleagent_allocate_xattr(mach_port_t server, mach_port_t file_port, int64_t file_size,
    xattrname name, uint64_t size, uint32_t options, int *err, uint64_t *value_offset)
{
    da_file_t f;
    int fd;

    *value_offset = 0;
    if ((*err = open_port(file_port, &fd))) return KERN_SUCCESS;
    name[DA_XATTR_MAXNAMELEN] = '\0';

    int error = load_file(&f, fd, file_size, true);
    if (error == 0) error = allocate(&f, name, size, options, value_offset);

    release_file(&f);
    close(fd);
    *err = error;

    return KERN_SUCCESS;
}

// the load skips blank resource forks, so they never keep the ._ file alive
static int remove_named(da_file_t *f, const char *name, boolean_t *is_empty)
{
    int count = attr_count(f);

    if (is_name(name, DA_XATTR_FINDERINFO_NAME)) {
        if (f->finderinfo == NULL || f->empty_finderinfo) return ENOATTR;
        if (count == 1) {
            *is_empty = true;
            return 0;
        }
        return pwrite_full(f->fd, empty_finfo, FINDERINFOSIZE, f->finderinfo->offset);
    }

    if (is_name(name, DA_XATTR_RESOURCEFORK_NAME)) {
        if (f->rsrcfork == NULL || f->rsrcfork->length == 0) return ENOATTR;
        if (count == 1) {
            *is_empty = true;
            return 0;
        }
        if (f->rsrcfork->offset + f->rsrcfork->length == (uint64_t)f->size) {
            if (ftruncate(f->fd, f->rsrcfork->offset) != 0) return errno;
        }
        f->rsrcfork->length = 0;
        return write_header(f);
    }

    int i = find_attr(f, name);
    if (i < 0) return ENOATTR;
    if (count == 1) {
        *is_empty = true;
        return 0;
    }

    return remove_attr(f, i);
}

kern_return_t doubleagent_remove_xattr(mach_port_t server, mach_port_t file_port, int64_t file_size,
    xattrname name, int *err, boolean_t *is_empty)
{
    da_file_t f;
    int fd;

    *is_empty = false;
    if ((*err = open_port(file_port, &fd))) return KERN_SUCCESS;
    name[DA_XATTR_MAXNAMELEN] = '\0';

    int error = load_file(&f, fd, file_size, false);
    if (error == 0) error = remove_named(&f, name, is_empty);

    release_file(&f);
    close(fd);
    *err = error;

    return KERN_SUCCESS;
}

static bool listable(const da_attr_t *a)
{
    if (strncmp(a->name, "com.apple.system.", strlen("com.apple.system.")) == 0) return false;
    if (a->name[0] == '\0') return false;

    return true;
}

kern_return_t doubleagent_list_xattrs(mach_port_t server, mach_port_t file_port, int64_t file_size,
    int *err, listxattrs_result_t *result)
{
    da_file_t f;
    int fd;

    memset(result, 0, offsetof(listxattrs_result_t, data));
    if ((*err = open_port(file_port, &fd))) return KERN_SUCCESS;

    int error = load_file(&f, fd, file_size, false);
    if (error == ENOATTR) error = 0;
    if (error == 0 && f.raw != NULL && f.num_entries > 0) {
        uint8_t *names = result->data;
        size_t nlen = 0;

        if (f.finderinfo && !f.empty_finderinfo) {
            result->finderInfoOffset = f.finderinfo->offset;
            memcpy(names + nlen, DA_XATTR_FINDERINFO_NAME, sizeof(DA_XATTR_FINDERINFO_NAME));
            nlen += sizeof(DA_XATTR_FINDERINFO_NAME);
        }
        if (f.rsrcfork) {
            result->resourceForkOffset = f.rsrcfork->offset;
            result->resourceForkLength = f.rsrcfork->length;
            memcpy(names + nlen, DA_XATTR_RESOURCEFORK_NAME, sizeof(DA_XATTR_RESOURCEFORK_NAME));
            nlen += sizeof(DA_XATTR_RESOURCEFORK_NAME);
        }

        uint32_t ranges[MAX_NUM_OF_XATTRS * 2];
        uint64_t count = 0;
        for (int i = 0; f.has_attrhdr && i < f.num_attrs && count < MAX_NUM_OF_XATTRS; i++) {
            da_attr_t *a = &f.attrs[i];
            if (!listable(a)) continue;
            memcpy(names + nlen, a->name, a->namelen);
            nlen += a->namelen;
            ranges[count * 2] = a->offset;
            ranges[count * 2 + 1] = a->length;
            count++;
        }

        memcpy(names + nlen, ranges, count * 2 * sizeof(uint32_t));
        result->numOfXattrs = count;
        result->namesLength = nlen;
        result->rangesLength = count * 2 * sizeof(uint32_t);
        result->dataLength = result->namesLength + result->rangesLength;
    }

    release_file(&f);
    close(fd);
    *err = error;

    return KERN_SUCCESS;
}

int main(void)
{
    mach_port_t port = MACH_PORT_NULL;

    // launchd holds the receive right and hands it to the kernel as HOST_DOUBLEAGENTD_PORT
    kern_return_t kr = bootstrap_check_in(bootstrap_port, kDoubleAgentServiceName, &port);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "doubleagentd: bootstrap_check_in %s failed: %s\n", kDoubleAgentServiceName, mach_error_string(kr));
        return 1;
    }

    static union {
        mach_msg_header_t head;
        union __RequestUnion__doubleagent_subsystem req;
        uint8_t room[sizeof(union __RequestUnion__doubleagent_subsystem) + MAX_TRAILER_SIZE];
    } in;
    static union {
        mach_msg_header_t head;
        union __ReplyUnion__doubleagent_subsystem rep;
    } out;

    // libSystem exports no mach_msg_server, so a plain receive, dispatch and reply loop
    for (;;) {
        kr = mach_msg(&in.head, MACH_RCV_MSG, 0, sizeof(in), port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (kr != MACH_MSG_SUCCESS) {
            fprintf(stderr, "doubleagentd: receive failed: %s\n", mach_error_string(kr));
            continue;
        }

        doubleagent_server(&in.head, &out.head);
        mig_reply_error_t *reply = (mig_reply_error_t *)&out.head;
        bool complex = out.head.msgh_bits & MACH_MSGH_BITS_COMPLEX;
        if (!complex && reply->RetCode != KERN_SUCCESS && reply->RetCode != MIG_NO_REPLY) {
            in.head.msgh_remote_port = MACH_PORT_NULL;
            mach_msg_destroy(&in.head);
        }
        if (out.head.msgh_remote_port == MACH_PORT_NULL || (!complex && reply->RetCode == MIG_NO_REPLY)) continue;

        kr = mach_msg(&out.head, MACH_SEND_MSG, out.head.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
        if (kr != MACH_MSG_SUCCESS) {
            fprintf(stderr, "doubleagentd: reply failed: %s\n", mach_error_string(kr));
            mach_msg_destroy(&out.head);
        }
    }

    return 1;
}
