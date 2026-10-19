/*
 * Copyright 2003-2006, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DISK_DEVICE_MANAGER_H
#define _DISK_DEVICE_MANAGER_H


#include <DiskDeviceDefs.h>
#include <Drivers.h>


#ifdef __cplusplus
extern "C" {
#endif

/** 	\brief C API partition representation
 *
 * 	Fields marked [sys] are set by the system and are not to be changed by
 * 	the disk system modules.
 */
typedef struct partition_data {
	partition_id	id;				// [sys]
	off_t			offset;
	off_t			size;
	off_t			content_size;
	uint32			block_size;
	uint32			physical_block_size;
	int32			child_count;
	int32			index;			// [sys]
	uint32			status;
	uint32			flags;
	dev_t			volume;			// [sys]
	char			*name;			// max: B_OS_NAME_LENGTH
	char			*content_name;	//
	char			*type;			//
	const char		*content_type;	// [sys]
	char			*parameters;
	char			*content_parameters;
	void			*cookie;
	void			*content_cookie;
} partition_data;

// C API disk device representation
typedef struct disk_device_data {
	partition_id	id;				// equal to that of the root partition
	uint32			flags;
	char			*path;
	device_geometry	geometry;
} disk_device_data;

// C API partitionable space representation
typedef struct partitionable_space_data {
	off_t	offset;
	off_t	size;
} partitionable_space_data;

// operations on partitions
enum {
	B_PARTITION_SHADOW,			// indicates creation of a shadow partition
	B_PARTITION_SHADOW_CHILD,	//
	B_PARTITION_DEFRAGMENT,
	B_PARTITION_REPAIR,
	B_PARTITION_RESIZE,
	B_PARTITION_RESIZE_CHILD,
	B_PARTITION_MOVE,
	B_PARTITION_MOVE_CHILD,
	B_PARTITION_SET_NAME,
	B_PARTITION_SET_CONTENT_NAME,
	B_PARTITION_SET_TYPE,
	B_PARTITION_SET_PARAMETERS,
	B_PARTITION_SET_CONTENT_PARAMETERS,
	B_PARTITION_INITIALIZE,
	B_PARTITION_CREATE_CHILD,
	B_PARTITION_DELETE_CHILD,
};

// disk device job cancel status
enum {
	B_DISK_DEVICE_JOB_CONTINUE,
	B_DISK_DEVICE_JOB_CANCEL,
	B_DISK_DEVICE_JOB_REVERSE,
};

// disk device locking
disk_device_data *write_lock_disk_device(partition_id partitionID);
void write_unlock_disk_device(partition_id partitionID);
disk_device_data *read_lock_disk_device(partition_id partitionID);
void read_unlock_disk_device(partition_id partitionID);
	// parameter is the ID of any partition on the device

// getting disk devices/partitions by path
// (no locking required)
int32 find_disk_device(const char *path);
int32 find_partition(const char *path);

// disk device/partition read access
// (read lock required)
disk_device_data *get_disk_device(partition_id partitionID);
partition_data *get_partition(partition_id partitionID);
partition_data *get_parent_partition(partition_id partitionID);
partition_data *get_child_partition(partition_id partitionID, int32 index);

int open_partition(partition_id partitionID, int openMode);

// partition write access
// (write lock required)
partition_data *create_child_partition(partition_id partitionID, int32 index,
		off_t offset, off_t size, partition_id childID);
	// childID is an optional input parameter -- -1 to be ignored
bool delete_partition(partition_id partitionID);
void partition_modified(partition_id partitionID);
	// tells the disk device manager, that the partition has been modified

status_t scan_partition(partition_id partitionID);
	// Service method for disks systems: Synchronously scans the partition.
	// Device must not be locked.

// partition support functions
// (no lock required)
status_t get_default_partition_content_name(partition_id partitionID,
		const char* fileSystemName, char* buffer, size_t bufferSize);
	// The partition_data::content_size field must already be initialized.

// disk systems
disk_system_id find_disk_system(const char *name);

// jobs
bool update_disk_device_job_progress(disk_job_id jobID, float progress);
bool update_disk_device_job_extra_progress(disk_job_id jobID, const char *info);
bool set_disk_device_job_error_message(disk_job_id jobID, const char *message);
uint32 update_disk_device_job_interrupt_properties(disk_job_id jobID,
		uint32 interruptProperties);
	// returns one of B_DISK_DEVICE_JOB_{CONTINUE,CANCEL,REVERSE}

#ifdef __cplusplus
}
#endif

#endif	// _DISK_DEVICE_MANAGER_H
