#ifndef _MALLOC_FREE_ALLOCATOR_H_
#define _MALLOC_FREE_ALLOCATOR_H_

#include <util/Constructor.h>

#include <malloc.h>

template <class DataType>
class MallocFreeAllocator : public Constructor<DataType> {
public:
	typedef DataType* Pointer;
	typedef const DataType* ConstPointer;
	typedef DataType& Reference;
	typedef const DataType& ConstReference;

	/*! malloc()'s an object of type \c DataType and returns a
		pointer to it.
	*/
	Pointer Allocate() {
		return reinterpret_cast<Pointer>(malloc(sizeof(DataType)));
	}

	/*! free()'s the given object.
	*/
	void Deallocate(Pointer object) {
		free(object);
	}
};

#endif	// _MALLOC_FREE_ALLOCATOR_H_
