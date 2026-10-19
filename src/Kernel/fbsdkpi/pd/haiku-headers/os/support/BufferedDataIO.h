/*
 * Copyright 2011 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BUFFERED_DATA_IO_H
#define _BUFFERED_DATA_IO_H


#include <DataIO.h>


class BBufferedDataIO : public BDataIO {
public:
								BBufferedDataIO(BDataIO& stream,
									size_t bufferSize = 65536L,
									bool ownsStream = true,
									bool partialReads = false);
	virtual						~BBufferedDataIO();

			status_t			InitCheck() const;

			BDataIO*			Stream() const;
			size_t				BufferSize() const;
			bool				OwnsStream() const;
			void				SetOwnsStream(bool ownsStream);
			status_t			Flush();

	// BDataIO interface
	virtual	ssize_t				Read(void* buffer, size_t size);
	virtual	ssize_t				Write(const void* buffer, size_t size);

private:
								BBufferedDataIO(const BBufferedDataIO& other);
									// not implemented

	virtual	status_t			_Reserved0(void*);
	virtual	status_t			_Reserved1(void*);
	virtual	status_t			_Reserved2(void*);
	virtual	status_t			_Reserved3(void*);
	virtual	status_t			_Reserved4(void*);

private:
			BDataIO&			fStream;
			uint8*				fBuffer;
			size_t				fBufferSize;
			size_t				fPosition;
			size_t				fSize;

			uint32				_reserved_ints[4];

			bool				fDirty;
			bool				fOwnsStream;
			bool				fPartialReads;

			bool				_reserved_bools[5];
};


#endif	// _BUFFERED_DATA_IO_H
