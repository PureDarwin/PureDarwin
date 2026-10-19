/*
 * Copyright 2007 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _CLASS_INFO_H
#define _CLASS_INFO_H


#include <typeinfo>


// deprecated, use standard RTTI calls instead 
// TODO: maybe get rid of this header completely?

#define class_name(object) \
	((typeid(*(object))).name())
#define cast_as(object, class) \
	(dynamic_cast<class*>(object))
#define is_kind_of(object, class) \
	(dynamic_cast<class*>(object) != NULL)
#define is_instance_of(object, class) \
	(typeid(*(object)) == typeid(class))

#endif // _CLASS_INFO_H
