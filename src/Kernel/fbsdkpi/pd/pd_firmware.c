// SPDX-License-Identifier: MIT
// firmware(9) from images linked into the driver kext, as FreeBSD modules do
// so no userland helper is needed at boot

#include <OS.h>
#include <KernelExport.h>
#include <stdlib.h>
#include <string.h>

#include <compat/sys/haiku-module.h>
#include <compat/sys/firmware.h>

#include "pd_firmware.h"
#include "pd_fbsd_os.h"

// the table is generated for each driver kext, it may be empty
extern const struct pd_firmware_image pd_firmware_images[];
extern const int pd_firmware_image_count;

static const char *
base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

static const struct pd_firmware_image *
find_image(const char *name)
{
	// drivers ask with or without a directory, match either way
	for (int i = 0; i < pd_firmware_image_count; i++) {
		const struct pd_firmware_image *image = &pd_firmware_images[i];
		if (strcmp(image->name, name) == 0
				|| strcmp(base_name(image->name), base_name(name)) == 0)
			return image;
	}
	return NULL;
}

// marks a firmware read from the bundle, its data is freed on put
#define PD_FIRMWARE_FROM_FILE	0x80000000u

const struct firmware *
firmware_get_flags(const char *name, int flags)
{
	const struct pd_firmware_image *image = find_image(name);
	void *data = NULL;
	size_t size = 0;

	if (image == NULL) {
		int error = pdos_load_firmware(name, &data, &size);
		if (error != 0) {
			if ((flags & FIRMWARE_GET_NOWARN) == 0)
				dprintf("%s: no firmware %s in the kext or its bundle (%d)\n",
					gDriverName, name, error);
			return NULL;
		}
		dprintf("%s: firmware %s loaded, %lu bytes\n", gDriverName,
			name, (unsigned long)size);
	}

	struct firmware *firmware = malloc(sizeof(struct firmware));
	if (firmware == NULL) {
		pdos_free_firmware(data, size);
		return NULL;
	}
	firmware->name = strdup(name);
	if (image != NULL) {
		firmware->data = image->start;
		firmware->datasize = image->end - image->start;
		firmware->version = 0;
	} else {
		firmware->data = data;
		firmware->datasize = size;
		firmware->version = PD_FIRMWARE_FROM_FILE;
	}
	return firmware;
}

const struct firmware *
firmware_get(const char *name)
{
	return firmware_get_flags(name, 0);
}

void
firmware_put(const struct firmware *firmware, int flags)
{
	if (firmware == NULL)
		return;
	if (firmware->version == PD_FIRMWARE_FROM_FILE)
		pdos_free_firmware((void *)firmware->data, firmware->datasize);
	free((void *)firmware->name);
	free((void *)firmware);
}
