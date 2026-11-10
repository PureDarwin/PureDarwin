/*
 * Copyright (c) 2023 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// PDQcomSMEM.cpp - qualcomm shared memory item lookup, from openbsd sys/dev/fdt/qcsmem.c
// SPDX-License-Identifier: ISC

#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <sys/param.h>

#include "PDQcomSMEM.h"

#define QCSMEM_ITEM_COUNT	512
#define QCSMEM_HOST_COUNT	15

struct qcsmem_proc_comm {
	uint32_t command;
	uint32_t status;
	uint32_t params[2];
};

struct qcsmem_global_entry {
	uint32_t allocated;
	uint32_t offset;
	uint32_t size;
	uint32_t aux_base;
#define QCSMEM_GLOBAL_ENTRY_AUX_BASE_MASK	0xfffffffc
};

struct qcsmem_header {
	struct qcsmem_proc_comm proc_comm[4];
	uint32_t version[32];
#define QCSMEM_HEADER_VERSION_MASTER_SBL_IDX	7
#define QCSMEM_HEADER_VERSION_GLOBAL_HEAP	11
#define QCSMEM_HEADER_VERSION_GLOBAL_PART	12
	uint32_t initialized;
	uint32_t free_offset;
	uint32_t available;
	uint32_t reserved;
	struct qcsmem_global_entry toc[QCSMEM_ITEM_COUNT];
};

struct qcsmem_ptable_entry {
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
	uint16_t host[2];
#define QCSMEM_LOCAL_HOST			0
#define QCSMEM_GLOBAL_HOST			0xfffe
	uint32_t cacheline;
	uint32_t reserved[7];
};

struct qcsmem_ptable {
	uint32_t magic;
#define QCSMEM_PTABLE_MAGIC	0x434f5424
	uint32_t version;
#define QCSMEM_PTABLE_VERSION	1
	uint32_t num_entries;
	uint32_t reserved[5];
	struct qcsmem_ptable_entry entry[];
};

struct qcsmem_partition_header {
	uint32_t magic;
#define QCSMEM_PART_HDR_MAGIC	0x54525024
	uint16_t host[2];
	uint32_t size;
	uint32_t offset_free_uncached;
	uint32_t offset_free_cached;
	uint32_t reserved[3];
};

struct qcsmem_private_entry {
	uint16_t canary;
#define QCSMEM_PRIV_ENTRY_CANARY	0xa5a5
	uint16_t item;
	uint32_t size;
	uint16_t padding_data;
	uint16_t padding_hdr;
	uint32_t reserved;
};

struct qcsmem_partition {
	uint8_t *phdr;
	size_t cacheline;
	size_t size;
};

static IOMemoryMap *smemMap, *auxMap;
static uint8_t *smemBase, *auxBase;
static uint64_t smemPa, smemSize, auxPa, auxSize;
static struct qcsmem_partition partitions[QCSMEM_HOST_COUNT];

// the version 11 heap of msm8916 keeps items in the header toc, later parts add partitions
bool
PDQcomSMEM::attach(uint64_t pa, uint64_t size)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(pa, size, kIODirectionInOut);

	smemMap = md != NULL ? md->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(md);
	if (smemMap == NULL)
		return false;
	smemBase = (uint8_t *)smemMap->getVirtualAddress();
	smemPa = pa;
	smemSize = size;

	struct qcsmem_header *header = (struct qcsmem_header *)smemBase;
	uint32_t version = header->version[QCSMEM_HEADER_VERSION_MASTER_SBL_IDX] >> 16;
	struct qcsmem_ptable *ptable = (struct qcsmem_ptable *)(smemBase + size - 4096);
	int nparts = 0;

	if (ptable->magic == QCSMEM_PTABLE_MAGIC && ptable->version == QCSMEM_PTABLE_VERSION) {
		for (uint32_t i = 0; i < ptable->num_entries; i++) {
			struct qcsmem_ptable_entry *pte = &ptable->entry[i];
			struct qcsmem_partition *part;

			if (!pte->offset || !pte->size || pte->offset + pte->size > size)
				continue;
			if (pte->host[0] == QCSMEM_LOCAL_HOST && pte->host[1] < QCSMEM_HOST_COUNT)
				part = &partitions[pte->host[1]];
			else if (pte->host[1] == QCSMEM_LOCAL_HOST && pte->host[0] < QCSMEM_HOST_COUNT)
				part = &partitions[pte->host[0]];
			else
				continue;

			struct qcsmem_partition_header *phdr = (struct qcsmem_partition_header *)(smemBase + pte->offset);
			if (part->phdr != NULL || phdr->magic != QCSMEM_PART_HDR_MAGIC || phdr->size != pte->size ||
			    phdr->offset_free_uncached > phdr->size)
				continue;
			part->phdr = (uint8_t *)phdr;
			part->size = pte->size;
			part->cacheline = pte->cacheline;
			nparts++;
		}
	}
	IOLog("PDQcomSMEM: version %u, %u bytes free, %d private partitions\n", version,
	    header->available, nparts);
	return version == QCSMEM_HEADER_VERSION_GLOBAL_HEAP || version == QCSMEM_HEADER_VERSION_GLOBAL_PART;
}

static void *
getPrivate(struct qcsmem_partition *part, int item, uint32_t *size)
{
	struct qcsmem_partition_header *phdr = (struct qcsmem_partition_header *)part->phdr;
	uint8_t *entry = (uint8_t *)&phdr[1];
	uint8_t *last = part->phdr + phdr->offset_free_uncached;

	while (entry < last) {
		struct qcsmem_private_entry *e = (struct qcsmem_private_entry *)entry;
		if (e->canary != QCSMEM_PRIV_ENTRY_CANARY)
			return NULL;
		if (e->item == item) {
			if (e->size > part->size || e->padding_data > e->size)
				return NULL;
			if (size != NULL)
				*size = e->size - e->padding_data;
			return entry + sizeof(*e) + e->padding_hdr;
		}
		entry += sizeof(*e) + e->padding_hdr + e->size;
	}

	size_t step = roundup(sizeof(struct qcsmem_private_entry), part->cacheline);
	entry = part->phdr + phdr->size - step;
	last = part->phdr + phdr->offset_free_cached;
	while (entry > last && entry >= part->phdr) {
		struct qcsmem_private_entry *e = (struct qcsmem_private_entry *)entry;
		if (e->canary != QCSMEM_PRIV_ENTRY_CANARY)
			return NULL;
		if (e->item == item) {
			if (e->size > part->size || e->padding_data > e->size)
				return NULL;
			if (size != NULL)
				*size = e->size - e->padding_data;
			return entry - e->size;
		}
		entry -= e->size + step;
	}
	return NULL;
}

static void *
getGlobal(int item, uint32_t *size)
{
	struct qcsmem_header *header = (struct qcsmem_header *)smemBase;
	struct qcsmem_global_entry *entry = &header->toc[item];

	if (!entry->allocated)
		return NULL;
	uint32_t aux = entry->aux_base & QCSMEM_GLOBAL_ENTRY_AUX_BASE_MASK;
	if (aux != 0 && aux == auxPa && auxBase != NULL && (uint64_t)entry->offset + entry->size <= auxSize) {
		if (size != NULL)
			*size = entry->size;
		return auxBase + entry->offset;
	}
	if (aux != 0 && aux != smemPa)
		return NULL;
	if ((uint64_t)entry->size + entry->offset > smemSize)
		return NULL;
	if (size != NULL)
		*size = entry->size;
	return smemBase + entry->offset;
}

// a second region global items may point into through aux_base, the rpm message ram on msm8916
bool
PDQcomSMEM::attachAux(uint64_t pa, uint64_t size)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(pa, size, kIODirectionInOut);

	auxMap = md != NULL ? md->createMappingInTask(kernel_task, 0, kIOMapAnywhere | kIOMapInhibitCache) : NULL;
	OSSafeReleaseNULL(md);
	if (auxMap == NULL)
		return false;
	auxBase = (uint8_t *)auxMap->getVirtualAddress();
	auxPa = pa;
	auxSize = size;
	return true;
}

// the remote's private partition first, then the global heap
void *
PDQcomSMEM::get(int host, int item, uint32_t *size)
{
	if (smemBase == NULL || item < 0 || item >= QCSMEM_ITEM_COUNT)
		return NULL;
	if (host >= 0 && host < QCSMEM_HOST_COUNT && partitions[host].phdr != NULL) {
		void *p = getPrivate(&partitions[host], item, size);
		if (p != NULL)
			return p;
	}
	return getGlobal(item, size);
}
