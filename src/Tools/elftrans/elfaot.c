// elfaot.c - turn the syscall instructions of an x86-64 Linux ELF into int3 then nop, and list where they are
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <capstone/capstone.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "elfaot_format.h"

#ifdef __APPLE__
#define MTIME(st) ((st)->st_mtimespec)
#else
#define MTIME(st) ((st)->st_mtim)
#endif

static int
transfer(int fd, unsigned char *buffer, size_t size, int output)
{
    while (size) {
        ssize_t n = output ? write(fd, buffer, size) : read(fd, buffer, size);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        buffer += n;
        size -= (size_t)n;
    }
    return 0;
}

// one run of code: every 2-byte syscall becomes int3 then nop, an int 0x80 (the 32-bit ABI) is refused
static int
scan(csh decoder, unsigned char *file, uint64_t offset, uint64_t size, uint64_t address, uint64_t *sites,
    unsigned *count)
{
    const uint8_t *code = file + offset;
    size_t left = size;
    uint64_t pc = address;
    cs_insn *insn = cs_malloc(decoder);

    if (!insn) return -1;
    while (left) {
        const uint8_t *start = code;

        // bytes that decode to nothing are skipped one at a time
        if (!cs_disasm_iter(decoder, &code, &left, &pc, insn)) {
            ++code, --left, ++pc;
            continue;
        }
        if (insn->id == X86_INS_SYSCALL && insn->size == 2) {
            if (*count == ELFAOT_MAX_SYSCALLS) break;
            file[start - file] = 0xcc;
            file[start - file + 1] = 0x90;
            sites[(*count)++] = insn->address;
        } else if (insn->id == X86_INS_INT && insn->size == 2 && start[1] == 0x80) {
            fprintf(stderr, "elfaot: int 0x80 at %#llx\n", (unsigned long long)insn->address);
            break;
        }
    }
    cs_free(insn, 1);
    return left ? -1 : 0;
}

// the executable sections when the file has a section table, else the executable segments
static int
convert(unsigned char *file, size_t size, uint64_t *sites, unsigned *count)
{
    Elf64_Ehdr *eh = (Elf64_Ehdr *)file;
    Elf64_Phdr *ph;
    Elf64_Shdr *sh = NULL;
    csh decoder;
    int sections = 0, result = 0;

    if (size < sizeof(*eh) || memcmp(eh->e_ident, ELFMAG, SELFMAG) || eh->e_ident[EI_CLASS] != ELFCLASS64 ||
        eh->e_ident[EI_DATA] != ELFDATA2LSB || eh->e_machine != EM_X86_64 ||
        (eh->e_type != ET_EXEC && eh->e_type != ET_DYN) || eh->e_phentsize != sizeof(Elf64_Phdr) ||
        !eh->e_phnum || eh->e_phoff > size || eh->e_phnum > (size - eh->e_phoff) / sizeof(Elf64_Phdr))
        return -1;
    ph = (Elf64_Phdr *)(file + eh->e_phoff);
    if (eh->e_shoff && eh->e_shnum && eh->e_shentsize == sizeof(Elf64_Shdr) && eh->e_shoff <= size &&
        eh->e_shnum <= (size - eh->e_shoff) / sizeof(Elf64_Shdr))
        sh = (Elf64_Shdr *)(file + eh->e_shoff);

    if (cs_open(CS_ARCH_X86, CS_MODE_64, &decoder) != CS_ERR_OK)
        return -1;
    for (unsigned i = 0; sh && i < eh->e_shnum && !result; ++i) {
        if (sh[i].sh_type != SHT_PROGBITS || (sh[i].sh_flags & (SHF_ALLOC | SHF_EXECINSTR)) !=
            (SHF_ALLOC | SHF_EXECINSTR))
            continue;
        if (sh[i].sh_offset > size || sh[i].sh_size > size - sh[i].sh_offset) {
            result = -1;
            break;
        }
        sections = 1;
        result = scan(decoder, file, sh[i].sh_offset, sh[i].sh_size, sh[i].sh_addr, sites, count);
    }
    for (unsigned i = 0; !sections && i < eh->e_phnum && !result; ++i) {
        if (ph[i].p_type != PT_LOAD || !(ph[i].p_flags & PF_X))
            continue;
        if (ph[i].p_offset > size || ph[i].p_filesz > size - ph[i].p_offset) {
            result = -1;
            break;
        }
        result = scan(decoder, file, ph[i].p_offset, ph[i].p_filesz, ph[i].p_vaddr, sites, count);
    }
    cs_close(&decoder);
    return result;
}

int
main(int argc, char **argv)
{
    struct stat st;
    unsigned char *file;
    unsigned count = 0;
    uint64_t *sites;
    AotFooter footer = {0};
    int in, out;

    if (argc != 3) {
        fprintf(stderr, "usage: elfaot input-elf output-elf\n");
        return 2;
    }
    in = open(argv[1], O_RDONLY);
    if (in < 0 || fstat(in, &st) || st.st_size <= 0 || st.st_size > 1024 * 1024 * 1024) {
        perror(argv[1]);
        return 1;
    }
    file = malloc((size_t)st.st_size);
    sites = calloc(ELFAOT_MAX_SYSCALLS, sizeof(*sites));
    if (!file || !sites || transfer(in, file, (size_t)st.st_size, 0)) {
        perror("read");
        return 1;
    }
    close(in);
    if (convert(file, (size_t)st.st_size, sites, &count)) {
        fprintf(stderr, "elfaot: %s: not a convertible x86-64 ELF\n", argv[1]);
        return 1;
    }
    memcpy(footer.magic, ELFAOT_MAGIC, sizeof(footer.magic));
    footer.syscall_count = count;
    footer.source_dev = (uint64_t)st.st_dev, footer.source_ino = (uint64_t)st.st_ino;
    footer.source_size = (uint64_t)st.st_size;
    footer.source_mtime_sec = MTIME(&st).tv_sec, footer.source_mtime_nsec = MTIME(&st).tv_nsec;
    out = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0755);
    if (out < 0 || transfer(out, file, (size_t)st.st_size, 1) ||
        transfer(out, (unsigned char *)sites, count * sizeof(*sites), 1) ||
        transfer(out, (unsigned char *)&footer, sizeof(footer), 1)) {
        perror(argv[2]);
        return 1;
    }
    close(out);
    fprintf(stderr, "elfaot: converted %u syscall sites\n", count);
    return 0;
}
