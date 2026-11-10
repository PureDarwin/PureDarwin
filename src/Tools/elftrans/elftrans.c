// elftrans.c - load a converted Linux x86-64 ELF and its interpreter, then start it on Darwin
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include "elfaot_format.h"
#include "elftrans.h"

#define PT_LOAD 1U
#define PT_DYNAMIC 2U
#define PT_INTERP 3U
#define PT_PHDR 6U
#define PF_X 1U
#define PF_W 2U
#define PF_R 4U
#define DT_NULL 0
#define DT_NEEDED 1
#define DT_PLTRELSZ 2
#define DT_HASH 4
#define DT_STRTAB 5
#define DT_SYMTAB 6
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_STRSZ 10
#define DT_SYMENT 11
#define DT_SONAME 14
#define DT_PLTREL 20
#define DT_JMPREL 23
#define R_X86_64_64 1U
#define R_X86_64_GLOB_DAT 6U
#define R_X86_64_JUMP_SLOT 7U
#define R_X86_64_RELATIVE 8U

typedef struct {
    unsigned char ident[16];
    uint16_t type, machine;
    uint32_t version;
    uint64_t entry, phoff, shoff;
    uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ElfHeader;

typedef struct {
    uint32_t type, flags;
    uint64_t offset, vaddr, paddr, filesz, memsz, align;
} ProgHeader;

typedef struct {
    int64_t tag;
    uint64_t value;
} DynEntry;

typedef struct {
    uint32_t name;
    unsigned char info, other;
    uint16_t section;
    uint64_t value, size;
} DynSymbol;

typedef struct {
    uint64_t offset, info;
    int64_t addend;
} DynReloc;

typedef struct {
    uintptr_t lo, hi, bias, entry;
    uintptr_t image_phdr;
    int has_interp;
    const ProgHeader *ph;
    unsigned phnum;
    uintptr_t dynamic;
    size_t dynamic_size;
} LoadedObject;

typedef struct {
    uint64_t needed, soname, strtab, strsz, symtab, syment, hash;
    uint64_t rela, relasz, relaent, jmprel, pltrelsz, pltrel;
} DynInfo;

static uintptr_t guest_lo, guest_hi;

// the low range Linux programs are linked into, 64 KB up to 4 GB, a zero-fill segment of elftrans's own image
// (__GUEST, at 0x10000): the kernel maps it at exec, before anything could be put there
__attribute__((section("__GUEST,__space,zerofill"), used)) static char guest_space[GUEST_HI - GUEST_LO];

int
in_guest_space(uintptr_t lo, uintptr_t hi)
{
    return lo >= GUEST_LO && hi <= GUEST_HI && lo < hi;
}
static uintptr_t syscall_sites[ELFAOT_MAX_SYSCALLS];
static size_t syscall_count;
typedef struct {
    size_t count;
    uintptr_t sites[];
} SiteTable;
static SiteTable *active_sites;
pthread_mutex_t sites_lock = PTHREAD_MUTEX_INITIALIZER;

static int
compare_sites(const void *a, const void *b)
{
    uintptr_t x = *(const uintptr_t *)a;
    uintptr_t y = *(const uintptr_t *)b;
    return (x > y) - (x < y);
}

int
publish_sites(uintptr_t remove_lo, uintptr_t remove_hi,
    const uintptr_t *added, size_t add_count)
{
    SiteTable *old = __atomic_load_n(&active_sites, __ATOMIC_ACQUIRE);
    size_t old_count = old ? old->count : 0;
    size_t kept = 0;
    for (size_t i = 0; i < old_count; ++i) {
        uintptr_t site = old->sites[i];
        if (site < remove_lo || site >= remove_hi) ++kept;
    }
    if (!add_count && kept == old_count) return 0;
    if (add_count > ELFAOT_MAX_SYSCALLS - kept)
        return -1;
    size_t bytes = sizeof(SiteTable) +
        (kept + add_count) * sizeof(uintptr_t);
    SiteTable *next = mmap(NULL, round_up(bytes), PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (next == MAP_FAILED) return -1;
    next->count = 0;
    for (size_t i = 0; i < old_count; ++i) {
        uintptr_t site = old->sites[i];
        if (site < remove_lo || site >= remove_hi)
            next->sites[next->count++] = site;
    }
    for (size_t i = 0; i < add_count; ++i)
        next->sites[next->count++] = added[i];
    qsort(next->sites, next->count, sizeof(uintptr_t), compare_sites);
    for (size_t i = 1; i < next->count; ++i) {
        if (next->sites[i - 1] == next->sites[i]) {
            munmap(next, round_up(bytes));
            return -1;
        }
    }
    __atomic_store_n(&active_sites, next, __ATOMIC_RELEASE);
    return 0;
}

int
is_syscall_site(uintptr_t site)
{
    SiteTable *table = __atomic_load_n(&active_sites, __ATOMIC_ACQUIRE);
    if (!table) return 0;
    size_t left = 0, right = table->count;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        if (table->sites[mid] < site) left = mid + 1;
        else right = mid;
    }
    return left < table->count && table->sites[left] == site;
}

// the footer at the end of a converted file, and where the ELF's own bytes end
static int
read_footer(int fd, AotFooter *footer, uint64_t *payload_size)
{
    struct stat st;

    if (fstat(fd, &st) || st.st_size < (off_t)sizeof(*footer) ||
        pread(fd, footer, sizeof(*footer), st.st_size - sizeof(*footer)) != (ssize_t)sizeof(*footer) ||
        memcmp(footer->magic, ELFAOT_MAGIC, sizeof(footer->magic)) || footer->syscall_count > ELFAOT_MAX_SYSCALLS ||
        footer->syscall_count > ((uint64_t)st.st_size - sizeof(*footer)) / sizeof(uint64_t))
        return -1;
    *payload_size = (uint64_t)st.st_size - sizeof(*footer) - footer->syscall_count * sizeof(uint64_t);
    return 0;
}

// the ELF header and program headers of a converted file, read into memory
static int
read_headers(int fd, uint64_t payload_size, ElfHeader *eh, ProgHeader **phs)
{
    if (pread(fd, eh, sizeof(*eh), 0) != (ssize_t)sizeof(*eh) || memcmp(eh->ident, "\177ELF", 4) ||
        eh->ident[4] != 2 || eh->ident[5] != 1 || eh->machine != 62 || (eh->type != 2 && eh->type != 3) ||
        eh->phentsize != sizeof(ProgHeader) || !eh->phnum || eh->phnum > 128 || eh->phoff > payload_size ||
        eh->phnum > (payload_size - eh->phoff) / sizeof(ProgHeader))
        return -1;
    *phs = malloc(eh->phnum * sizeof(ProgHeader));
    if (!*phs) return -1;
    if (pread(fd, *phs, eh->phnum * sizeof(ProgHeader), eh->phoff) != (ssize_t)(eh->phnum * sizeof(ProgHeader))) {
        free(*phs);
        return -1;
    }
    return 0;
}

// sites of a converted file mapped by the guest itself (ld.so loading a library): those inside this mapping
int
register_mapped_aot(int fd, off_t offset, uintptr_t address, size_t length)
{
    AotFooter footer;
    ElfHeader eh;
    ProgHeader *ph;
    uint64_t payload_size;

    if (read_footer(fd, &footer, &payload_size) || read_headers(fd, payload_size, &eh, &ph))
        return -1;
    uintptr_t *added = malloc(footer.syscall_count * sizeof(uintptr_t) + 1);
    size_t added_count = 0;
    int valid = added != NULL;
    for (uint64_t i = 0; valid && i < footer.syscall_count; ++i) {
        uint64_t site;
        if (pread(fd, &site, sizeof(site), payload_size + i * sizeof(site)) != sizeof(site)) {
            valid = 0;
            break;
        }
        for (unsigned j = 0; j < eh.phnum; ++j) {
            if (ph[j].type != PT_LOAD || !(ph[j].flags & PF_X) || site < ph[j].vaddr ||
                site - ph[j].vaddr > ph[j].filesz || ph[j].filesz - (site - ph[j].vaddr) < 2)
                continue;
            uint64_t file_offset = ph[j].offset + site - ph[j].vaddr;
            if (file_offset < (uint64_t)offset || file_offset - offset > length ||
                length - (file_offset - offset) < 2)
                continue;
            uintptr_t mapped_site = address + file_offset - offset;
            if (((unsigned char *)mapped_site)[0] != 0xcc || ((unsigned char *)mapped_site)[1] != 0x90)
                valid = 0;
            else
                added[added_count++] = mapped_site;
            break;
        }
    }
    if (valid)
        valid = !publish_sites(0, 0, added, added_count);
    free(added);
    free(ph);
    if (!valid) return -1;
    if (!guest_lo || address < guest_lo) guest_lo = address;
    if (address + length > guest_hi) guest_hi = address + length;
    return 0;
}

// a converted image mapped from its file: an ET_EXEC at the addresses it was linked for, an ET_DYN wherever
// there is room, data pages private copies, bss anonymous, everything writable until protect_object
static int
map_image(int fd, LoadedObject *obj, int require_entry)
{
    AotFooter footer;
    ElfHeader eh;
    ProgHeader *ph;
    uint64_t payload_size;
    uintptr_t lo = UINTPTR_MAX, hi = 0;

    if (read_footer(fd, &footer, &payload_size) || read_headers(fd, payload_size, &eh, &ph))
        return -1;
    for (unsigned i = 0; i < eh.phnum; ++i) {
        if (ph[i].type == PT_INTERP)
            obj->has_interp = 1;
        if (ph[i].type != PT_LOAD)
            continue;
        if (ph[i].filesz > ph[i].memsz || ph[i].offset > payload_size ||
            ph[i].filesz > payload_size - ph[i].offset || ph[i].vaddr > UINTPTR_MAX - ph[i].memsz ||
            (ph[i].vaddr - ph[i].offset) % PAGE)
            goto fail;
        if (round_down(ph[i].vaddr) < lo) lo = round_down(ph[i].vaddr);
        if (round_up(ph[i].vaddr + ph[i].memsz) > hi) hi = round_up(ph[i].vaddr + ph[i].memsz);
    }
    if (hi <= lo || hi - lo > 1024UL * 1024 * 1024 ||
        ((require_entry || eh.entry) && (eh.entry < lo || eh.entry >= hi)))
        goto fail;

    // one reservation for the whole image, the segments then replace parts of it: an ET_EXEC's at exactly its
    // link address, which a fixed mach_vm_allocate takes only when nothing is mapped there
    void *base;
    if (eh.type == 2 && in_guest_space(lo, hi)) {
        base = mmap((void *)lo, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0);
        if (base == MAP_FAILED) goto fail;
    } else if (eh.type == 2) {
        mach_vm_address_t at = lo;
        kern_return_t kr = mach_vm_allocate(mach_task_self(), &at, hi - lo, VM_FLAGS_FIXED);
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "elftrans: %p..%p, where the program was linked, is in use (%d)\n", (void *)lo,
                (void *)hi, kr);
            goto fail;
        }
        base = (void *)lo;
        mprotect(base, hi - lo, PROT_NONE);
    } else if ((base = mmap(NULL, hi - lo, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0)) == MAP_FAILED) {
        goto fail;
    }
    uintptr_t bias = (uintptr_t)base - lo;
    for (unsigned i = 0; i < eh.phnum; ++i) {
        if (ph[i].type != PT_LOAD || !ph[i].memsz)
            continue;
        uintptr_t start = bias + round_down(ph[i].vaddr);
        uintptr_t file_end = bias + ph[i].vaddr + ph[i].filesz, end = bias + round_up(ph[i].vaddr + ph[i].memsz);
        if (ph[i].filesz && mmap((void *)start, round_up(file_end) - start, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_FIXED, fd, (off_t)round_down(ph[i].offset)) == MAP_FAILED)
            goto fail;
        // the rest of the last file page is bss, the pages after it anonymous memory
        if (ph[i].memsz > ph[i].filesz) {
            uintptr_t zero_end = round_up(file_end) < end ? round_up(file_end) : end;
            if (ph[i].filesz)
                memset((void *)file_end, 0, zero_end - file_end);
            uintptr_t anon = ph[i].filesz ? round_up(file_end) : start;
            if (anon < end && mmap((void *)anon, end - anon, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED)
                goto fail;
        }
    }

    if (footer.syscall_count > ELFAOT_MAX_SYSCALLS - syscall_count) goto fail;
    for (uint64_t i = 0; i < footer.syscall_count; ++i) {
        uint64_t site;
        if (pread(fd, &site, sizeof(site), payload_size + i * sizeof(site)) != sizeof(site) || site < lo ||
            site > hi - 2 || ((unsigned char *)(bias + site))[0] != 0xcc || ((unsigned char *)(bias + site))[1] != 0x90)
            goto fail;
        syscall_sites[syscall_count++] = bias + site;
    }

    obj->lo = bias + lo, obj->hi = bias + hi, obj->bias = bias;
    obj->entry = eh.entry ? bias + eh.entry : 0;
    obj->ph = ph, obj->phnum = eh.phnum;
    // the program headers as the guest sees them: PT_PHDR, else inside the segment that maps them
    obj->image_phdr = 0;
    for (unsigned i = 0; i < eh.phnum; ++i) {
        if (ph[i].type == PT_PHDR)
            obj->image_phdr = bias + ph[i].vaddr;
        else if (!obj->image_phdr && ph[i].type == PT_LOAD && eh.phoff >= ph[i].offset &&
            eh.phoff - ph[i].offset < ph[i].filesz)
            obj->image_phdr = bias + ph[i].vaddr + (eh.phoff - ph[i].offset);
        if (ph[i].type == PT_DYNAMIC)
            obj->dynamic = bias + ph[i].vaddr, obj->dynamic_size = ph[i].memsz;
    }
    if (!guest_lo || obj->lo < guest_lo) guest_lo = obj->lo;
    if (obj->hi > guest_hi) guest_hi = obj->hi;
    return 0;
fail:
    free(ph);
    return -1;
}

static void *
object_ptr(const LoadedObject *obj, uint64_t address, size_t size,
    int file_backed)
{
    for (unsigned i = 0; i < obj->phnum; ++i) {
        const ProgHeader *ph = &obj->ph[i];
        uint64_t limit = file_backed ? ph->filesz : ph->memsz;
        if (ph->type == PT_LOAD && address >= ph->vaddr &&
            address - ph->vaddr <= limit &&
            size <= limit - (address - ph->vaddr))
            return (void *)(obj->bias + address);
    }
    return NULL;
}

static int
dynamic_info(const LoadedObject *obj, DynInfo *out)
{
    memset(out, 0, sizeof(*out));
    if (!obj->dynamic) return 0;
    if (!obj->dynamic_size ||
        !object_ptr(obj, obj->dynamic - obj->bias,
            obj->dynamic_size, 1))
        return -1;

    const DynEntry *entries = (const DynEntry *)obj->dynamic;
    size_t count = obj->dynamic_size / sizeof(*entries);
    for (size_t i = 0; i < count; ++i) {
        switch (entries[i].tag) {
        case DT_NULL: return 0;
        case DT_NEEDED: out->needed = entries[i].value; break;
        case DT_SONAME: out->soname = entries[i].value; break;
        case DT_STRTAB: out->strtab = entries[i].value; break;
        case DT_STRSZ: out->strsz = entries[i].value; break;
        case DT_SYMTAB: out->symtab = entries[i].value; break;
        case DT_SYMENT: out->syment = entries[i].value; break;
        case DT_HASH: out->hash = entries[i].value; break;
        case DT_RELA: out->rela = entries[i].value; break;
        case DT_RELASZ: out->relasz = entries[i].value; break;
        case DT_RELAENT: out->relaent = entries[i].value; break;
        case DT_JMPREL: out->jmprel = entries[i].value; break;
        case DT_PLTRELSZ: out->pltrelsz = entries[i].value; break;
        case DT_PLTREL: out->pltrel = entries[i].value; break;
        default: break;
        }
    }
    return -1;
}

static const char *
dyn_string(const LoadedObject *obj, const DynInfo *dyn, uint64_t offset)
{
    if (offset >= dyn->strsz) return NULL;
    const char *strings = object_ptr(obj, dyn->strtab, dyn->strsz, 1);
    if (!strings || !memchr(strings + offset, 0, dyn->strsz - offset))
        return NULL;
    return strings + offset;
}

static uintptr_t
lookup_symbol(const LoadedObject *obj, const DynInfo *dyn,
    const char *name)
{
    const uint32_t *hash = object_ptr(obj, dyn->hash, 8, 1);
    if (!hash || dyn->syment != sizeof(DynSymbol)) return 0;
    uint32_t count = hash[1];
    if (count > 65536) return 0;
    const DynSymbol *symbols = object_ptr(obj, dyn->symtab,
        (size_t)count * sizeof(*symbols), 1);
    if (!symbols) return 0;
    for (uint32_t i = 0; i < count; ++i) {
        const char *candidate = dyn_string(obj, dyn, symbols[i].name);
        if (symbols[i].section && candidate && !strcmp(candidate, name))
            return obj->bias + symbols[i].value;
    }
    return 0;
}

static int
relocate_table(const LoadedObject *obj, const DynInfo *dyn,
    const LoadedObject *library, const DynInfo *libdyn,
    uint64_t address, uint64_t size)
{
    if (!size) return 0;
    if (size % sizeof(DynReloc)) return -1;
    const DynReloc *relocs = object_ptr(obj, address, size, 1);
    if (!relocs) return -1;

    const uint32_t *hash = object_ptr(obj, dyn->hash, 8, 1);
    uint32_t symbols_count = hash ? hash[1] : 0;
    const DynSymbol *symbols = NULL;
    if (symbols_count && symbols_count <= 65536 &&
        dyn->syment == sizeof(DynSymbol))
        symbols = object_ptr(obj, dyn->symtab,
            (size_t)symbols_count * sizeof(*symbols), 1);

    for (uint64_t i = 0; i < size / sizeof(*relocs); ++i) {
        uint32_t type = (uint32_t)relocs[i].info;
        uint32_t symbol = (uint32_t)(relocs[i].info >> 32);
        uint64_t *target = object_ptr(obj, relocs[i].offset, 8, 0);
        if (!target) return -1;
        if (type == R_X86_64_RELATIVE) {
            *target = obj->bias + relocs[i].addend;
            continue;
        }
        if (type != R_X86_64_64 && type != R_X86_64_GLOB_DAT &&
            type != R_X86_64_JUMP_SLOT)
            return -1;
        if (!symbols || symbol >= symbols_count) return -1;
        const char *name = dyn_string(obj, dyn, symbols[symbol].name);
        if (!name) return -1;
        uintptr_t value = lookup_symbol(library, libdyn, name);
        if (!value && (symbols[symbol].info >> 4) != 2) return -1;
        *target = value + relocs[i].addend;
    }
    return 0;
}

static int
link_objects(const LoadedObject *main_obj, const LoadedObject *library)
{
    DynInfo main_dyn, lib_dyn;
    if (dynamic_info(main_obj, &main_dyn) ||
        dynamic_info(library, &lib_dyn))
        return -1;
    const char *needed = dyn_string(main_obj, &main_dyn, main_dyn.needed);
    const char *soname = lib_dyn.soname ?
        dyn_string(library, &lib_dyn, lib_dyn.soname) : NULL;
    if (!needed || (soname && strcmp(needed, soname)) ||
        (main_dyn.relasz && main_dyn.relaent != sizeof(DynReloc)) ||
        (main_dyn.pltrelsz && main_dyn.pltrel != DT_RELA) ||
        (lib_dyn.relasz && lib_dyn.relaent != sizeof(DynReloc)) ||
        (lib_dyn.pltrelsz && lib_dyn.pltrel != DT_RELA))
        return -1;

    if (relocate_table(library, &lib_dyn, library, &lib_dyn,
            lib_dyn.rela, lib_dyn.relasz) ||
        relocate_table(library, &lib_dyn, library, &lib_dyn,
            lib_dyn.jmprel, lib_dyn.pltrelsz) ||
        relocate_table(main_obj, &main_dyn, library, &lib_dyn,
            main_dyn.rela, main_dyn.relasz) ||
        relocate_table(main_obj, &main_dyn, library, &lib_dyn,
            main_dyn.jmprel, main_dyn.pltrelsz))
        return -1;
    return 0;
}

static int
protect_object(const LoadedObject *obj)
{
    for (unsigned i = 0; i < obj->phnum; ++i) {
        const ProgHeader *ph = &obj->ph[i];
        if (ph->type != PT_LOAD || !ph->memsz) continue;

        int prot = 0;
        if (ph->flags & PF_R) prot |= PROT_READ;
        if (ph->flags & PF_W) prot |= PROT_WRITE;
        if (ph->flags & PF_X) prot |= PROT_EXEC;
        if (mprotect((void *)(obj->bias + round_down(ph->vaddr)),
            round_up(ph->vaddr + ph->memsz) - round_down(ph->vaddr),
            prot)) {
            fprintf(stderr, "elftrans: protect %p: %s\n",
                (void *)(obj->bias + ph->vaddr), strerror(errno));
            return -1;
        }
    }
    return 0;
}

static int
load_path(const char *path, LoadedObject *obj, int require_entry)
{
    int fd = open(path, O_RDONLY);
    int result = fd < 0 ? -1 : map_image(fd, obj, require_entry);

    if (result) fprintf(stderr, "elftrans: %s: cannot map the converted image\n", path);
    if (fd >= 0) close(fd);
    return result;
}

int
is_elf_path(const char *path)
{
    unsigned char magic[4];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    int result = read(fd, magic, sizeof(magic)) == sizeof(magic) &&
        !memcmp(magic, "\177ELF", sizeof(magic));
    close(fd);
    return result;
}

static int
elf_interpreter(const char *path, char *out, size_t capacity)
{
    ElfHeader eh;
    int fd = open(path, O_RDONLY);
    if (fd < 0 || read(fd, &eh, sizeof(eh)) != sizeof(eh) ||
        memcmp(eh.ident, "\177ELF", 4) ||
        eh.phentsize != sizeof(ProgHeader) || eh.phnum > 128) {
        if (fd >= 0) close(fd);
        return -1;
    }
    for (unsigned i = 0; i < eh.phnum; ++i) {
        ProgHeader ph;
        if (pread(fd, &ph, sizeof(ph), eh.phoff + i * sizeof(ph)) !=
            sizeof(ph)) break;
        if (ph.type != PT_INTERP) continue;
        if (!ph.filesz || ph.filesz > capacity ||
            pread(fd, out, ph.filesz, ph.offset) != (ssize_t)ph.filesz ||
            out[ph.filesz - 1]) break;
        close(fd);
        return 1;
    }
    close(fd);
    return 0;
}

static int
make_cache_dir(const char *path)
{
    char current[PATH_MAX];
    size_t length = strlen(path);
    if (!length || length >= sizeof(current)) return -1;
    memcpy(current, path, length + 1);
    for (char *p = current + 1; ; ++p) {
        if (*p && *p != '/') continue;
        char saved = *p;
        *p = 0;
        if (mkdir(current, 0700) && errno != EEXIST) return -1;
        *p = saved;
        if (!saved) return 0;
    }
}

#ifdef __APPLE__
#define MTIME(st) ((st)->st_mtimespec)
#else
#define MTIME(st) ((st)->st_mtim)
#endif

// a cached conversion stands for the file while its device, inode, size and modification time are the same
static int
footer_matches(const AotFooter *footer, const struct stat *st)
{
    return footer->source_dev == (uint64_t)st->st_dev && footer->source_ino == (uint64_t)st->st_ino &&
        footer->source_size == (uint64_t)st->st_size && footer->source_mtime_sec == MTIME(st).tv_sec &&
        footer->source_mtime_nsec == MTIME(st).tv_nsec;
}

// ELFTRANS_CACHE_DIR, else the user's Library/Caches, else a directory of the user's own in /tmp
static int
cache_dir(char *out, size_t capacity)
{
    const char *base = getenv("ELFTRANS_CACHE_DIR"), *home = getenv("HOME");
    int n;

    if (base) n = snprintf(out, capacity, "%s", base);
    else if (home) n = snprintf(out, capacity, "%s/Library/Caches/elftrans", home);
    else n = snprintf(out, capacity, "/tmp/elftrans-%u", (unsigned)getuid());
    return n < 0 || (size_t)n >= capacity || make_cache_dir(out) ? -1 : 0;
}

// the converted copy of source: the file itself when elfaot already made it, else the cache's copy, made
// now by elfaot when there is none or the file changed since
int
prepare_aot(const char *source, char *out, size_t capacity)
{
    static const char version[] = ELFAOT_MAGIC "-capstone5";
    const char *converter = getenv("ELFTRANS_ELFAOT");
    char directory[PATH_MAX], temporary[PATH_MAX];
    AotFooter footer;
    uint64_t payload_size, hash = UINT64_C(14695981039346656037);
    struct stat st;
    int fd = open(source, O_RDONLY);

    if (fd < 0 || fstat(fd, &st)) {
        if (fd >= 0) close(fd);
        return -1;
    }
    int converted = !read_footer(fd, &footer, &payload_size);
    close(fd);
    if (converted)
        return (size_t)snprintf(out, capacity, "%s", source) >= capacity ? -1 : 0;

    uint64_t key[] = { (uint64_t)st.st_dev, (uint64_t)st.st_ino, (uint64_t)st.st_size,
        (uint64_t)MTIME(&st).tv_sec, (uint64_t)MTIME(&st).tv_nsec };
    for (const unsigned char *p = (const unsigned char *)version; *p; ++p)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    for (const unsigned char *p = (const unsigned char *)key; p < (const unsigned char *)(key + 5); ++p)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    if (cache_dir(directory, sizeof(directory)) ||
        (size_t)snprintf(out, capacity, "%s/%016llx.aot", directory, (unsigned long long)hash) >= capacity ||
        (size_t)snprintf(temporary, sizeof(temporary), "%s.%ld.tmp", out, (long)getpid()) >= sizeof(temporary))
        return -1;

    fd = open(out, O_RDONLY);
    if (fd >= 0) {
        int valid = !read_footer(fd, &footer, &payload_size) && footer_matches(&footer, &st);
        close(fd);
        if (valid) return 0;
    }
    unlink(temporary);
    pid_t child = fork();
    if (child < 0) return -1;
    if (!child) {
        execl(converter ? converter : "/usr/bin/elfaot", "elfaot", source, temporary, (char *)NULL);
        _exit(127);
    }
    int status;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) ||
        rename(temporary, out)) {
        unlink(temporary);
        return -1;
    }
    return 0;
}

// a converted copy of the file behind fd, for a mapping the guest makes executable
int
aot_open(int fd)
{
    char path[PATH_MAX], converted[PATH_MAX];

    if (fcntl(fd, F_GETPATH, path) == -1 || !is_elf_path(path) || prepare_aot(path, converted, sizeof(converted)))
        return -1;
    return open(converted, O_RDONLY);
}

#define STACK_SIZE (8UL * 1024 * 1024)

int
main(int argc, char **argv)
{
    extern char **environ;
    LoadedObject main_obj = {0}, library = {0};
    DynInfo main_dyn;
    size_t env_count = 0, guest_count, guest_first = 2;
    char main_aot[PATH_MAX], library_aot[PATH_MAX], interpreter[PATH_MAX];
    const char *library_path = NULL;

    // an exec from a guest: --exec (no library argument), its argv[0], mask and ignored signals
    const char *argv0 = NULL;
    uint64_t inherit_mask = 0, inherit_ignored = 0;
    int from_exec = 0;

    while (argc > 2 && !strncmp(argv[1], "--", 2)) {
        if (!strcmp(argv[1], "--exec")) {
            from_exec = 1, argv++, argc--;
            continue;
        }
        if (argc < 4)
            break;
        if (!strcmp(argv[1], "--argv0"))
            argv0 = argv[2];
        else if (!strcmp(argv[1], "--sigmask"))
            inherit_mask = strtoull(argv[2], NULL, 16);
        else if (!strcmp(argv[1], "--sigign"))
            inherit_ignored = strtoull(argv[2], NULL, 16);
        else
            break;
        argv += 2, argc -= 2;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: elftrans linux-elf [library] [args...]\n");
        return 2;
    }
    if (getenv("ELFTRANS_TRACE")) elftrans_set_trace(2048);

    int has_interpreter = elf_interpreter(argv[1], interpreter, sizeof(interpreter));
    if (has_interpreter < 0 || prepare_aot(argv[1], main_aot, sizeof(main_aot)) ||
        load_path(main_aot, &main_obj, 1)) {
        fprintf(stderr, "elftrans: %s: not a Linux x86-64 program elftrans can load\n", argv[1]);
        return 1;
    }
    // static-pie has PT_DYNAMIC and relocates itself, only an interpreter or a DT_NEEDED brings in a library
    int needs_library = main_obj.has_interp ||
        (main_obj.dynamic && !dynamic_info(&main_obj, &main_dyn) && main_dyn.needed);
    if (needs_library && !main_obj.has_interp && !from_exec && argc > 2 && is_elf_path(argv[2])) {
        library_path = argv[2];
        guest_first = 3;
    } else if (has_interpreter > 0) {
        library_path = interpreter;
    }
    guest_count = (size_t)argc > guest_first ? (size_t)argc - guest_first : 0;
    if (needs_library && (!library_path || prepare_aot(library_path, library_aot, sizeof(library_aot)) ||
            load_path(library_aot, &library, main_obj.has_interp) ||
            (!main_obj.has_interp && link_objects(&main_obj, &library)))) {
        fprintf(stderr, "elftrans: %s: its library could not be loaded\n", argv[1]);
        return 1;
    }
    if (protect_object(&main_obj) || (needs_library && protect_object(&library)))
        return 1;
    guest_library_bias = library.bias;
    elftrans_set_brk(main_obj.hi);
    if (publish_sites(0, 0, syscall_sites, syscall_count) || elftrans_install()) {
        perror("elftrans");
        return 1;
    }
    if (from_exec)
        signals_inherit(inherit_mask, inherit_ignored);

    void *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (stack == MAP_FAILED) {
        perror("mmap stack");
        return 1;
    }
    while (environ[env_count]) {
        if (++env_count > 4096) return 1;
    }

    // the Linux process stack: argc, argv, envp, then the auxiliary vector, AT_RANDOM's bytes above it all
    uintptr_t *random = (uintptr_t *)((uintptr_t)stack + STACK_SIZE) - 2;
    arc4random_buf(random, 16);
    size_t words = 1 + 1 + guest_count + 1 + env_count + 1 + 2 * 16;
    uintptr_t *sp = (uintptr_t *)((uintptr_t)(random - words) & ~(uintptr_t)15), *w = sp;
    *w++ = 1 + guest_count;
    guest_argv = (const char *const *)w, guest_argc = (int)(1 + guest_count);
    if (!realpath(argv[1], guest_exe))
        snprintf(guest_exe, PATH_MAX, "%s", argv[1]);
    *w++ = (uintptr_t)(argv0 ? argv0 : argv[1]);
    for (size_t i = 0; i < guest_count; ++i)
        *w++ = (uintptr_t)argv[guest_first + i];
    *w++ = 0;
    for (size_t i = 0; i < env_count; ++i)
        *w++ = (uintptr_t)environ[i];
    *w++ = 0;
    const uintptr_t aux[][2] = {
        { 3, main_obj.image_phdr }, { 4, sizeof(ProgHeader) }, { 5, main_obj.phnum }, { 6, PAGE },
        { 7, main_obj.has_interp ? library.bias : 0 }, { 8, 0 }, { 9, main_obj.entry },
        { 11, getuid() }, { 12, geteuid() }, { 13, getgid() }, { 14, getegid() }, { 23, 0 },
        { 25, (uintptr_t)random }, { 31, (uintptr_t)argv[1] }, { 0, 0 },
    };
    for (size_t i = 0; i < sizeof(aux) / sizeof(aux[0]); ++i)
        *w++ = aux[i][0], *w++ = aux[i][1];

    guest_in_host = 0;
    __asm__ volatile("mov %0, %%rsp\n\txor %%rbp, %%rbp\n\tjmp *%1"
        :: "r"(sp), "r"(main_obj.has_interp ? library.entry : main_obj.entry) : "memory");
    __builtin_unreachable();
}
