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
#ifdef PD_FBSD_SHARED
// the shared layer searches every driver's table: firmware loads at interface up too, on a task queue with no
// driver attaching, and each driver's firmware names are its own
#include "pd_fbsd_driver.h"
#else
extern const struct pd_firmware_image pd_firmware_images[];
extern const int pd_firmware_image_count;
#endif

static const char *
base_name(const char *path)
{
	const char *slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

// drivers ask with or without a directory, match either way
static const struct pd_firmware_image *
find_in(const struct pd_firmware_image *images, int count, const char *name)
{
	for (int i = 0; i < count; i++) {
		const struct pd_firmware_image *image = &images[i];
		if (strcmp(image->name, name) == 0
				|| strcmp(base_name(image->name), base_name(name)) == 0)
			return image;
	}
	return NULL;
}

static const struct pd_firmware_image *
find_image(const char *name)
{
#ifdef PD_FBSD_SHARED
	const struct pd_firmware_image *image = NULL;
	struct pd_fbsd_driver *d;

	for (int i = 0; image == NULL && (d = pd_fbsd_driver_at(i)) != NULL; i++)
		image = find_in(d->firmware_images, d->firmware_count, name);
	return image;
#else
	return find_in(pd_firmware_images, pd_firmware_image_count, name);
#endif
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
