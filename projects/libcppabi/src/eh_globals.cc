//===------------------------- eh_globals.cpp -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "unwind-cxx.h"
#include "abort_message.h"

#ifdef HAS_THREAD_LOCAL

namespace __cxxabiv1
{

extern "C" {

static
__cxa_eh_globals*
__storage() throw()
{
    static thread_local __cxa_eh_globals eh_globals;
    return &eh_globals;
}

__cxa_eh_globals*
__cxa_get_globals_fast() throw()
{
    return __storage();
}

__cxa_eh_globals*
__cxa_get_globals() throw()
{
    return __storage();
}

}  // extern "C"

}  // __cxxabiv1

#else

#include <stdlib.h>
#include <pthread.h>

namespace __cxxabiv1
{

extern "C" {

static pthread_key_t key;

static void destruct_key_for_one_thread(void* vp) throw()
{
    // If the caughtExceptions stack isn't empty here, then the program is
    // about to terminate anyway.
    ::free(vp);
    if (pthread_setspecific(key, 0) != 0)
        abort_message("cannot zero out thread specific value for __cxa_get_globals()");
}

static void construct_key_for_all_threads() throw()
{
    if (pthread_key_create(&key, destruct_key_for_one_thread) != 0)
        abort_message("cannot create pthread key for __cxa_get_globals()");
}

static pthread_once_t flag = PTHREAD_ONCE_INIT;

__cxa_eh_globals*
__cxa_get_globals() throw()
{
    if (pthread_once(&flag, construct_key_for_all_threads) != 0)
        abort_message("cannot run pthread_once for __cxa_get_globals()");
    __cxa_eh_globals* eh_globals = (__cxa_eh_globals*)pthread_getspecific(key);
    if (eh_globals == 0)
    {   // construct key for one thread
        eh_globals = (__cxa_eh_globals*)::calloc(1, sizeof(__cxa_eh_globals));
        if (eh_globals == 0)
            abort_message("cannot allocate __cxa_eh_globals");
        if (pthread_setspecific(key, eh_globals) != 0)
            abort_message("cannot set pthread specific value in __cxa_get_globals()");
    }
    return eh_globals;
}

__cxa_eh_globals*
__cxa_get_globals_fast() throw()
{
    return (__cxa_eh_globals*)pthread_getspecific(key);
}

}  // extern "C"

}  // __cxxabiv1

#endif
