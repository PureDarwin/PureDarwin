// New abi Support -*- C++ -*-

// Copyright (C) 2000, 2001, 2003, 2004 Free Software Foundation, Inc.
//  
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.

// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING.  If not, write to
// the Free Software Foundation, 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA. 

// As a special exception, you may use this file as part of a free software
// library without restriction.  Specifically, if other files instantiate
// templates or use macros or inline functions from this file, or you compile
// this file and link it with other files to produce an executable, this
// file does not by itself cause the resulting executable to be covered by
// the GNU General Public License.  This exception does not however
// invalidate any other reasons why the executable file might be covered by
// the GNU General Public License.

// Written by Nathan Sidwell, Codesourcery LLC, <nathan@codesourcery.com>
#include <stdlib.h>
#include <dlfcn.h>

#include <cxxabi.h>
#include <new>
#include <exception>
#include "unwind-cxx.h"

typedef void __cxa_cdtor_return_type;
typedef void __cxa_vec_ctor_return_type;
typedef __cxa_cdtor_return_type (*__cxa_cdtor_type)(void *);

namespace __cxxabiv1
{
  namespace 
  {
    struct uncatch_exception 
    {
      uncatch_exception();
      ~uncatch_exception () { __cxa_begin_catch (&p->unwindHeader); }
      
      __cxa_exception* p;

    private:
      uncatch_exception&
      operator=(const uncatch_exception&);

      uncatch_exception(const uncatch_exception&);
    };

    uncatch_exception::uncatch_exception() : p(0)
    {
      __cxa_eh_globals *globals = __cxa_get_globals_fast ();

      p = globals->caughtExceptions;
      p->handlerCount -= 1;
      globals->caughtExceptions = p->nextException;
      globals->uncaughtExceptions += 1;
    }
	
	void* lazyOperatorNewArray(size_t s)
	{
		typedef void* (*proc)(size_t);
		static proc p = NULL;
		if ( p == NULL ) {
			p = (proc)dlsym(RTLD_DEFAULT, "_Znam");
			if ( p == NULL )
				p = &::malloc;
		}
		return (*p)(s);
	}

	void lazyOperatorDeleteArray(void* x)
	{
		typedef void (*proc)(void* x);
		static proc p = NULL;
		if ( p == NULL ) {
			p = (proc)dlsym(RTLD_DEFAULT, "_ZdaPv");
			if ( p == NULL )
				p = &::free;
		}
		(*p)(x);
	}
  }

  // Allocate and construct array.
  extern "C" void *
  __cxa_vec_new(size_t element_count,
		size_t element_size,
		size_t padding_size,
		__cxa_cdtor_type constructor,
		__cxa_cdtor_type destructor)
  {
    return __cxa_vec_new2(element_count, element_size, padding_size,
			   constructor, destructor,
						  &lazyOperatorNewArray, &lazyOperatorDeleteArray);
  }

  extern "C" void *
  __cxa_vec_new2(size_t element_count,
		 size_t element_size,
		 size_t padding_size,
		 __cxa_cdtor_type constructor,
		 __cxa_cdtor_type destructor,
		 void *(*alloc) (size_t),
		 void (*dealloc) (void *))
  {
    size_t size = element_count * element_size + padding_size;
    char *base = static_cast <char *> (alloc (size));
    if (!base)
      return base;

    if (padding_size)
      {
	base += padding_size;
	reinterpret_cast <size_t *> (base)[-1] = element_count;
#ifdef _GLIBCXX_ELTSIZE_IN_COOKIE
	reinterpret_cast <size_t *> (base)[-2] = element_size;
#endif
      }
    try
      {
	__cxa_vec_ctor(base, element_count, element_size,
		       constructor, destructor);
      }
    catch (...)
      {
	{
	  uncatch_exception ue;
	  dealloc(base - padding_size);
	}
	throw;
      }
    return base;
  }
  
  extern "C" void *
  __cxa_vec_new3(size_t element_count,
		 size_t element_size,
		 size_t padding_size,
		 __cxa_cdtor_type constructor,
		 __cxa_cdtor_type destructor,
		 void *(*alloc) (size_t),
		 void (*dealloc) (void *, size_t))
  {
    size_t size = element_count * element_size + padding_size;
    char *base = static_cast<char *>(alloc (size));
    if (!base)
      return base;
    
    if (padding_size)
      {
	base += padding_size;
	reinterpret_cast<size_t *>(base)[-1] = element_count;
#ifdef _GLIBCXX_ELTSIZE_IN_COOKIE
	reinterpret_cast <size_t *> (base)[-2] = element_size;
#endif
      }
    try
      {
	__cxa_vec_ctor(base, element_count, element_size,
		       constructor, destructor);
      }
    catch (...)
      {
	{
	  uncatch_exception ue;
	  dealloc(base - padding_size, size);
	}
	throw;
      }
    return base;
  }
  
  // Construct array.
  extern "C" __cxa_vec_ctor_return_type
  __cxa_vec_ctor(void *array_address,
		 size_t element_count,
		 size_t element_size,
		 __cxa_cdtor_type constructor,
		 __cxa_cdtor_type destructor)
  {
    size_t ix = 0;
    char *ptr = static_cast<char *>(array_address);
    
    try
      {
	if (constructor)
	  for (; ix != element_count; ix++, ptr += element_size)
	    constructor(ptr);
      }
    catch (...)
      {
	{
	  uncatch_exception ue;
	  __cxa_vec_cleanup(array_address, ix, element_size, destructor);
	}
	throw;
      }
    return;
  }
  
  // Construct an array by copying.
  extern "C" __cxa_vec_ctor_return_type
  __cxa_vec_cctor(void *dest_array,
		  void *src_array,
		  size_t element_count,
		  size_t element_size,
		  __cxa_cdtor_return_type (*constructor) (void *, void *),
		  __cxa_cdtor_type destructor)
  {
    size_t ix = 0;
    char *dest_ptr = static_cast<char *>(dest_array);
    char *src_ptr = static_cast<char *>(src_array);
    
    try
      {
	if (constructor)
	  for (; ix != element_count; 
	       ix++, src_ptr += element_size, dest_ptr += element_size)
	    constructor(dest_ptr, src_ptr);
      }
    catch (...)
      {
	{
	  uncatch_exception ue;
	  __cxa_vec_cleanup(dest_array, ix, element_size, destructor);
	}
	throw;
      }
    return;
  }
  
  // Destruct array.
  extern "C" void
  __cxa_vec_dtor(void *array_address,
		 size_t element_count,
		 size_t element_size,
		 __cxa_cdtor_type destructor)
  {
    if (destructor)
      {
	char *ptr = static_cast<char *>(array_address);
	size_t ix = element_count;

	ptr += element_count * element_size;

	try
	  {
	    while (ix--)
	      {
		ptr -= element_size;
		destructor(ptr);
	      }
	  }
	catch (...)
	  {
	    {
	      uncatch_exception ue;
	      __cxa_vec_cleanup(array_address, ix, element_size, destructor);
	    }
	    throw;
	  }
      }
  }

  // Destruct array as a result of throwing an exception.
  // [except.ctor]/3 If a destructor called during stack unwinding
  // exits with an exception, terminate is called.
  extern "C" void
  __cxa_vec_cleanup(void *array_address,
		    size_t element_count,
		    size_t element_size,
		    __cxa_cdtor_type destructor)
  {
    if (destructor)
      {
	char *ptr = static_cast <char *> (array_address);
	size_t ix = element_count;

	ptr += element_count * element_size;

	try
	  {
	    while (ix--)
	      {
		ptr -= element_size;
		destructor(ptr);
	      }
	  }
	catch (...)
	  {
	    std::terminate();
	  }
      }
  }

  // Destruct and release array.
  extern "C" void
  __cxa_vec_delete(void *array_address,
		   size_t element_size,
		   size_t padding_size,
		   __cxa_cdtor_type destructor)
  {
    __cxa_vec_delete2(array_address, element_size, padding_size,
		       destructor,
					  &lazyOperatorDeleteArray);
  }

  extern "C" void
  __cxa_vec_delete2(void *array_address,
		    size_t element_size,
		    size_t padding_size,
		    __cxa_cdtor_type destructor,
		    void (*dealloc) (void *))
  {
    if (!array_address)
      return;

    char* base = static_cast<char *>(array_address);
  
    if (padding_size)
      {
	size_t element_count = reinterpret_cast<size_t *>(base)[-1];
	base -= padding_size;
	try
	  {
	    __cxa_vec_dtor(array_address, element_count, element_size,
			   destructor);
	  }
	catch (...)
	  {
	    {
	      uncatch_exception ue;
	      dealloc(base);
	    }
	    throw;
	  }
      }
    dealloc(base);
  }

  extern "C" void
  __cxa_vec_delete3(void *array_address,
		    size_t element_size,
		    size_t padding_size,
		     __cxa_cdtor_type destructor,
		    void (*dealloc) (void *, size_t))
  {
    if (!array_address)
      return;

    char* base = static_cast <char *> (array_address);
    size_t size = 0;

    if (padding_size)
      {
	size_t element_count = reinterpret_cast<size_t *> (base)[-1];
	base -= padding_size;
	size = element_count * element_size + padding_size;
	try
	  {
	    __cxa_vec_dtor(array_address, element_count, element_size,
			   destructor);
	  }
	catch (...)
	  {
	    {
	      uncatch_exception ue;
	      dealloc(base, size);
	    }
	    throw;
	  }
      }
    dealloc(base, size);
  }
} // namespace __cxxabiv1

#if defined(__arm__) && defined(__ARM_EABI__)

// The ARM C++ ABI requires that the library provide these additional
// helper functions.  There are placed in this file, despite being
// architecture-specifier, so that the compiler can inline the __cxa
// functions into these functions as appropriate.

namespace __aeabiv1
{
  extern "C" void *
  __aeabi_vec_ctor_nocookie_nodtor (void *array_address,
				    abi::__cxa_cdtor_type constructor,
				    size_t element_size,
				    size_t element_count)
  {
    return abi::__cxa_vec_ctor (array_address, element_count, element_size,
				constructor, /*destructor=*/NULL);
  }

  extern "C" void *
  __aeabi_vec_ctor_cookie_nodtor (void *array_address,
				  abi::__cxa_cdtor_type constructor,
				  size_t element_size,
				  size_t element_count)
  {
    if (array_address == NULL)
      return NULL;

    array_address = reinterpret_cast<size_t *>(array_address) + 2;
    reinterpret_cast<size_t *>(array_address)[-2] = element_size;
    reinterpret_cast<size_t *>(array_address)[-1] = element_count;
    return abi::__cxa_vec_ctor (array_address,
				element_count, element_size, 
				constructor, /*destructor=*/NULL);
  }
  
  extern "C" void *
  __aeabi_vec_cctor_nocookie_nodtor (void *dest_array,
				     void *src_array, 
				     size_t element_size, 
				     size_t element_count,
				     void *(*constructor) (void *, void *))
  {
    return abi::__cxa_vec_cctor (dest_array, src_array,
				 element_count, element_size,
				 constructor, NULL);
  }

  extern "C" void *
  __aeabi_vec_new_cookie_noctor (size_t element_size, 
				 size_t element_count)
  {
    return abi::__cxa_vec_new(element_count, element_size, 
			      2 * sizeof (size_t),
			      /*constructor=*/NULL, /*destructor=*/NULL);
  }

  extern "C" void *
  __aeabi_vec_new_nocookie (size_t element_size, 
			    size_t element_count,
			    abi::__cxa_cdtor_type constructor)
  {
    return abi::__cxa_vec_new (element_count, element_size, 0, constructor, 
			       NULL);
  }

  extern "C" void *
  __aeabi_vec_new_cookie_nodtor (size_t element_size, 
				 size_t element_count,
				 abi::__cxa_cdtor_type constructor)
  {
    return abi::__cxa_vec_new(element_count, element_size, 
			      2 * sizeof (size_t),
			      constructor, NULL);
  }

  extern "C" void *
  __aeabi_vec_new_cookie(size_t element_size, 
			 size_t element_count,
			 abi::__cxa_cdtor_type constructor,
			 abi::__cxa_cdtor_type destructor)
  {
    return abi::__cxa_vec_new (element_count, element_size, 
			       2 * sizeof (size_t),
			       constructor, destructor);
  }

  
  extern "C" void *
  __aeabi_vec_dtor (void *array_address, 
		    abi::__cxa_cdtor_type destructor,
		    size_t element_size, 
		    size_t element_count)
  {
    abi::__cxa_vec_dtor (array_address, element_count, element_size, 
			 destructor);
    return reinterpret_cast<size_t*> (array_address) - 2;
  }

  extern "C" void *
  __aeabi_vec_dtor_cookie (void *array_address, 
			   abi::__cxa_cdtor_type destructor)
  {
    abi::__cxa_vec_dtor (array_address, 
			 reinterpret_cast<size_t *>(array_address)[-1],
			 reinterpret_cast<size_t *>(array_address)[-2],
			 destructor);
    return reinterpret_cast<size_t*> (array_address) - 2;
  }
  
  
  extern "C" void
  __aeabi_vec_delete (void *array_address, 
		      abi::__cxa_cdtor_type destructor)
  {
    abi::__cxa_vec_delete (array_address,
			   reinterpret_cast<size_t *>(array_address)[-2],
			   2 * sizeof (size_t),
			   destructor);
  }

  extern "C" void
  __aeabi_vec_delete3 (void *array_address, 
		       abi::__cxa_cdtor_type destructor,
		       void (*dealloc) (void *, size_t))
  {
    abi::__cxa_vec_delete3 (array_address,
			    reinterpret_cast<size_t *>(array_address)[-2],
			    2 * sizeof (size_t),
			    destructor, dealloc);
  }

  extern "C" void
  __aeabi_vec_delete3_nodtor (void *array_address,
			      void (*dealloc) (void *, size_t))
  {
    abi::__cxa_vec_delete3 (array_address,
			    reinterpret_cast<size_t *>(array_address)[-2],
			    2 * sizeof (size_t),
			    /*destructor=*/NULL, dealloc);
  }
  
  extern "C" int
  __aeabi_atexit (void *object, 
		  void (*destructor) (void *),
		  void *dso_handle)
  {
    return abi::__cxa_atexit(destructor, object, dso_handle);
  }
} // namespace __aeabiv1

#endif // defined(__arm__) && defined(__ARM_EABI__)
