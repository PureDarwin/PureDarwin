/*
 *	Copyright 2023, Haiku Inc. All Rights Reserved.
 *	Distributed under the terms of the MIT License.
 */

#ifndef ISA_H
#define ISA_H

#include <bus_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *	ISA scatter/gather dma support.
 */

typedef struct {
	uint32				address;					// memory address in little endian
	uint16				transfer_count;				// number of transfers -1 in little endian
	uint8				reserved;					// empty space
	uint8				flag;						// end of link flag
} isa_dma_entry;

#define B_LAST_ISA_DMA_ENTRY						0x80				// sets end of link flag
#define B_MAX_ISA_DMA_COUNT							0x10000
#define B_ISA_MODULE_NAME							"bus_managers/isa/v1"

enum {
	B_8_BIT_TRANSFER,
	B_16_BIT_TRANSFER
};

typedef struct isa_module_info {
	bus_manager_info    binfo;

	uint8				(*read_io_8)				(int mapped_io_addr);
	void				(*write_io_8)				(int mapped_io_addr, uint8 value);
	uint16				(*read_io_16)				(int mapped_io_addr);
	void				(*write_io_16)				(int mapped_io_addr, uint16 value);
	uint32				(*read_io_32)				(int mapped_io_addr);
	void				(*write_io_32)				(int mapped_io_addr, uint32 value);

	phys_addr_t			(*ram_address)				(phys_addr_t physical_address_in_system_memory);

	long				(*make_isa_dma_table) (
							const void				*buffer,			// buffer for making table
							long					buffer_size,		// size of the buffer
							ulong					num_bits,			// dma transfer size
							isa_dma_entry			*table,				// -> caller-supplied
																		// scatter/gather table
							long					num_entries			// max # of entries in table
						);
	status_t			(*start_isa_dma) (
							long					channel,			// dma channel to use
							void					*buf,				// buffer to transfer
							long					transfer_count,		// number of transfers
							uchar					mode,				// mode flags
							uchar					e_mode				// extended mode flags
						);
	long				(*start_scattered_isa_dma) (
							long					channel,			// channel number to use
							const isa_dma_entry		*table,				// physical address of
																		// scatter/gather table
							uchar					mode,				// mode flags
							uchar					emode				// extended mode flags
						);
	status_t			(*lock_isa_dma_channel)		(long channel);
	status_t			(*unlock_isa_dma_channel)	(long channel);
} isa_module_info;

#ifdef __cplusplus
}
#endif

#endif	/* ISA_H */
