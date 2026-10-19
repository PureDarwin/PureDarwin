/*
 * Copyright 2003, Ingo Weinhold, ingo_weinhold@gmx.de.
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _KERNEL_UTILS_ORDERS_H
#define _KERNEL_UTILS_ORDERS_H

namespace KernelUtilsOrder {

// Ascending
/*!	\brief A compare function object implying and ascending order.

	The < operator must be defined on the template argument type.
*/
template<typename Value>
class Ascending {
public:
	inline int operator()(const Value &a, const Value &b) const
	{
		if (a < b)
			return -1;
		else if (b < a)
			return 1;
		return 0;
	}
};

// Descending
/*!	\brief A compare function object implying and descending order.

	The < operator must be defined on the template argument type.
*/
template<typename Value>
class Descending {
public:
	inline int operator()(const Value &a, const Value &b) const
	{
		if (a < b)
			return -1;
		else if (b < a)
			return 1;
		return 0;
	}
};

}	// namespace KernelUtilsOrder

#endif	// _KERNEL_UTILS_ORDERS_H
