
#include "unwind-cxx.h"
#include <stdio.h>
#include <unistd.h>
#include <cxxabi.h>

using namespace __cxxabiv1;


void __cxxabiapple::__cxa_increment_exception_refcount(void* p) throw()
{
	if (p != NULL ) {
		__cxa_exception* header = static_cast<__cxa_exception*>(p) - 1;
		//fprintf(stderr, "[%d] __cxa_increment_exception_refcount(%p)\n", getpid(), header);
		__sync_add_and_fetch(&header->referenceCount, 1);
	}
}

void __cxxabiapple::__cxa_decrement_exception_refcount(void* p) throw()
{
	if (p != NULL ) {
		__cxa_exception* header = static_cast<__cxa_exception*>(p) - 1;
		//fprintf(stderr, "[%d] __cxa_decrement_exception_refcount(%p)\n", getpid(), header);
		if (__sync_sub_and_fetch(&header->referenceCount, 1) == 0)
			_Unwind_DeleteException(&header->unwindHeader);
	}
}

static void
__dependent_exception_cleanup(_Unwind_Reason_Code code, _Unwind_Exception *exc)
{
    using namespace __cxxabiv1;
    __cxa_dependent_exception* deh =
                reinterpret_cast<__cxa_dependent_exception*>(exc + 1) - 1;

    // If we haven't been caught by a foreign handler, then this is
    // some sort of unwind error.  In that case just die immediately.
    // _Unwind_DeleteException in the HP-UX IA64 libunwind library
    //  returns _URC_NO_REASON and not _URC_FOREIGN_EXCEPTION_CAUGHT
    // like the GCC _Unwind_DeleteException function does.
    if (code != _URC_FOREIGN_EXCEPTION_CAUGHT && code != _URC_NO_REASON)
        __terminate(deh->terminateHandler);
   __cxxabiapple::__cxa_decrement_exception_refcount(deh->primaryException);
    __cxa_free_dependent_exception(deh);
}


void __cxxabiapple::__cxa_rethrow_primary_exception(void* thrown_exception)
{
	//fprintf(stderr, "[%d] __cxa_rethrow_primary_exception()\n", getpid());
	if ( thrown_exception != NULL ) {
		__cxa_exception* header = static_cast<__cxa_exception*>(thrown_exception) - 1;
		__cxa_dependent_exception* deh =
			(__cxa_dependent_exception*)__cxa_allocate_dependent_exception();
		deh->primaryException = header + 1;
		__cxa_increment_exception_refcount(thrown_exception);
		deh->exceptionType = header->exceptionType;
		deh->unexpectedHandler = __cxxabiapple::__cxa_unexpected_handler;
		deh->terminateHandler = __cxxabiapple::__cxa_terminate_handler;
		__GXX_INIT_EXCEPTION_CLASS(deh->unwindHeader.exception_class);
		deh->unwindHeader.exception_class += 1;
		deh->unwindHeader.exception_cleanup = __dependent_exception_cleanup;
	#if __arm__
		_Unwind_SjLj_RaiseException(&deh->unwindHeader);
	#else
		_Unwind_RaiseException(&deh->unwindHeader);
	#endif
		// Some sort of unwinding error.  Note that terminate is a handler.
		__cxa_begin_catch(&deh->unwindHeader);
	}
}

void* __cxxabiapple::__cxa_current_primary_exception() throw()
{
	//fprintf(stderr, "[%d] __cxa_current_primary_exception()\n", getpid());
    __cxa_eh_globals* globals = __cxa_get_globals();
    __cxa_exception* header = globals->caughtExceptions;
    if (header != 0 &&
        __is_gxx_exception_class(header->unwindHeader.exception_class))
    {
        if (header->unwindHeader.exception_class & 1)
        {
            __cxa_dependent_exception* deh =
                reinterpret_cast<__cxa_dependent_exception*>(header + 1) - 1;
            header = static_cast<__cxa_exception*>(deh->primaryException) - 1;
        }
        __cxa_increment_exception_refcount(header+1);
        return header + 1;
    }
	return NULL;
}


bool __cxxabiapple::__cxa_uncaught_exception() throw()
{
  __cxa_eh_globals* globals = __cxa_get_globals();
  return globals->uncaughtExceptions != 0;
}


