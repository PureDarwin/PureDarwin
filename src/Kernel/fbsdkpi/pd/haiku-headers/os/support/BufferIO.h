/*
 * Copyright 2007-2010 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _BUFFER_IO_H
#define _BUFFER_IO_H


#include <DataIO.h>


class BBufferIO : public BPositionIO {
public:
								BBufferIO(BPositionIO* stream,
									size_t bufferSize = 65536L,
									bool ownsStream = true);
	virtual						~BBufferIO();

	// BPositionIO interface
	virtual	ssize_t				ReadAt(off_t pos, void* buffer, size_t size);
	virtual	ssize_t				WriteAt(off_t pos, const void* buffer,
									size_t size);
	virtual	off_t				Seek(off_t position, uint32 seekMode);
	virtual	off_t				Position() const;
	virtual	status_t			SetSize(off_t size);
	virtual	status_t			Flush();

	// BBufferIO interface
			BPositionIO*		Stream() const;
			size_t				BufferSize() const;
			bool				OwnsStream() const;
			void				SetOwnsStream(bool ownsStream);

			void				PrintToStream() const;

private:
	virtual	status_t			_Reserved_BufferIO_0(void*);
	virtual	status_t			_Reserved_BufferIO_1(void*);
	virtual	status_t			_Reserved_BufferIO_2(void*);
	virtual	status_t			_Reserved_BufferIO_3(void*);
	virtual	status_t			_Reserved_BufferIO_4(void*);

private:
			off_t				fBufferStart;
			BPositionIO*		fStream;
			char*				fBuffer;
			size_t				fBufferSize;
			size_t				fBufferUsed;
			off_t				fPosition;

			uint32				_reserved_ints[4];

			bool				fBufferIsDirty;
			bool				fOwnsStream;

			bool				_reserved_bools[6];
};


#endif	// _BUFFER_IO_H
