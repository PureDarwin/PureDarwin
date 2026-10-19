/*
 * Copyright 2005-2014 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DATA_IO_H
#define _DATA_IO_H


#include <SupportDefs.h>


class BDataIO {
public:
								BDataIO();
	virtual						~BDataIO();

	virtual	ssize_t				Read(void* buffer, size_t size);
	virtual	ssize_t				Write(const void* buffer, size_t size);

	virtual	status_t			Flush();

			status_t			ReadExactly(void* buffer, size_t size,
									size_t* _bytesRead = NULL);
			status_t			WriteExactly(const void* buffer, size_t size,
									size_t* _bytesWritten = NULL);

private:
								BDataIO(const BDataIO&);
			BDataIO&			operator=(const BDataIO&);

	virtual	void				_ReservedDataIO2();
	virtual	void				_ReservedDataIO3();
	virtual	void				_ReservedDataIO4();
	virtual	void				_ReservedDataIO5();
	virtual	void				_ReservedDataIO6();
	virtual	void				_ReservedDataIO7();
	virtual	void				_ReservedDataIO8();
	virtual	void				_ReservedDataIO9();
	virtual	void				_ReservedDataIO10();
	virtual	void				_ReservedDataIO11();
	virtual	void				_ReservedDataIO12();

private:
			uint32				_reserved[2];
};


class BPositionIO : public BDataIO {
public:
								BPositionIO();
	virtual						~BPositionIO();

	// BDataIO interface (implemented via ReadAt/WriteAt)
	virtual	ssize_t				Read(void* buffer, size_t size);
	virtual	ssize_t				Write(const void* buffer, size_t size);

	// BPositionIO interface
	virtual	ssize_t				ReadAt(off_t position, void* buffer,
									size_t size) = 0;
	virtual	ssize_t				WriteAt(off_t position, const void* buffer,
									size_t size) = 0;

			status_t			ReadAtExactly(off_t position, void* buffer,
									size_t size, size_t* _bytesRead = NULL);
			status_t			WriteAtExactly(off_t position,
									const void* buffer, size_t size,
									size_t* _bytesWritten = NULL);

	virtual	off_t				Seek(off_t position, uint32 seekMode) = 0;
	virtual	off_t				Position() const = 0;

	virtual	status_t			SetSize(off_t size);
	virtual	status_t			GetSize(off_t* size) const;

private:
	virtual	void				_ReservedPositionIO2();
	virtual	void				_ReservedPositionIO3();
	virtual	void				_ReservedPositionIO4();
	virtual	void				_ReservedPositionIO5();
	virtual	void				_ReservedPositionIO6();
	virtual	void				_ReservedPositionIO7();
	virtual	void				_ReservedPositionIO8();
	virtual	void				_ReservedPositionIO9();
	virtual	void				_ReservedPositionIO10();
	virtual	void				_ReservedPositionIO11();
	virtual	void				_ReservedPositionIO12();

private:
			uint32				_reserved[2];
};


class BMemoryIO : public BPositionIO {
public:
								BMemoryIO(void* data, size_t length);
								BMemoryIO(const void* data, size_t length);
	virtual						~BMemoryIO();

	virtual	ssize_t				ReadAt(off_t position, void* buffer,
									size_t size);
	virtual	ssize_t				WriteAt(off_t position, const void* buffer,
									size_t size);

	virtual	off_t				Seek(off_t position, uint32 seekMode);
	virtual off_t				Position() const;

	virtual	status_t			SetSize(off_t size);

private:
								BMemoryIO(const BMemoryIO&);
			BMemoryIO&			operator=(const BMemoryIO&);

	virtual	void				_ReservedMemoryIO1();
	virtual	void				_ReservedMemoryIO2();

private:
			bool				fReadOnly;
			char*				fBuffer;
			size_t				fLength;
			size_t				fBufferSize;
			size_t				fPosition;

			uint32				_reserved[1];
};


class BMallocIO : public BPositionIO {
public:
								BMallocIO();
	virtual						~BMallocIO();

	virtual	ssize_t				ReadAt(off_t position, void* buffer,
									size_t size);
	virtual	ssize_t				WriteAt(off_t position, const void* buffer,
									size_t size);

	virtual	off_t				Seek(off_t position, uint32 seekMode);
	virtual	off_t				Position() const;

	virtual	status_t			SetSize(off_t size);

	// BMallocIO interface
			void				SetBlockSize(size_t blockSize);

			const void*			Buffer() const;
			size_t				BufferLength() const;

private:
								BMallocIO(const BMallocIO&);
			BMallocIO&			operator=(const BMallocIO&);

	virtual	void				_ReservedMallocIO1();
	virtual void				_ReservedMallocIO2();

private:
			size_t				fBlockSize;
			size_t				fMallocSize;
			size_t				fLength;
			char*				fData;
			off_t				fPosition;

			uint32				_reserved[1];
};


#endif	// _DATA_IO_H
