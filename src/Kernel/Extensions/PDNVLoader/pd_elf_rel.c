// pd_elf_rel.c - links one x86-64 ELF relocatable object into memory, as a kernel module loader does
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include "pd_elf_rel.h"

// the ELF64 structures and constants this needs, from the ELF specification
typedef struct {
	uint8_t e_ident[16];
	uint16_t e_type, e_machine;
	uint32_t e_version;
	uint64_t e_entry, e_phoff, e_shoff;
	uint32_t e_flags;
	uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Ehdr;

typedef struct {
	uint32_t sh_name, sh_type;
	uint64_t sh_flags, sh_addr, sh_offset, sh_size;
	uint32_t sh_link, sh_info;
	uint64_t sh_addralign, sh_entsize;
} Shdr;

typedef struct {
	uint32_t st_name;
	uint8_t st_info, st_other;
	uint16_t st_shndx;
	uint64_t st_value, st_size;
} Sym;

typedef struct {
	uint64_t r_offset, r_info;
	int64_t r_addend;
} Rela;

#define ET_REL          1
#define EM_X86_64       62
#define SHT_SYMTAB      2
#define SHT_RELA        4
#define SHT_NOBITS      8
#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_COMPRESSED  0x800
#define SHN_UNDEF       0
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define STB_LOCAL       0
#define R_X86_64_NONE   0
#define R_X86_64_64     1
#define R_X86_64_PC32   2
#define R_X86_64_32S    11
#define NOT_LOADED      (~(uint64_t)0)
#define NO_STUB         0xffffffffu
#define RELA_CHUNK      1024

static int
streq(const char *a, const char *b)
{
	while (*a && *a == *b)
		a++, b++;
	return *a == *b;
}

static void
fill(uint8_t *p, uint8_t v, uint64_t n)
{
	while (n--)
		*p++ = v;
}

static uint64_t
align_up(uint64_t v, uint64_t a)
{
	return a > 1 ? (v + a - 1) & ~(a - 1) : v;
}

static void *
read_alloc(struct pdelf_obj *o, uint64_t off, uint64_t len)
{
	void *p = o->env->alloc(o->env->ctx, len ? len : 1);

	if (p == NULL)
		return NULL;

	if (len && o->env->read(o->env->ctx, off, p, len) != 0) {
		o->env->free(o->env->ctx, p, len);
		return NULL;
	}
	return p;
}

static void
fail(const struct pdelf_obj *o, const char *msg, const char *name, uint64_t value)
{
	o->env->log(o->env->ctx, msg, name, value);
}

static Shdr *
sh(const struct pdelf_obj *o, uint32_t i)
{
	return (Shdr *)o->shdrs + i;
}

static const char *
symname(const struct pdelf_obj *o, const Sym *s)
{
	return s->st_name < o->strtab_size ? o->strtab + s->st_name : "";
}

// retpoline thunks a compiler emits for indirect calls: here each is a plain jump through its register
static int
thunk_register(const char *name)
{
	static const char *const regs[16] = {
		"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
		"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	};
	static const char prefix[] = "__x86_indirect_thunk_";

	for (int i = 0; prefix[i]; i++) {
		if (name[i] != prefix[i])
			return -1;
	}
	for (int r = 0; r < 16; r++) {
		if (streq(name + sizeof(prefix) - 1, regs[r]))
			return r;
	}
	return -1;
}

static int
is_import(const Sym *s, uint64_t i)
{
	return i != 0 && s->st_shndx == SHN_UNDEF && (s->st_info >> 4) != STB_LOCAL;
}

int
pdelf_open(struct pdelf_obj *o, const struct pdelf_env *env, uint64_t min_align, pdelf_resolve_fn resolve,
    void *rctx, uint64_t missing)
{
	Ehdr eh;
	Shdr *symsh = NULL;
	uint64_t cursor = 0, cells = 0;
	uint32_t k = 0;

	*o = (struct pdelf_obj){ .env = env, .min_align = min_align > 16 ? min_align : 16 };
	if (env->read(env->ctx, 0, &eh, sizeof(eh)) != 0)
		return -1;

	if (eh.e_ident[0] != 0x7f || eh.e_ident[1] != 'E' || eh.e_ident[2] != 'L' || eh.e_ident[3] != 'F' ||
	    eh.e_ident[4] != 2 || eh.e_ident[5] != 1 || eh.e_type != ET_REL || eh.e_machine != EM_X86_64 ||
	    eh.e_shentsize != sizeof(Shdr) || eh.e_shstrndx >= eh.e_shnum) {
		fail(o, "not an x86-64 ELF relocatable object", "", eh.e_type);
		return -1;
	}

	o->shnum = eh.e_shnum;
	o->shdrs = read_alloc(o, eh.e_shoff, (uint64_t)eh.e_shnum * sizeof(Shdr));
	o->secoff = env->alloc(env->ctx, eh.e_shnum * sizeof(uint64_t));
	if (o->shdrs == NULL || o->secoff == NULL)
		return -1;

	o->shstrtab_size = sh(o, eh.e_shstrndx)->sh_size;
	o->shstrtab = read_alloc(o, sh(o, eh.e_shstrndx)->sh_offset, o->shstrtab_size);
	if (o->shstrtab == NULL)
		return -1;

	// the loaded sections in index order, each at its own alignment
	for (uint32_t i = 0; i < o->shnum; i++) {
		Shdr *s = sh(o, i);

		o->secoff[i] = NOT_LOADED;
		if (s->sh_type == SHT_SYMTAB)
			symsh = s;

		if (!(s->sh_flags & SHF_ALLOC) || s->sh_size == 0)
			continue;

		if (s->sh_flags & SHF_COMPRESSED) {
			fail(o, "compressed loaded section", o->shstrtab + s->sh_name, i);
			return -1;
		}
		cursor = align_up(cursor, s->sh_addralign > min_align ? s->sh_addralign : min_align);
		o->secoff[i] = cursor;
		cursor += s->sh_size;
	}
	o->sections_size = align_up(cursor, min_align > 16 ? min_align : 16);

	if (symsh == NULL || symsh->sh_entsize != sizeof(Sym) || symsh->sh_link >= o->shnum) {
		fail(o, "no symbol table", "", 0);
		return -1;
	}
	o->nsyms = symsh->sh_size / sizeof(Sym);
	o->syms = read_alloc(o, symsh->sh_offset, symsh->sh_size);
	o->strtab_size = sh(o, symsh->sh_link)->sh_size;
	o->strtab = read_alloc(o, sh(o, symsh->sh_link)->sh_offset, o->strtab_size);
	if (o->syms == NULL || o->strtab == NULL)
		return -1;

	for (uint64_t i = 0; i < o->nsyms; i++) {
		const Sym *s = (const Sym *)o->syms + i;

		if (s->st_shndx == SHN_COMMON) {
			fail(o, "common symbol, not supported", symname(o, s), i);
			return -1;
		}
		if (is_import(s, i))
			o->nimports++;
	}

	// every import resolved now, so data cells can be sized before the image exists
	o->targets = env->alloc(env->ctx, (o->nimports ? o->nimports : 1) * sizeof(struct pdelf_target));
	o->celloff = env->alloc(env->ctx, (o->nimports ? o->nimports : 1) * sizeof(uint64_t));
	if (o->targets == NULL || o->celloff == NULL)
		return -1;

	for (uint64_t i = 0; i < o->nsyms; i++) {
		const Sym *s = (const Sym *)o->syms + i;
		struct pdelf_target *t = &o->targets[k];
		const char *name = symname(o, s);

		if (!is_import(s, i))
			continue;

		*t = (struct pdelf_target){ 0, 0, 0, NULL };
		o->celloff[k] = NOT_LOADED;
		if (thunk_register(name) < 0 && resolve(rctx, name, t) != 0) {
			fail(o, "unresolved import", name, i);
			o->unresolved++;
			*t = (struct pdelf_target){ missing, 0, 0, NULL };
		}
		// storage the object expects someone else to define: a cell of its own size in the image
		if (t->is_data && t->addr == 0) {
			o->celloff[k] = cells;
			cells += align_up(t->size ? t->size : 8, 16);
		}
		k++;
	}

	// stubs execute, cells are written: each on pages of its own
	o->stubs_off = o->sections_size;
	o->cells_off = align_up(o->stubs_off + (uint64_t)o->nimports * PDELF_STUB_SIZE, o->min_align);
	o->image_size = align_up(o->cells_off + (cells ? cells : 16), o->min_align);
	return 0;
}

// a 16-byte stub: jmp *0(%rip) and the target's address after it, no register touched
static void
write_jump_stub(uint8_t *p, uint64_t target)
{
	p[0] = 0xff, p[1] = 0x25;
	p[2] = p[3] = p[4] = p[5] = 0;
	for (int i = 0; i < 8; i++)
		p[6 + i] = (uint8_t)(target >> (8 * i));
	p[14] = p[15] = 0xcc;
}

// jmp *%reg, the rest int3
static void
write_thunk_stub(uint8_t *p, int reg)
{
	int n = 0;

	fill(p, 0xcc, PDELF_STUB_SIZE);
	if (reg >= 8)
		p[n++] = 0x41;
	p[n++] = 0xff;
	p[n] = (uint8_t)(0xe0 | (reg & 7));
}

static int
fits_s32(int64_t v)
{
	return v >= -2147483648LL && v <= 2147483647LL;
}

static void
put32(uint8_t *p, uint32_t v)
{
	for (int i = 0; i < 4; i++)
		p[i] = (uint8_t)(v >> (8 * i));
}

static void
put64(uint8_t *p, uint64_t v)
{
	for (int i = 0; i < 8; i++)
		p[i] = (uint8_t)(v >> (8 * i));
}

int
pdelf_load(struct pdelf_obj *o, uint8_t *image, uint64_t va)
{
	const struct pdelf_env *env = o->env;
	uint64_t *symaddr = NULL;
	Rela *buf = NULL;
	uint32_t stub = 0;
	int error = -1;

	o->image_va = va;

	// section contents, bss zeroed
	for (uint32_t i = 0; i < o->shnum; i++) {
		Shdr *s = sh(o, i);

		if (o->secoff[i] == NOT_LOADED)
			continue;

		if (s->sh_type == SHT_NOBITS) {
			fill(image + o->secoff[i], 0, s->sh_size);
		} else if (env->read(env->ctx, s->sh_offset, image + o->secoff[i], s->sh_size) != 0) {
			fail(o, "cannot read section", o->shstrtab + s->sh_name, i);
			return -1;
		}
	}
	fill(image + o->stubs_off, 0xcc, o->cells_off - o->stubs_off);
	fill(image + o->cells_off, 0, o->image_size - o->cells_off);

	// every symbol's final address: defined ones in their section, imports through their stub
	symaddr = env->alloc(env->ctx, o->nsyms * sizeof(uint64_t));
	buf = env->alloc(env->ctx, RELA_CHUNK * sizeof(Rela));
	if (symaddr == NULL || buf == NULL)
		goto out;

	for (uint64_t i = 0; i < o->nsyms; i++) {
		const Sym *s = (const Sym *)o->syms + i;

		symaddr[i] = 0;
		if (s->st_shndx == SHN_ABS) {
			symaddr[i] = s->st_value;
		} else if (s->st_shndx != SHN_UNDEF && s->st_shndx < o->shnum) {
			if (o->secoff[s->st_shndx] != NOT_LOADED)
				symaddr[i] = va + o->secoff[s->st_shndx] + s->st_value;
		} else if (is_import(s, i)) {
			const struct pdelf_target *t = &o->targets[stub];
			uint8_t *p = image + o->stubs_off + (uint64_t)stub * PDELF_STUB_SIZE;
			int reg = thunk_register(symname(o, s));

			if (reg >= 0) {
				write_thunk_stub(p, reg);
				symaddr[i] = va + o->stubs_off + (uint64_t)stub * PDELF_STUB_SIZE;
			} else if (t->is_data && t->addr != 0) {
				symaddr[i] = t->addr;
			} else if (t->is_data) {
				uint8_t *cell = image + o->cells_off + o->celloff[stub];

				for (uint64_t b = 0; t->init != NULL && b < t->size; b++)
					cell[b] = ((const uint8_t *)t->init)[b];
				symaddr[i] = va + o->cells_off + o->celloff[stub];
			} else {
				write_jump_stub(p, t->addr);
				symaddr[i] = va + o->stubs_off + (uint64_t)stub * PDELF_STUB_SIZE;
			}
			stub++;
		}
	}

	// the relocations of every loaded section
	for (uint32_t r = 0; r < o->shnum; r++) {
		Shdr *rs = sh(o, r);
		uint64_t n, done;

		if (rs->sh_type != SHT_RELA || rs->sh_info >= o->shnum || o->secoff[rs->sh_info] == NOT_LOADED)
			continue;

		if (rs->sh_entsize != sizeof(Rela)) {
			fail(o, "odd relocation entry size", o->shstrtab + rs->sh_name, rs->sh_entsize);
			goto out;
		}
		n = rs->sh_size / sizeof(Rela);
		for (done = 0; done < n;) {
			uint64_t take = n - done < RELA_CHUNK ? n - done : RELA_CHUNK;

			if (env->read(env->ctx, rs->sh_offset + done * sizeof(Rela), buf, take * sizeof(Rela)) != 0) {
				fail(o, "cannot read relocations", o->shstrtab + rs->sh_name, done);
				goto out;
			}
			for (uint64_t k = 0; k < take; k++) {
				const Rela *rl = &buf[k];
				uint32_t type = (uint32_t)rl->r_info, si = (uint32_t)(rl->r_info >> 32);
				uint64_t place = o->secoff[rs->sh_info] + rl->r_offset;
				uint64_t P = va + place, S;
				int64_t v;

				if (si >= o->nsyms || place + (type == R_X86_64_64 ? 8 : 4) > o->sections_size) {
					fail(o, "relocation outside the image", "", place);
					goto out;
				}
				S = symaddr[si];
				switch (type) {
				case R_X86_64_NONE:
					break;
				case R_X86_64_64:
					put64(image + place, S + (uint64_t)rl->r_addend);
					break;
				case R_X86_64_PC32:
					v = (int64_t)(S + (uint64_t)rl->r_addend - P);
					if (!fits_s32(v)) {
						fail(o, "PC32 out of range", symname(o, (const Sym *)o->syms + si), P);
						goto out;
					}
					put32(image + place, (uint32_t)v);
					break;
				case R_X86_64_32S:
					v = (int64_t)(S + (uint64_t)rl->r_addend);
					if (!fits_s32(v)) {
						fail(o, "32S out of range, the image must sit in the top 2 GB",
						    symname(o, (const Sym *)o->syms + si), P);
						goto out;
					}
					put32(image + place, (uint32_t)v);
					break;
				default:
					fail(o, "unsupported relocation type", "", type);
					goto out;
				}
			}
			done += take;
		}
	}
	error = 0;
out:
	if (buf != NULL)
		env->free(env->ctx, buf, RELA_CHUNK * sizeof(Rela));
	if (symaddr != NULL)
		env->free(env->ctx, symaddr, o->nsyms * sizeof(uint64_t));
	return error;
}

uint64_t
pdelf_import_slot(const struct pdelf_obj *o, const char *name, int cell)
{
	uint32_t stub = 0;

	for (uint64_t i = 0; i < o->nsyms; i++) {
		const Sym *s = (const Sym *)o->syms + i;

		if (!is_import(s, i))
			continue;

		if (!streq(symname(o, s), name)) {
			stub++;
			continue;
		}
		if (cell)
			return o->celloff[stub] == NOT_LOADED ? 0 : o->image_va + o->cells_off + o->celloff[stub];
		return o->image_va + o->stubs_off + (uint64_t)stub * PDELF_STUB_SIZE;
	}
	return 0;
}

uint64_t
pdelf_export(const struct pdelf_obj *o, const char *name)
{
	for (uint64_t i = 1; i < o->nsyms; i++) {
		const Sym *s = (const Sym *)o->syms + i;

		if ((s->st_info >> 4) == STB_LOCAL || s->st_shndx == SHN_UNDEF || s->st_shndx >= o->shnum)
			continue;

		if (o->secoff[s->st_shndx] != NOT_LOADED && streq(symname(o, s), name))
			return o->image_va + o->secoff[s->st_shndx] + s->st_value;
	}
	return 0;
}

void
pdelf_ranges(const struct pdelf_obj *o, pdelf_range_fn fn, void *ctx)
{
	for (uint32_t i = 0; i < o->shnum; i++) {
		Shdr *s = sh(o, i);

		if (o->secoff[i] != NOT_LOADED)
			fn(ctx, o->secoff[i], align_up(s->sh_size, o->min_align), (s->sh_flags & SHF_WRITE) != 0,
			    (s->sh_flags & SHF_EXECINSTR) != 0);
	}
	fn(ctx, o->stubs_off, o->cells_off - o->stubs_off, 0, 1);
	fn(ctx, o->cells_off, o->image_size - o->cells_off, 1, 0);
}

void
pdelf_close(struct pdelf_obj *o)
{
	const struct pdelf_env *env = o->env;

	if (env == NULL)
		return;

	if (o->targets != NULL)
		env->free(env->ctx, o->targets, (o->nimports ? o->nimports : 1) * sizeof(struct pdelf_target));
	if (o->celloff != NULL)
		env->free(env->ctx, o->celloff, (o->nimports ? o->nimports : 1) * sizeof(uint64_t));
	if (o->syms != NULL)
		env->free(env->ctx, o->syms, o->nsyms * sizeof(Sym));
	if (o->strtab != NULL)
		env->free(env->ctx, o->strtab, o->strtab_size);
	if (o->shstrtab != NULL)
		env->free(env->ctx, o->shstrtab, o->shstrtab_size);
	if (o->secoff != NULL)
		env->free(env->ctx, o->secoff, o->shnum * sizeof(uint64_t));
	if (o->shdrs != NULL)
		env->free(env->ctx, o->shdrs, (uint64_t)o->shnum * sizeof(Shdr));
	o->env = NULL;
}
