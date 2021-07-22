//===-------------------------- cxa_apple.cxx -----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <pthread.h>
#include <stdlib.h>

#include <exception>
#include <new>

#include "cxxabi.h"
#include "unwind-cxx.h"
#include "abort_message.h"

//
// This file implements Apple extensions to the __cxa_* C++ ABI:
//


// A safe way to call a terminate or unexpected handler
static __attribute__((noreturn))
void safe_handler_caller(void (*handler)())
{
	try {
		(*handler)();
		::abort_message("handler unexpected returned");
	} catch (...) {
		::abort_message("handler threw exception");
	}
}

// The default unexpected handler which defaults to calling terminate handler 
static void unexpected_defaults_to_terminate()
{
	safe_handler_caller(__cxxabiapple::__cxa_terminate_handler);
}

// The default terminate handler
static void default_terminate()
{
	if ( __cxxabiv1::__cxa_current_exception_type() != NULL )
		abort_message("terminate called throwing an exception");
	else
		abort_message("terminate called without an active exception");
}



/* The current installed terminate handler.  */
void (*__cxxabiapple::__cxa_terminate_handler)() = &default_terminate;


/* The current installed unexpected handler.  */
void (*__cxxabiapple::__cxa_unexpected_handler)() = &unexpected_defaults_to_terminate;


/* The current installed new handler.  */
void (*__cxxabiapple::__cxa_new_handler)() = NULL;




void __cxxabiv1::__terminate(std::terminate_handler handler)
{
	safe_handler_caller(handler);
}

void __cxxabiv1::__unexpected(std::unexpected_handler handler)
{
	(*handler)();
	safe_handler_caller(__cxxabiapple::__cxa_terminate_handler);
}


// The compiler can generate implicit calls to these functions below when 
// building this libc++abi project.  But these functions really reside
// in the higher level libstdcxx or libcxx projects.  We stub out implementations
// here.  The export lists in this project prevent these functions from
// being exported.
void std::terminate()
{
	safe_handler_caller(__cxxabiapple::__cxa_terminate_handler);
}

void __cxxabiv1::__cxa_bad_typeid()
{
	abort_message("__cxa_bad_typeid() called during dynamic cast");
}

void __cxxabiv1::__cxa_bad_cast()
{
	abort_message("__cxa_bad_cast() called");
}

void operator delete(void* x) throw()
{
	free(x);
}


