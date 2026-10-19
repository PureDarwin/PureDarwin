/*
 * Copyright 2004-2016, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FS_INTERFACE_H
#define _FS_INTERFACE_H


/*! File System Interface Layer Definition */


#include <OS.h>
#include <Select.h>
#include <module.h>
#include <disk_device_manager.h>

#include <sys/uio.h>


struct dirent;
struct stat;
struct fs_info;
struct select_sync;

typedef struct IORequest io_request;


/* additional flags passed to write_stat() (see NodeMonitor.h for the others) */
// NOTE: Changing the constants here or in NodeMonitor.h will break
// src/kits/storage/LibBeAdapter.cpp:_kern_write_stat().
#define B_STAT_SIZE_INSECURE	0x2000
	// TODO: this should be faded out once BFS supports sparse files

/* passed to write_fs_info() */
#define	FS_WRITE_FSINFO_NAME	0x0001

struct file_io_vec {
	off_t	offset;
	off_t	length;
};

#define	B_CURRENT_FS_API_VERSION "/v1"

// flags for publish_vnode() and fs_volume_ops::get_vnode()
#define B_VNODE_PUBLISH_REMOVED					0x01
#define B_VNODE_DONT_CREATE_SPECIAL_SUB_NODE	0x02


#ifdef __cplusplus
extern "C" {
#endif

typedef struct fs_volume fs_volume;
typedef struct fs_volume_ops fs_volume_ops;
typedef struct fs_vnode fs_vnode;
typedef struct fs_vnode_ops fs_vnode_ops;
typedef struct file_system_module_info file_system_module_info;

struct fs_volume {
	dev_t						id;
	partition_id				partition;
	int32						layer;
	void*						private_volume;
	fs_volume_ops*				ops;
	fs_volume*					sub_volume;
	fs_volume*					super_volume;
	file_system_module_info*	file_system;
	char*						file_system_name;
};

struct fs_vnode {
	void*			private_node;
	fs_vnode_ops*	ops;
};

struct fs_volume_ops {
	status_t (*unmount)(fs_volume* volume);

	status_t (*read_fs_info)(fs_volume* volume, struct fs_info* info);
	status_t (*write_fs_info)(fs_volume* volume, const struct fs_info* info,
				uint32 mask);
	status_t (*sync)(fs_volume* volume);

	status_t (*get_vnode)(fs_volume* volume, ino_t id, fs_vnode* vnode,
				int* _type, uint32* _flags, bool reenter);

	/* index directory & index operations */
	status_t (*open_index_dir)(fs_volume* volume, void** _cookie);
	status_t (*close_index_dir)(fs_volume* volume, void* cookie);
	status_t (*free_index_dir_cookie)(fs_volume* volume, void* cookie);
	status_t (*read_index_dir)(fs_volume* volume, void* cookie,
				struct dirent* buffer, size_t bufferSize, uint32* _num);
	status_t (*rewind_index_dir)(fs_volume* volume, void* cookie);

	status_t (*create_index)(fs_volume* volume, const char* name, uint32 type,
				uint32 flags);
	status_t (*remove_index)(fs_volume* volume, const char* name);
	status_t (*read_index_stat)(fs_volume* volume, const char* name,
				struct stat* stat);

	/* query operations */
	status_t (*open_query)(fs_volume* volume, const char* query, uint32 flags,
				port_id port, uint32 token, void** _cookie);
	status_t (*close_query)(fs_volume* volume, void* cookie);
	status_t (*free_query_cookie)(fs_volume* volume, void* cookie);
	status_t (*read_query)(fs_volume* volume, void* cookie,
				struct dirent* buffer, size_t bufferSize, uint32* _num);
	status_t (*rewind_query)(fs_volume* volume, void* cookie);

	/* support for FS layers */
	status_t (*all_layers_mounted)(fs_volume* volume);
	status_t (*create_sub_vnode)(fs_volume* volume, ino_t id, fs_vnode* vnode);
	status_t (*delete_sub_vnode)(fs_volume* volume, fs_vnode* vnode);
};

struct fs_vnode_ops {
	/* vnode operations */
	status_t (*lookup)(fs_volume* volume, fs_vnode* dir, const char* name,
				ino_t* _id);
	status_t (*get_vnode_name)(fs_volume* volume, fs_vnode* vnode, char* buffer,
				size_t bufferSize);

	status_t (*put_vnode)(fs_volume* volume, fs_vnode* vnode, bool reenter);
	status_t (*remove_vnode)(fs_volume* volume, fs_vnode* vnode, bool reenter);

	/* VM file access */
	bool (*can_page)(fs_volume* volume, fs_vnode* vnode, void* cookie);
	status_t (*read_pages)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				off_t pos, const iovec* vecs, size_t count, size_t* _numBytes);
	status_t (*write_pages)(fs_volume* volume, fs_vnode* vnode,
				void* cookie, off_t pos, const iovec* vecs, size_t count,
				size_t* _numBytes);

	/* asynchronous I/O */
	status_t (*io)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				io_request* request);
	status_t (*cancel_io)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				io_request* request);

	/* cache file access */
	status_t (*get_file_map)(fs_volume* volume, fs_vnode* vnode, off_t offset,
				size_t size, struct file_io_vec* vecs, size_t* _count);

	/* common operations */
	status_t (*ioctl)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				uint32 op, void* buffer, size_t length);
	status_t (*set_flags)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				int flags);
	status_t (*select)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				uint8 event, selectsync* sync);
	status_t (*deselect)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				uint8 event, selectsync* sync);
	status_t (*fsync)(fs_volume* volume, fs_vnode* vnode, bool dataOnly);

	status_t (*read_symlink)(fs_volume* volume, fs_vnode* link, char* buffer,
				size_t* _bufferSize);
	status_t (*create_symlink)(fs_volume* volume, fs_vnode* dir,
				const char* name, const char* path, int mode);

	status_t (*link)(fs_volume* volume, fs_vnode* dir, const char* name,
				fs_vnode* vnode);
	status_t (*unlink)(fs_volume* volume, fs_vnode* dir, const char* name);
	status_t (*rename)(fs_volume* volume, fs_vnode* fromDir,
				const char* fromName, fs_vnode* toDir, const char* toName);

	status_t (*access)(fs_volume* volume, fs_vnode* vnode, int mode);
	status_t (*read_stat)(fs_volume* volume, fs_vnode* vnode,
				struct stat* stat);
	status_t (*write_stat)(fs_volume* volume, fs_vnode* vnode,
				const struct stat* stat, uint32 statMask);
	status_t (*preallocate)(fs_volume* volume, fs_vnode* vnode,
				off_t pos, off_t length);

	/* file operations */
	status_t (*create)(fs_volume* volume, fs_vnode* dir, const char* name,
				int openMode, int perms, void** _cookie,
				ino_t* _newVnodeID);
	status_t (*open)(fs_volume* volume, fs_vnode* vnode, int openMode,
				void** _cookie);
	status_t (*close)(fs_volume* volume, fs_vnode* vnode, void* cookie);
	status_t (*free_cookie)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);
	status_t (*read)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				off_t pos, void* buffer, size_t* length);
	status_t (*write)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				off_t pos, const void* buffer, size_t* length);

	/* directory operations */
	status_t (*create_dir)(fs_volume* volume, fs_vnode* parent,
				const char* name, int perms);
	status_t (*remove_dir)(fs_volume* volume, fs_vnode* parent,
				const char* name);
	status_t (*open_dir)(fs_volume* volume, fs_vnode* vnode,
				void** _cookie);
	status_t (*close_dir)(fs_volume* volume, fs_vnode* vnode, void* cookie);
	status_t (*free_dir_cookie)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);
	status_t (*read_dir)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				struct dirent* buffer, size_t bufferSize, uint32* _num);
	status_t (*rewind_dir)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);

	/* attribute directory operations */
	status_t (*open_attr_dir)(fs_volume* volume, fs_vnode* vnode,
				void** _cookie);
	status_t (*close_attr_dir)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);
	status_t (*free_attr_dir_cookie)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);
	status_t (*read_attr_dir)(fs_volume* volume, fs_vnode* vnode,
				void* cookie, struct dirent* buffer, size_t bufferSize,
				uint32* _num);
	status_t (*rewind_attr_dir)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);

	/* attribute operations */
	status_t (*create_attr)(fs_volume* volume, fs_vnode* vnode,
				const char* name, uint32 type, int openMode,
				void** _cookie);
	status_t (*open_attr)(fs_volume* volume, fs_vnode* vnode, const char* name,
				int openMode, void** _cookie);
	status_t (*close_attr)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);
	status_t (*free_attr_cookie)(fs_volume* volume, fs_vnode* vnode,
				void* cookie);
	status_t (*read_attr)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				off_t pos, void* buffer, size_t* length);
	status_t (*write_attr)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				off_t pos, const void* buffer, size_t* length);

	status_t (*read_attr_stat)(fs_volume* volume, fs_vnode* vnode,
				void* cookie, struct stat* stat);
	status_t (*write_attr_stat)(fs_volume* volume, fs_vnode* vnode,
				void* cookie, const struct stat* stat, int statMask);
	status_t (*rename_attr)(fs_volume* volume, fs_vnode* fromVnode,
				const char* fromName, fs_vnode* toVnode, const char* toName);
	status_t (*remove_attr)(fs_volume* volume, fs_vnode* vnode,
				const char* name);

	/* support for node and FS layers */
	status_t (*create_special_node)(fs_volume* volume, fs_vnode* dir,
				const char* name, fs_vnode* subVnode, mode_t mode, uint32 flags,
				fs_vnode* _superVnode, ino_t* _nodeID);
	status_t (*get_super_vnode)(fs_volume* volume, fs_vnode* vnode,
				fs_volume* superVolume, fs_vnode* superVnode);

	/* lock operations */
	status_t (*test_lock)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				struct flock* lock);
	status_t (*acquire_lock)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				const struct flock* lock, bool wait);
	status_t (*release_lock)(fs_volume* volume, fs_vnode* vnode, void* cookie,
				const struct flock* lock);
};

struct file_system_module_info {
	struct module_info	info;
	const char*			short_name;
	const char*			pretty_name;
	uint32				flags;	// DDM flags

	/* scanning (the device is write locked) */
	float (*identify_partition)(int fd, partition_data* partition,
				void** _cookie);
	status_t (*scan_partition)(int fd, partition_data* partition,
				void* cookie);
	void (*free_identify_partition_cookie)(partition_data* partition,
				void* cookie);
	void (*free_partition_content_cookie)(partition_data* partition);

	/* general operations */
	status_t (*mount)(fs_volume* volume, const char* device, uint32 flags,
				const char* args, ino_t* _rootVnodeID);

	/* capability querying (the device is read locked) */
	uint32 (*get_supported_operations)(partition_data* partition, uint32 mask);

	bool (*validate_resize)(partition_data* partition, off_t* size);
	bool (*validate_move)(partition_data* partition, off_t* start);
	bool (*validate_set_content_name)(partition_data* partition,
				char* name);
	bool (*validate_set_content_parameters)(partition_data* partition,
				const char* parameters);
	bool (*validate_initialize)(partition_data* partition, char* name,
				const char* parameters);

	/* shadow partition modification (device is write locked) */
	status_t (*shadow_changed)(partition_data* partition,
				partition_data* child, uint32 operation);

	/* writing (the device is NOT locked) */
	status_t (*defragment)(int fd, partition_id partition,
				disk_job_id job);
	status_t (*repair)(int fd, partition_id partition, bool checkOnly,
				disk_job_id job);
	status_t (*resize)(int fd, partition_id partition, off_t size,
				disk_job_id job);
	status_t (*move)(int fd, partition_id partition, off_t offset,
				disk_job_id job);
	status_t (*set_content_name)(int fd, partition_id partition,
				const char* name, disk_job_id job);
	status_t (*set_content_parameters)(int fd, partition_id partition,
				const char* parameters, disk_job_id job);
	status_t (*initialize)(int fd, partition_id partition, const char* name,
				const char* parameters, off_t partitionSize, disk_job_id job);
	status_t (*uninitialize)(int fd, partition_id partition,
				off_t partitionSize, uint32 blockSize, disk_job_id job);
};


/* file system add-ons only prototypes */

// callbacks for do_iterative_fd_io()
typedef status_t (*iterative_io_get_vecs)(void* cookie, io_request* request,
				off_t offset, size_t size, struct file_io_vec* vecs,
				size_t* _count);
typedef status_t (*iterative_io_finished)(void* cookie, io_request* request,
				status_t status, bool partialTransfer, size_t bytesTransferred);

extern status_t new_vnode(fs_volume* volume, ino_t vnodeID, void* privateNode,
					fs_vnode_ops* ops);
extern status_t publish_vnode(fs_volume* volume, ino_t vnodeID,
					void* privateNode, fs_vnode_ops* ops, int type,
					uint32 flags);
extern status_t get_vnode(fs_volume* volume, ino_t vnodeID,
					void** _privateNode);
extern status_t put_vnode(fs_volume* volume, ino_t vnodeID);
extern status_t acquire_vnode(fs_volume* volume, ino_t vnodeID);
extern status_t remove_vnode(fs_volume* volume, ino_t vnodeID);
extern status_t unremove_vnode(fs_volume* volume, ino_t vnodeID);
extern status_t get_vnode_removed(fs_volume* volume, ino_t vnodeID,
					bool* _removed);
extern fs_volume* volume_for_vnode(fs_vnode* vnode);

extern status_t check_access_permissions(int accessMode, mode_t mode,
					gid_t nodeGroupID, uid_t nodeUserID);
extern status_t check_write_stat_permissions(gid_t nodeGroupID, uid_t nodeUserID,
					mode_t nodeMode, uint32 mask, const struct stat* stat);

extern status_t read_file_io_vec_pages(int fd,
					const struct file_io_vec* fileVecs, size_t fileVecCount,
					const struct iovec* vecs, size_t vecCount,
					uint32* _vecIndex, size_t* _vecOffset, size_t* _bytes);
extern status_t write_file_io_vec_pages(int fd,
					const struct file_io_vec* fileVecs, size_t fileVecCount,
					const struct iovec* vecs, size_t vecCount,
					uint32* _vecIndex, size_t* _vecOffset, size_t* _bytes);
extern status_t do_fd_io(int fd, io_request* request);
extern status_t do_iterative_fd_io(int fd, io_request* request,
					iterative_io_get_vecs getVecs,
					iterative_io_finished finished, void* cookie);

extern status_t notify_entry_created(dev_t device, ino_t directory,
					const char* name, ino_t node);
extern status_t notify_entry_removed(dev_t device, ino_t directory,
					const char* name, ino_t node);
extern status_t notify_entry_moved(dev_t device, ino_t fromDirectory,
					const char* fromName, ino_t toDirectory,
					const char* toName, ino_t node);
extern status_t notify_stat_changed(dev_t device, ino_t directory, ino_t node,
					uint32 statFields);
extern status_t notify_attribute_changed(dev_t device, ino_t directory,
					ino_t node, const char* attribute, int32 cause);

extern status_t notify_query_entry_created(port_id port, int32 token,
					dev_t device, ino_t directory, const char* name,
					ino_t node);
extern status_t notify_query_entry_moved(port_id port, int32 token,
					dev_t device, ino_t fromDirectory,
					const char* fromName, ino_t toDirectory,
					const char* toName, ino_t node);
extern status_t notify_query_entry_removed(port_id port, int32 token,
					dev_t device, ino_t directory, const char* name,
					ino_t node);
extern status_t notify_query_attribute_changed(port_id port, int32 token,
					dev_t device, ino_t directory, ino_t node,
					const char* attribute, int32 cause);

#ifdef __cplusplus
}
#endif

#endif	/* _FS_INTERFACE_H */
