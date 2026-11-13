// test_link.c - links an object with pd_elf_rel at a fixed low address, for comparing against ld.lld
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
//
// test_link OBJ OUTDIR [MIN_ALIGN]: writes OUTDIR/image.bin, OUTDIR/layout.ld (a linker script placing every loaded
// section where pd_elf_rel put it) and OUTDIR/defsyms (every import at the address pd_elf_rel gave it),
// so ld.lld can link the same object to the same addresses and the bytes can be compared
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../pd_elf_rel.c"

#define BASE       0x40000000ull    // low enough for 32S on a host, the kernel uses the top 2 GB
#define FAKE_FN    0x7f0000000000ull
static int sFd;
static unsigned sNfn;
static const unsigned char sTrue = 1;
static unsigned char sParms[64];

// the object's data imports, each given a cell of its own in the image
static const struct { const char *name; uint64_t size; const void *init; } sData[] = {
	{ "nvidia_caps_root", 8, NULL },
	{ "nv_is_rm_firmware_supported_os", 1, &sTrue },
	{ "nv_parms", sizeof(sParms), sParms },
};

static int
host_read(void *ctx, uint64_t off, void *buf, size_t len)
{
	return pread(sFd, buf, len, (off_t)off) == (ssize_t)len ? 0 : -1;
}

static void *host_alloc(void *ctx, size_t len) { return malloc(len); }
static void host_free(void *ctx, void *p, size_t len) { free(p); }

static void
host_log(void *ctx, const char *msg, const char *name, uint64_t value)
{
	fprintf(stderr, "pdelf: %s %s 0x%llx\n", msg, name, (unsigned long long)value);
}

// every function import at its own fake address, data in cells
static int
resolve(void *ctx, const char *name, struct pdelf_target *t)
{
	for (unsigned i = 0; i < sizeof(sData) / sizeof(sData[0]); i++) {
		if (strcmp(name, sData[i].name) == 0) {
			*t = (struct pdelf_target){ 0, 1, sData[i].size, sData[i].init };
			return 0;
		}
	}
	*t = (struct pdelf_target){ FAKE_FN + 16 * sNfn++, 0, 0, NULL };
	return 0;
}

int
main(int argc, char **argv)
{
	static const struct pdelf_env env = { NULL, host_read, host_alloc, host_free, host_log };
	struct pdelf_obj o;
	uint8_t *image;
	char path[4096];
	FILE *f;
	uint32_t stub = 0;

	if (argc != 3 && argc != 4) {
		fprintf(stderr, "usage: test_link OBJ OUTDIR [MIN_ALIGN]\n");
		return 2;
	}
	if ((sFd = open(argv[1], O_RDONLY)) < 0 || pdelf_open(&o, &env, argc > 3 ? strtoull(argv[3], NULL, 0) : 1, resolve, NULL, FAKE_FN + 0xfff0) != 0)
		return 1;

	printf("sections 0x%llx, %u imports, image 0x%llx bytes\n", (unsigned long long)o.sections_size,
	    o.nimports, (unsigned long long)o.image_size);
	image = mmap((void *)BASE, o.image_size, PROT_READ | PROT_WRITE,
	    MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	if (image != (void *)BASE) {
		perror("mmap at the fixed base");
		return 1;
	}
	for (unsigned i = 0; i < sizeof(sParms); i++)
		sParms[i] = (unsigned char)(0xa0 + i);
	if (pdelf_load(&o, image, BASE) != 0)
		return 1;

	// the initial bytes reached their cells
	if (*(unsigned char *)(uintptr_t)pdelf_import_slot(&o, "nv_is_rm_firmware_supported_os", 1) != 1 ||
	    memcmp((void *)(uintptr_t)pdelf_import_slot(&o, "nv_parms", 1), sParms, sizeof(sParms)) != 0) {
		fprintf(stderr, "a data cell lost its initial bytes\n");
		return 1;
	}
	printf("unresolved %u, rm_init_rm at 0x%llx\n", o.unresolved,
	    (unsigned long long)pdelf_export(&o, "rm_init_rm"));

	snprintf(path, sizeof(path), "%s/image.bin", argv[2]);
	f = fopen(path, "wb");
	fwrite(image, 1, o.sections_size, f);
	fclose(f);

	// where each loaded section went
	snprintf(path, sizeof(path), "%s/layout.ld", argv[2]);
	f = fopen(path, "w");
	fprintf(f, "SECTIONS {\n");
	for (uint32_t i = 0; i < o.shnum; i++) {
		if (o.secoff[i] != NOT_LOADED)
			fprintf(f, "  %s 0x%llx : { *(%s) }\n", o.shstrtab + sh(&o, i)->sh_name,
			    (unsigned long long)(BASE + o.secoff[i]), o.shstrtab + sh(&o, i)->sh_name);
	}
	fprintf(f, "}\n");
	fclose(f);

	// every import at the address its references were bound to, the same walk pdelf_load made
	snprintf(path, sizeof(path), "%s/defsyms", argv[2]);
	f = fopen(path, "w");
	for (uint64_t i = 0; i < o.nsyms; i++) {
		const Sym *s = (const Sym *)o.syms + i;
		const char *name = symname(&o, s);
		uint64_t cell = pdelf_import_slot(&o, name, 1);

		if (!is_import(s, i))
			continue;

		fprintf(f, "--defsym=%s=0x%llx\n", name, (unsigned long long)(cell ? cell :
		    BASE + o.stubs_off + (uint64_t)stub * PDELF_STUB_SIZE));
		stub++;
	}
	fclose(f);
	pdelf_close(&o);
	return 0;
}
