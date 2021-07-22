// -*- C++ -*- Allocate exception objects.
// Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006
// Free Software Foundation, Inc.
//
// This file is part of GCC.
//
// GCC is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// GCC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
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

// This is derived from the C++ ABI for IA-64.  Where we diverge
// for cross-architecture compatibility are noted with "@@@".

#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include <exception>
#include "unwind-cxx.h"



using namespace __cxxabiv1;

/* APPLE LOCAL begin reduce emergency buffer size */
/* 256 bytes is more than large enough for an std::bad_alloc object */
#define EMERGENCY_OBJ_SIZE 256
#define EMERGENCY_OBJ_COUNT 2
/* APPLE LOCAL end reduce emergency buffer size */

#if INT_MAX == 32767 || EMERGENCY_OBJ_COUNT <= 32
typedef unsigned int bitmask_type;
#else
typedef unsigned long bitmask_type;
#endif

#ifndef NO_EMERGENCY_BUFFER
typedef char one_buffer[EMERGENCY_OBJ_SIZE] __attribute__((aligned));
static one_buffer emergency_buffer[EMERGENCY_OBJ_COUNT];
static bitmask_type emergency_used;
#endif

namespace
{
  // A single mutex controlling emergency allocations.
  pthread_mutex_t emergency_mutex = PTHREAD_MUTEX_INITIALIZER;
}

extern "C" void *
__cxxabiv1::__cxa_allocate_exception(size_t thrown_size) throw()
{
  void *ret;

  thrown_size += sizeof (__cxa_exception);
  ret = malloc (thrown_size);

  if (! ret)
    {
 #ifndef NO_EMERGENCY_BUFFER
     pthread_mutex_lock(&emergency_mutex);

      bitmask_type used = emergency_used;
      unsigned int which = 0;

      if (thrown_size > EMERGENCY_OBJ_SIZE)
	goto failed;
      while (used & 1)
	{
	  used >>= 1;
	  if (++which >= EMERGENCY_OBJ_COUNT)
	    goto failed;
	}

      emergency_used |= (bitmask_type)1 << which;
      ret = &emergency_buffer[which][0];

    failed:;

    pthread_mutex_unlock(&emergency_mutex);
#endif
      if (!ret)
	std::terminate ();
    }

  memset (ret, 0, sizeof (__cxa_exception));

  return (void *)((char *)ret + sizeof (__cxa_exception));
}


extern "C" void
__cxxabiv1::__cxa_free_exception(void *vptr) throw()
{
  char *ptr = (char *) vptr;
#ifndef NO_EMERGENCY_BUFFER
  if (ptr >= &emergency_buffer[0][0]
      && ptr < &emergency_buffer[0][0] + sizeof (emergency_buffer))
    {
      const unsigned int which
	= (unsigned)(ptr - &emergency_buffer[0][0]) / EMERGENCY_OBJ_SIZE;

      pthread_mutex_lock(&emergency_mutex);
      emergency_used &= ~((bitmask_type)1 << which);
      pthread_mutex_unlock(&emergency_mutex);
    }
  else
#endif
    free (ptr - sizeof (__cxa_exception));
}

extern "C" void*
__cxxabiv1::__cxa_allocate_dependent_exception() throw()
{
    size_t thrown_size = sizeof(__cxa_dependent_exception);
    void* ret = ::malloc(thrown_size);
    if (!ret)
    {
#ifndef NO_EMERGENCY_BUFFER
        pthread_mutex_lock(&emergency_mutex);
        bitmask_type used = emergency_used;
        unsigned int which = 0;
        if (thrown_size > EMERGENCY_OBJ_SIZE)
            goto failed;
        while (used & 1)
        {
            used >>= 1;
            if (++which >= EMERGENCY_OBJ_COUNT)
                goto failed;
        }
        emergency_used |= (bitmask_type)1 << which;
        ret = &emergency_buffer[which][0];
    failed:
        pthread_mutex_unlock(&emergency_mutex);
#endif
        if (!ret)
            std::terminate ();
    }
    ::memset(ret, 0, thrown_size);
    return ret;
}

extern "C" void
__cxxabiv1::__cxa_free_dependent_exception(void* vptr) throw()
{
    char* ptr = (char*)vptr;
#ifndef NO_EMERGENCY_BUFFER
    if (&emergency_buffer[0][0] <= ptr &&
        ptr < &emergency_buffer[0][0] + sizeof(emergency_buffer))
    {
        const unsigned int which = (unsigned)(ptr - &emergency_buffer[0][0])
                                   / EMERGENCY_OBJ_SIZE;
        pthread_mutex_lock(&emergency_mutex);
        emergency_used &= ~((bitmask_type)1 << which);
        pthread_mutex_unlock(&emergency_mutex);
    }
    else
#endif
        ::free(ptr);
}
