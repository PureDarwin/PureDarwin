//===----------------------- cxa_pure_virtual.cxx -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <cxxabi.h>
#include "abort_message.h"

// The compiler sets vtable slots for pure virtual (i.e. =0)
// methods to point to __cxa_pure_virtual.
void __cxxabiv1::__cxa_pure_virtual (void)
{
	::abort_message("pure virtual method called");
}




