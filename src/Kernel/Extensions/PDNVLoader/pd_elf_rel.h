// pd_elf_rel.h - links one x86-64 ELF relocatable object into memory, as a kernel module loader does
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef PD_ELF_REL_H
#define PD_ELF_REL_H

#include <stddef.h>
#include <stdint.h>

// what the caller provides: file access, memory and a log, so the same code runs in a kext and on a host
struct pdelf_env {
	void *ctx;
	int (*read)(void *ctx, uint64_t off, void *buf, size_t len);    // 0 on success
	void *(*alloc)(void *ctx, size_t len);
	void (*free)(void *ctx, void *p, size_t len);
	void (*log)(void *ctx, const char *msg, const char *name, uint64_t value);
};

// an import's target: a function goes through a stub in the image, data is used where it is.
// data with addr 0 gets a cell of size bytes inside the image instead, copied from init when that is set
// (the object reaches its data with 32-bit relocations, so storage it imports must sit near it)
struct pdelf_target {
	uint64_t addr;
	int is_data;
	uint64_t size;
	const void *init;
};

// 0 and *t filled when name is known, nonzero when it is not
typedef int (*pdelf_resolve_fn)(void *ctx, const char *name, struct pdelf_target *t);

struct pdelf_obj {
	const struct pdelf_env *env;
	uint16_t shnum;
	void *shdrs;                    // Elf64_Shdr[shnum]
	char *shstrtab;
	uint64_t shstrtab_size;
	void *syms;                     // Elf64_Sym[nsyms]
	uint64_t nsyms;
	char *strtab;
	uint64_t strtab_size;
	uint64_t *secoff;               // per section: its offset in the image, or ~0 when not loaded
	uint64_t min_align;
	uint64_t sections_size;         // the loaded sections
	uint64_t stubs_off;             // the jump stubs, executable
	uint64_t cells_off;             // the data cells, writable
	uint32_t nimports;              // undefined symbols that need a stub
	uint64_t image_size;            // sections, stubs and cells
	uint64_t image_va;              // set by pdelf_load
	uint32_t unresolved;            // imports the resolver did not know
	struct pdelf_target *targets;   // per import, in symbol order
	uint64_t *celloff;              // per import: its cell in the cell area, or ~0
};

#define PDELF_STUB_SIZE 16

// reads the headers and tables, resolves every import and plans the image, with every loaded section
// aligned to at least min_align (a page, so each can get its own protection; 1 keeps the object's own
// alignment). Unknown imports point at missing, which the caller supplies to catch them; 0 on success
int pdelf_open(struct pdelf_obj *o, const struct pdelf_env *env, uint64_t min_align, pdelf_resolve_fn resolve,
    void *rctx, uint64_t missing);

// fills image (image_size bytes, at virtual address va once mapped): sections, stubs, cells, relocations;
// 0 on success
int pdelf_load(struct pdelf_obj *o, uint8_t *image, uint64_t va);

// the address of a global symbol the object defines, 0 if it has none by that name
uint64_t pdelf_export(const struct pdelf_obj *o, const char *name);

// the address of the stub (cell 0) or data cell (cell 1) an import was given, 0 if it imports no such name
uint64_t pdelf_import_slot(const struct pdelf_obj *o, const char *name, int cell);

// every part of the image with the access it needs, for the caller to set page protections
typedef void (*pdelf_range_fn)(void *ctx, uint64_t off, uint64_t size, int writable, int executable);
void pdelf_ranges(const struct pdelf_obj *o, pdelf_range_fn fn, void *ctx);

void pdelf_close(struct pdelf_obj *o);

#endif
