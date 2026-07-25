/*
 * Link-time-only stubs for host-otool (see nix/pkgs/host-otool.nix).
 *
 * All of these belong to otool's ObjC2 class/selector/CFString symbol-name
 * printing during instruction disassembly, never reached from otool's core
 * Mach-O header/load-command listing (all this host build is used for:
 * meson's darwin rpath introspection during nix builds). The real
 * implementations need Apple's real objc/runtime.h (print_objc2_64bit.c,
 * skipped entirely) so they're never linked in for real here.
 */

#include <stddef.h>
#include <stdint.h>
#include "ofile_print.h"

char *
get_objc2_64bit_cfstring_name(
    uint64_t p,
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex object_byte_sex,
    char *object_addr,
    uint64_t object_size,
    struct nlist_64 *symbols64,
    uint32_t nsymbols,
    char *strings,
    uint32_t strings_size,
    cpu_type_t cputype)
{
	return NULL;
}

char *
get_objc2_64bit_class_name(
    uint64_t p,
    uint64_t address_of_p,
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex object_byte_sex,
    char *object_addr,
    uint64_t object_size,
    struct nlist_64 *symbols64,
    uint32_t nsymbols,
    char *strings,
    uint32_t strings_size,
    cpu_type_t cputype)
{
	return NULL;
}

uint64_t
get_objc2_64bit_selref(
    uint64_t address_of_p,
    struct load_command *load_commands,
    uint32_t ncmds,
    uint32_t sizeofcmds,
    enum byte_sex object_byte_sex,
    char *object_addr,
    uint64_t object_size,
    struct nlist_64 *symbols64,
    uint32_t nsymbols,
    char *strings,
    uint32_t strings_size,
    cpu_type_t cputype)
{
	return 0;
}

char *
__cxa_demangle(const char *mangled_name, char *output_buffer,
    size_t *length, int *status)
{
	if (status != NULL)
		*status = -1;
	return NULL;
}
