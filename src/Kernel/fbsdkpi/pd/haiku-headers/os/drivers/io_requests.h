/*
 * Copyright 2009, Haiku Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _IO_REQUESTS_H
#define _IO_REQUESTS_H


/*! I/O request interface */


#include <SupportDefs.h>


typedef struct IORequest io_request;


#ifdef __cplusplus
extern "C" {
#endif

bool		io_request_is_write(const io_request* request);
bool		io_request_is_vip(const io_request* request);
off_t		io_request_offset(const io_request* request);
off_t		io_request_length(const io_request* request);
status_t	read_from_io_request(io_request* request, void* buffer,
				size_t size);
status_t	write_to_io_request(io_request* request, const void* buffer,
				size_t size);
void		notify_io_request(io_request* request, status_t status);

#ifdef __cplusplus
}
#endif


#endif	/* _IO_REQUESTS_H */
