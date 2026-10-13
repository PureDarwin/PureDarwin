// pdbless: lists, adds, orders and removes firmware boot entries (Boot####, BootOrder) through PDEFIVariables
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "PDEFIVariables.h"

// EFI_GLOBAL_VARIABLE, 8BE4DF61-93CA-11D2-AA0D-00E098032B8C as it sits in memory
static const uint8_t kGlobalGuid[16] = { 0x61, 0xDF, 0xE4, 0x8B, 0xCA, 0x93, 0xD2, 0x11,
                                         0xAA, 0x0D, 0x00, 0xE0, 0x98, 0x03, 0x2B, 0x8C };
#define kAttrs      0x7     // non volatile, boot service and runtime access
#define kActive     0x1     // LOAD_OPTION_ACTIVE

static io_connect_t conn;

static int open_service(void)
{
	io_service_t svc = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching("PDEFIVariables"));
	kern_return_t kr;

	if (svc == IO_OBJECT_NULL) {
		fprintf(stderr, "pdbless: no PDEFIVariables, the firmware gave no runtime services\n");
		return -1;
	}
	kr = IOServiceOpen(svc, mach_task_self(), 0, &conn);
	IOObjectRelease(svc);
	if (kr != KERN_SUCCESS) {
		fprintf(stderr, "pdbless: cannot open PDEFIVariables (0x%x), run as root\n", kr);
		return -1;
	}
	return 0;
}

static kern_return_t call(uint32_t sel, const struct pd_efivar_request *in, struct pd_efivar_request *out)
{
	size_t outsize = out ? sizeof(*out) : 0;
	return IOConnectCallStructMethod(conn, sel, in, sizeof(*in), out, out ? &outsize : NULL);
}

static kern_return_t get_var(const char *name, struct pd_efivar_request *out)
{
	static struct pd_efivar_request in;

	memset(&in, 0, sizeof(in));
	memcpy(in.guid, kGlobalGuid, 16);
	strlcpy(in.name, name, sizeof(in.name));
	return call(kPDEFIVarGet, &in, out);
}

static kern_return_t set_var(const char *name, const void *data, uint32_t size)
{
	static struct pd_efivar_request in;

	memset(&in, 0, sizeof(in));
	memcpy(in.guid, kGlobalGuid, 16);
	strlcpy(in.name, name, sizeof(in.name));
	in.attrs = kAttrs;
	in.size = size;
	if (size)
		memcpy(in.data, data, size);
	return call(kPDEFIVarSet, &in, NULL);
}

// the description of a load option, ASCII out of its UCS-2
static void describe(const uint8_t *opt, uint32_t size, char *out, size_t max)
{
	size_t i = 0;

	for (uint32_t off = 6; off + 1 < size && i + 1 < max; off += 2) {
		uint16_t c = opt[off] | opt[off + 1] << 8;
		if (c == 0)
			break;
		out[i++] = c < 0x80 ? (char)c : '?';
	}
	out[i] = 0;
}

static int list(void)
{
	static struct pd_efivar_request order, entry;
	char name[16], desc[128];

	if (get_var("BootOrder", &order) != KERN_SUCCESS) {
		printf("no BootOrder\n");
		order.size = 0;
	}
	for (uint32_t i = 0; i + 1 < order.size; i += 2) {
		uint16_t n = order.data[i] | order.data[i + 1] << 8;
		snprintf(name, sizeof(name), "Boot%04X", n);
		if (get_var(name, &entry) != KERN_SUCCESS) {
			printf("%s  (missing)\n", name);
			continue;
		}
		describe(entry.data, entry.size, desc, sizeof(desc));
		printf("%s%s  %s\n", name, (entry.data[0] & kActive) ? "*" : " ", desc);
	}
	return 0;
}

// the partition's number, start, size and unique GUID, read from the disk's own GPT
static int esp_location(const char *disk, unsigned part, uint64_t *start, uint64_t *size, uint8_t guid[16])
{
	char path[64];
	uint32_t bs = 512;
	uint8_t *hdr, *ent;
	int fd;

	snprintf(path, sizeof(path), "/dev/r%s", disk);
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror(path);
		return -1;
	}
	ioctl(fd, DKIOCGETBLOCKSIZE, &bs);
	hdr = malloc(bs);
	if (!hdr || pread(fd, hdr, bs, bs) != (ssize_t)bs || memcmp(hdr, "EFI PART", 8)) {
		fprintf(stderr, "pdbless: %s has no GPT\n", disk);
		close(fd);
		return -1;
	}
	uint64_t elba = *(uint64_t *)(hdr + 72);
	uint32_t count = *(uint32_t *)(hdr + 80), esize = *(uint32_t *)(hdr + 84);
	if (part == 0 || part > count || esize < 128) {
		fprintf(stderr, "pdbless: %s has no partition %u\n", disk, part);
		close(fd);
		return -1;
	}
	// a raw disk only reads whole blocks, so read the block holding the entry
	uint64_t off = elba * bs + (uint64_t)(part - 1) * esize;
	uint32_t in = (uint32_t)(off % bs), len = (in + esize + bs - 1) / bs * bs;
	ent = malloc(len);
	if (!ent || pread(fd, ent, len, (off_t)(off - in)) != (ssize_t)len) {
		perror(path);
		free(ent);
		free(hdr);
		close(fd);
		return -1;
	}
	memcpy(guid, ent + in + 16, 16);
	*start = *(uint64_t *)(ent + in + 32);
	*size = *(uint64_t *)(ent + in + 40) - *start + 1;
	free(ent);
	free(hdr);
	close(fd);
	return 0;
}

// EFI_LOAD_OPTION: attributes, path list length, description, then HD() / File() / End
static uint32_t make_option(uint8_t *buf, const char *label, unsigned part, uint64_t start, uint64_t size,
                            const uint8_t guid[16], const char *loader)
{
	uint32_t o = 6, path_start, i;

	*(uint32_t *)buf = kActive;
	for (i = 0; label[i]; i++, o += 2)
		buf[o] = label[i], buf[o + 1] = 0;
	buf[o] = buf[o + 1] = 0;
	o += 2;
	path_start = o;

	buf[o] = 4, buf[o + 1] = 1, buf[o + 2] = 42, buf[o + 3] = 0;
	*(uint32_t *)(buf + o + 4) = part;
	*(uint64_t *)(buf + o + 8) = start;
	*(uint64_t *)(buf + o + 16) = size;
	memcpy(buf + o + 24, guid, 16);
	buf[o + 40] = 2, buf[o + 41] = 2;        // GPT, GUID signature
	o += 42;

	uint32_t flen = 4 + (uint32_t)(strlen(loader) + 1) * 2;
	buf[o] = 4, buf[o + 1] = 4, buf[o + 2] = (uint8_t)flen, buf[o + 3] = (uint8_t)(flen >> 8);
	for (i = 0; loader[i]; i++)
		buf[o + 4 + i * 2] = loader[i] == '/' ? '\\' : loader[i], buf[o + 5 + i * 2] = 0;
	buf[o + 4 + i * 2] = buf[o + 5 + i * 2] = 0;
	o += flen;

	buf[o] = 0x7F, buf[o + 1] = 0xFF, buf[o + 2] = 4, buf[o + 3] = 0;
	o += 4;
	*(uint16_t *)(buf + 4) = (uint16_t)(o - path_start);
	return o;
}

static int add(const char *disk, unsigned part, const char *loader, const char *label)
{
	static struct pd_efivar_request order, probe;
	uint8_t opt[1024], guid[16], neworder[kPDEFIVarDataMax];
	uint64_t start, size;
	char name[16];
	unsigned n;

	if (esp_location(disk, part, &start, &size, guid))
		return 1;
	// the first free Boot#### number
	for (n = 0; n < 0x10000; n++) {
		snprintf(name, sizeof(name), "Boot%04X", n);
		if (get_var(name, &probe) == kIOReturnNotFound)
			break;
	}
	uint32_t len = make_option(opt, label, part, start, size, guid, loader);
	if (set_var(name, opt, len) != KERN_SUCCESS) {
		fprintf(stderr, "pdbless: the firmware refused %s\n", name);
		return 1;
	}
	// the new entry goes first, the rest keep their order
	if (get_var("BootOrder", &order) != KERN_SUCCESS)
		order.size = 0;
	neworder[0] = (uint8_t)n, neworder[1] = (uint8_t)(n >> 8);
	memcpy(neworder + 2, order.data, order.size);
	if (set_var("BootOrder", neworder, order.size + 2) != KERN_SUCCESS) {
		fprintf(stderr, "pdbless: added %s but could not put it first in BootOrder\n", name);
		return 1;
	}
	printf("pdbless: %s \"%s\" on %ss%u, first in BootOrder\n", name, label, disk, part);
	return 0;
}

static int remove_entry(const char *hex)
{
	static struct pd_efivar_request order;
	unsigned n = (unsigned)strtoul(hex, NULL, 16);
	uint8_t kept[kPDEFIVarDataMax];
	uint32_t k = 0;
	char name[16];

	snprintf(name, sizeof(name), "Boot%04X", n);
	if (set_var(name, NULL, 0) != KERN_SUCCESS)
		fprintf(stderr, "pdbless: %s was not there\n", name);
	if (get_var("BootOrder", &order) == KERN_SUCCESS) {
		for (uint32_t i = 0; i + 1 < order.size; i += 2) {
			if ((order.data[i] | order.data[i + 1] << 8) != n)
				kept[k++] = order.data[i], kept[k++] = order.data[i + 1];
		}
		set_var("BootOrder", kept, k);
	}
	printf("pdbless: removed %s\n", name);
	return 0;
}

static int set_order(const char *list)
{
	uint8_t order[kPDEFIVarDataMax];
	uint32_t k = 0;
	char *dup = strdup(list), *tok, *save = NULL;

	for (tok = strtok_r(dup, ",", &save); tok && k + 2 <= sizeof(order); tok = strtok_r(NULL, ",", &save)) {
		unsigned n = (unsigned)strtoul(tok, NULL, 16);
		order[k++] = (uint8_t)n, order[k++] = (uint8_t)(n >> 8);
	}
	free(dup);
	if (set_var("BootOrder", order, k) != KERN_SUCCESS) {
		fprintf(stderr, "pdbless: the firmware refused BootOrder\n");
		return 1;
	}
	return 0;
}

static void usage(void)
{
	fprintf(stderr,
	    "usage: pdbless list\n"
	    "       pdbless add <disk> <partition> <loader path> <label>\n"
	    "       pdbless remove <entry, hex>\n"
	    "       pdbless order <entry,entry,...>\n"
	    "example: pdbless add disk0 1 /EFI/PureDarwin/BOOTX64.EFI PureDarwin\n");
	exit(2);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		usage();
	if (open_service())
		return 1;
	if (!strcmp(argv[1], "list") && argc == 2)
		return list();
	if (!strcmp(argv[1], "add") && argc == 6)
		return add(argv[2], (unsigned)atoi(argv[3]), argv[4], argv[5]);
	if (!strcmp(argv[1], "remove") && argc == 3)
		return remove_entry(argv[2]);
	if (!strcmp(argv[1], "order") && argc == 3)
		return set_order(argv[2]);
	usage();
	return 2;
}
