#ifndef _CONSTRUCTOR_H_
#define _CONSTRUCTOR_H_

#include <util/kernel_cpp.h>

template <class DataType>
class Constructor {
public:
	typedef DataType* Pointer;
	typedef const DataType* ConstPointer;
	typedef DataType& Reference;
	typedef const DataType& ConstReference;

	/*! Constructs the object pointed to by \a object via a
		zero-parameter constructor.
	*/
	inline
	void Construct(Pointer object) {
		if (object)
			new(reinterpret_cast<void*>(object)) DataType();
	}

	/*! Constructs the object pointed to by \a object via a
		one-parameter constructor using the given argument.
	*/
	template <typename ArgType1>
	inline
	void Construct(Pointer object, ArgType1 arg1) {
		if (object)
			new(reinterpret_cast<void*>(object)) DataType(arg1);
	}

	/*! Constructs the object pointed to by \a object via a
		two-parameter constructor using the given arguments.
	*/
	template <typename ArgType1, typename ArgType2>
	inline
	void Construct(Pointer object, ArgType1 arg1, ArgType2 arg2) {
		if (object)
			new(reinterpret_cast<void*>(object)) DataType(arg1, arg2);
	}

	/*! Constructs the object pointed to by \a object via a
		three-parameter constructor using the given arguments.
	*/
	template <typename ArgType1, typename ArgType2, typename ArgType3>
	inline
	void Construct(Pointer object, ArgType1 arg1, ArgType2 arg2, ArgType3 arg3) {
		if (object)
			new(reinterpret_cast<void*>(object)) DataType(arg1, arg2, arg3);
	}

	/*! Constructs the object pointed to by \a object via a
		four-parameter constructor using the given arguments.
	*/
	template <typename ArgType1, typename ArgType2, typename ArgType3,
	          typename ArgType4>
	inline
	void Construct(Pointer object, ArgType1 arg1, ArgType2 arg2, ArgType3 arg3,
	               ArgType4 arg4) {
		if (object)
			new(reinterpret_cast<void*>(object)) DataType(arg1, arg2, arg3, arg4);
	}

	/*! Calls the destructor for the object pointed to be \a object.
	*/
	inline
	void Destruct(Pointer object) {
		if (object)
			object->~DataType();
	}


};

#endif	// _CONSTRUCTOR_H_
