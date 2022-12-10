/*
 * Copyright (c) 2007, 2008, 2011-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

// This is the only C file compiled into Libsystem when building for i386.
// We use EXCLUDED_ and INCLUDED_SOURCE_FILES in the xcconfig to accomplish
// this.
//
// i386 is no longer a supported architecture for macOS. The kernel doesn’t run
// i386 macOS executables. However, we cannot completely remove the i386
// slice from libSystem.B.dylib. In order to support the watchOS Simulator,
// macOS includes several libraries with i386 slices. The watchOS Simulator
// uses the i386 ISA with a different  Mach-O platform and its own entire set
// of system libraries. Some of those libraries (including those from the clang/
// llvm project) verify that the compiler works by compiling and linking a
// simple executable. That would fail if Libsystem lacked an i386 slice.
// So, we will preserve a vestigial i386 slice, but no executables can actually
// use it to run.
// rdar://problem/59703537

#include <TargetConditionals.h>

#if !defined(__i386__) || !defined(TARGET_OS_OSX) || (TARGET_OS_OSX == 0)
#error "This file should not be built for this environment"
#endif // !defined(__i386__) || !defined(TARGET_OS_OSX) || (TARGET_OS_OSX == 0)

#include <sys/reason.h>

struct ProgramVars;

__attribute__((constructor))
static void
libSystem_initializer(int argc __attribute__((unused)),
              const char* argv[]  __attribute__((unused)),
              const char* envp[]  __attribute__((unused)),
              const char* apple[]  __attribute__((unused)),
              const struct ProgramVars* vars  __attribute__((unused)))
{
	abort_with_reason(OS_REASON_LIBSYSTEM, 386,
			"i386 is not supported on macOS",
			OS_REASON_FLAG_CONSISTENT_FAILURE);
}
