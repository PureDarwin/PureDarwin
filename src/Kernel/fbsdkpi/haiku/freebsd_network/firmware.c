/*
 * Copyright 2009-2010, Colin Günther, coling@gmx.de.
 * All Rights Reserved. Distributed under the terms of the MIT License.
 *
 */


#include <posix/sys/mman.h>

#include <compat/sys/param.h>
#include <compat/sys/firmware.h>
#include <compat/sys/haiku-module.h>

#include <stdlib.h>
#include <string.h>

#include <FindDirectory.h>
#include <StorageDefs.h>
#include <SupportDefs.h>

#include <device.h>


#define MAX_FBSD_FIRMWARE_NAME_CHARS 64
	// For strndup, beeing cautious in kernel code is a good thing.
	// NB: This constant doesn't exist in FreeBSD.


static const char*
getHaikuFirmwareName(const char* fbsdFirmwareName,
	const char* unknownFirmwareName)
{
	int i;

	if (__haiku_firmware_name_map == NULL)
		return unknownFirmwareName;

	for (i = 0; i < __haiku_firmware_parts_count; i++) {
		if (strcmp(__haiku_firmware_name_map[i][0], fbsdFirmwareName) == 0)
			return __haiku_firmware_name_map[i][1];
	}
	return unknownFirmwareName;
}


const struct firmware*
firmware_get(const char* fbsdFirmwareName)
{
	return firmware_get_flags(fbsdFirmwareName, 0);
}


const struct firmware*
firmware_get_flags(const char* fbsdFirmwareName, int flags)
{
	char*				fbsdFirmwareNameCopy = NULL;
	int					fileDescriptor = -1;
	struct firmware*	firmware = NULL;
	int32				firmwareFileSize;
	char*				firmwarePath = NULL;
	const char*			haikuFirmwareName = NULL;
	ssize_t				readCount = 0;
	directory_which		checkDirs[] = { B_SYSTEM_NONPACKAGED_DATA_DIRECTORY,
							B_SYSTEM_DATA_DIRECTORY };
	size_t				numCheckDirs
							= sizeof(checkDirs) / sizeof(checkDirs[0]);
	size_t				i = 0;

	haikuFirmwareName = getHaikuFirmwareName(fbsdFirmwareName,
		fbsdFirmwareName);

	firmwarePath = (char*)malloc(B_PATH_NAME_LENGTH);
	if (firmwarePath == NULL)
		goto cleanup;


	for (; i < numCheckDirs; i++) {
		if (find_directory(checkDirs[i], -1, false, firmwarePath,
				B_PATH_NAME_LENGTH) != B_OK) {
			continue;
		}

		strlcat(firmwarePath, "/firmware/", B_PATH_NAME_LENGTH);
		strlcat(firmwarePath, gDriverName, B_PATH_NAME_LENGTH);
		strlcat(firmwarePath, "/", B_PATH_NAME_LENGTH);
		strlcat(firmwarePath, haikuFirmwareName, B_PATH_NAME_LENGTH);

		fileDescriptor = open(firmwarePath, B_READ_ONLY);
		if (fileDescriptor >= 0)
			break;
	}

	if (fileDescriptor < 0)
		goto cleanup;

	firmwareFileSize = lseek(fileDescriptor, 0, SEEK_END);
	if (firmwareFileSize == -1)
		goto cleanup;

	lseek(fileDescriptor, 0, SEEK_SET);

	fbsdFirmwareNameCopy = strndup(fbsdFirmwareName,
		MAX_FBSD_FIRMWARE_NAME_CHARS);
	if (fbsdFirmwareNameCopy == NULL)
		goto cleanup;

	firmware = (struct firmware*)malloc(sizeof(struct firmware));
	if (firmware == NULL)
		goto cleanup;

	firmware->data = malloc(firmwareFileSize);
	if (firmware->data == NULL)
		goto cleanup;

	readCount = read(fileDescriptor, (void*)firmware->data, firmwareFileSize);
	if (readCount == -1 || readCount < firmwareFileSize) {
		free((void*)firmware->data);
		goto cleanup;
	}

	firmware->datasize = firmwareFileSize;
	firmware->name = fbsdFirmwareNameCopy;
	firmware->version = __haiku_firmware_version;

	close(fileDescriptor);
	free(firmwarePath);
	return firmware;

cleanup:
	if (firmware)
		free(firmware);
	if (fbsdFirmwareNameCopy)
		free(fbsdFirmwareNameCopy);
	if (firmwarePath)
		free(firmwarePath);
	if (fileDescriptor >= 0)
		close(fileDescriptor);
	return NULL;
}


void
firmware_put(const struct firmware* firmware, int flags)
{
	if (firmware == NULL)
		return;

	if (firmware->data)
		free((void*)firmware->data);
	if (firmware->name)
		free((void*)firmware->name);
	free((void*)firmware);
}
