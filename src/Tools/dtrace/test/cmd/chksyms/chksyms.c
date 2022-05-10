#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <darwin_shim.h>
#include <dtrace.h>
#include <err.h>
#include <fcntl.h>
#include <gelf.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>


static Elf_Scn *
findelfscn(Elf *elf, GElf_Ehdr *ehdr, char *secname)
{
	GElf_Shdr shdr;
	Elf_Scn *scn;
	char *name;

	for (scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL; ) {
		if (gelf_getshdr(scn, &shdr) != NULL && (name =
		    elf_strptr(elf, ehdr->e_shstrndx, shdr.sh_name)) != NULL &&
		    strcmp(name, secname) == 0)
			return (scn);
	}

	return (NULL);
}

int main(void)
{
	Elf *elf = NULL;
	GElf_Ehdr ehdr;
	Elf_Scn *ctfscn;
	int fd;
	char kernel_path[MAXPATHLEN];

	(void) elf_version(EV_CURRENT);

	// Check that we can get the kernel path

	if (dtrace_kernel_path(kernel_path, sizeof(kernel_path)) != 0) {
		err(1, "could not find currently running kernel binary");
		return 1;
	}

	// Check that this binary contains a CTF segment
	if ((fd = open(kernel_path, O_RDONLY)) == -1) {
		err(2, "failed to open %s: %s", kernel_path, strerror(errno));
		return (2);
	}

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
		err(3, "elf_begin failed to read %s\n", kernel_path);
		return (3);
	}

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		err(4, "gelf_getehdr failed on %s\n", kernel_path);
		return (4);
	}

	ctfscn = findelfscn(elf, &ehdr, ".SUNW_ctf");

	if (ctfscn == NULL || elf_getdata(ctfscn, NULL) == NULL) {
		err(5, "%s has no ctf symbols\n", kernel_path);
		return (5);
	}

	printf("%s has ctf symbols\n", kernel_path);

	(void)elf_end(elf);
	(void)close(fd);

	return (0);
}
