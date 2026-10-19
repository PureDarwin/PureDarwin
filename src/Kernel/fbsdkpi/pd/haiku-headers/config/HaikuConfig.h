/*
 * Copyright 2009-2010, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _CONFIG_HAIKU_CONFIG_H
#define _CONFIG_HAIKU_CONFIG_H


/* Determine the architecture and define macros for some fundamental
   properties:
   __HAIKU_ARCH					- short name of the architecture (used in paths)
   __HAIKU_ARCH_ABI				- name of ABI (as in package architecture)
   __HAIKU_ARCH_<arch>			- defined to 1 for the respective architecture
   __HAIKU_ARCH_BITS			- defined to 32/64 on 32/64 bit architectures
   								  (defaults to 32)
   __HAIKU_ARCH_PHYSICAL_BITS	- defined to 32/64 on architectures with 32/64
   								  (defaults to __HAIKU_ARCH_BITS)
   __HAIKU_BIG_ENDIAN			- defined to 1 on big endian architectures
   								  (defaults to undefined)
*/
#if defined(__i386__)
#	define __HAIKU_ARCH					x86
#	if __GNUC__ == 2
#		define __HAIKU_ARCH_ABI			"x86_gcc2"
#	else
#		define __HAIKU_ARCH_ABI			"x86"
#	endif
#	define __HAIKU_ARCH_X86				1
#	define __HAIKU_ARCH_PHYSICAL_BITS	64
#elif defined(__x86_64__)
#	define __HAIKU_ARCH					x86_64
#	define __HAIKU_ARCH_ABI				"x86_64"
#	define __HAIKU_ARCH_X86_64			1
#	define __HAIKU_ARCH_BITS			64
#elif defined(__POWERPC__)
#	define __HAIKU_ARCH					ppc
#	define __HAIKU_ARCH_ABI				"ppc"
#	define __HAIKU_ARCH_PPC				1
#	define __HAIKU_ARCH_PHYSICAL_BITS	64
#	define __HAIKU_BIG_ENDIAN			1
#elif defined(__M68K__)
#	define __HAIKU_ARCH					m68k
#	define __HAIKU_ARCH_ABI				"m68k"
#	define __HAIKU_ARCH_M68K			1
#	define __HAIKU_BIG_ENDIAN			1
#elif defined(__MIPSEL__)
#	define __HAIKU_ARCH					mipsel
#	define __HAIKU_ARCH_ABI				"mipsel"
#	define __HAIKU_ARCH_MIPSEL			1
#elif defined(__ARMEL__) || defined(__arm__)
#	define __HAIKU_ARCH					arm
#	define __HAIKU_ARCH_ABI				"arm"
#	define __HAIKU_ARCH_ARM				1
#elif defined(__ARMEB__)
#	define __HAIKU_ARCH					armeb
#	define __HAIKU_ARCH_ABI				"armeb"
#	define __HAIKU_ARCH_ARM				1
#	define __HAIKU_BIG_ENDIAN			1
#elif defined(__aarch64__) || defined(__arm64__)
#	define __HAIKU_ARCH					arm64
#	define __HAIKU_ARCH_ABI				"arm64"
#	define __HAIKU_ARCH_ARM64			1
#	define __HAIKU_ARCH_BITS			64
#elif (defined(__riscv) && __riscv_xlen == 32)
#	define __HAIKU_ARCH					riscv32
#	define __HAIKU_ARCH_ABI				"riscv32"
#	define __HAIKU_ARCH_RISCV32			1
#	define __HAIKU_ARCH_BITS			32
#elif (defined(__riscv) && __riscv_xlen == 64)
#	define __HAIKU_ARCH					riscv64
#	define __HAIKU_ARCH_ABI				"riscv64"
#	define __HAIKU_ARCH_RISCV64			1
#	define __HAIKU_ARCH_BITS			64
#elif (defined(__riscv) && __riscv_xlen == 128)
#	define __HAIKU_ARCH					riscv128
#	define __HAIKU_ARCH_ABI				"riscv128"
#	define __HAIKU_ARCH_RISCV128			1
#	define __HAIKU_ARCH_BITS			128
#elif defined(__sparc64__)
#	define __HAIKU_ARCH					sparc64
#	define __HAIKU_ARCH_ABI				"sparc"
#	define __HAIKU_ARCH_SPARC			1
#	define __HAIKU_ARCH_PHYSICAL_BITS	64
#	define __HAIKU_BIG_ENDIAN			1
#	define __HAIKU_ARCH_BITS			64
#else
#	error Unsupported architecture!
#endif

/* implied properties:
   __HAIKU_ARCH_{32,64}_BIT		- defined to 1 on 32/64 bit architectures, i.e.
   								  using 32/64 bit virtual addresses
   __HAIKU_ARCH_PHYSICAL_BITS	- defined to 32/64 on architectures with 32/64
   								  bit physical addresses
   __HAIKU_ARCH_PHYSICAL_{32,64}_BIT - defined to 1 on architectures using 64
   								  bit physical addresses
   __HAIKU_BIG_ENDIAN			- defined to 1 on big endian architectures
*/

/* bitness */
#ifndef __HAIKU_ARCH_BITS
#	define __HAIKU_ARCH_BITS		32
#endif

#if __HAIKU_ARCH_BITS == 32
#	define __HAIKU_ARCH_32_BIT		1
#elif __HAIKU_ARCH_BITS == 64
#	define __HAIKU_ARCH_64_BIT		1
#else
#	error Unsupported bitness!
#endif

/* physical bitness */
#ifndef __HAIKU_ARCH_PHYSICAL_BITS
#	define __HAIKU_ARCH_PHYSICAL_BITS	__HAIKU_ARCH_BITS
#endif

#if __HAIKU_ARCH_PHYSICAL_BITS == 32
#	define __HAIKU_ARCH_PHYSICAL_32_BIT	1
#elif __HAIKU_ARCH_PHYSICAL_BITS == 64
#	define __HAIKU_ARCH_PHYSICAL_64_BIT	1
#else
#	error Unsupported physical bitness!
#endif

/* endianess */
#ifndef __HAIKU_BIG_ENDIAN
#	define	__HAIKU_LITTLE_ENDIAN	1
#endif

/* architecture specific include macros */
#define __HAIKU_ARCH_HEADER(header)					<arch/__HAIKU_ARCH/header>
#define __HAIKU_SUBDIR_ARCH_HEADER(subdir, header)	\
	<subdir/arch/__HAIKU_ARCH/header>

/* BeOS R5 binary compatibility (gcc 2 on x86) */
#if defined(__HAIKU_ARCH_X86) && __GNUC__ == 2
#	define __HAIKU_BEOS_COMPATIBLE	1
#endif

/* BeOS R5 compatible types */
#if defined(__HAIKU_ARCH_X86) && !defined(_KERNEL_MODE)
	/* TODO: This should be "#ifdef __HAIKU_BEOS_COMPATIBLE", but this will
	   break all gcc 4 C++ optional packages. I.e. switch that at a suitable
	   time.
	*/
#	define __HAIKU_BEOS_COMPATIBLE_TYPES	1
#endif


#endif	/* _CONFIG_HAIKU_CONFIG_H */
