/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#ifndef __OS_CRASHLOG_PRIVATE__
#define __OS_CRASHLOG_PRIVATE__

#include <os/base_private.h>

#if __has_include(<CrashReporterClient.h>)
#include <CrashReporterClient.h>

#if defined(__x86_64__)

#define __os_set_crash_log_cause_and_message(ac, msg) \
		({ long _ac = (long)(ac); __asm__ ( \
			"mov	%[_msg], %[_cr_msg]\n\t" \
			"mov	%[_ac], %[_cr_ac]" \
			:	[_ac] "+&a" (_ac), \
				[_cr_msg] "=m" (gCRAnnotations.message), \
				[_cr_ac] "=m" (gCRAnnotations.abort_cause) \
			:	[_msg] "r" (("" msg)) \
		); })
#define _os_set_crash_log_message(msg) \
		({ long _clbr; __asm__ ( \
			"mov	%[_msg], %[_cr_msg]" \
			:	"=&a" (_clbr), \
				[_cr_msg] "=m" (gCRAnnotations.message) \
			:	[_msg] "r" (("" msg)) \
		); })

#elif defined(__arm__)

#define __os_set_crash_log_cause_and_message_impl(msg, ac_expr, set_cause, ...) \
		({ ac_expr; __asm__( \
			"push	{r9, r10}\n\t" \
			\
			"movw	r9, :lower16:(%[_msg] - 1f - 4)\n\t" \
			"movt	r9, :upper16:(%[_msg] - 1f - 4)\n" \
		"1:\n\t" \
			"add	r9, pc\n\t" \
			\
			"movw	r10, :lower16:(3f - 2f - 4)\n\t" \
			"movt	r10, :upper16:(3f - 2f - 4)\n" \
		"2:\n\t" \
			"add	r10, pc\n\t" \
			"ldr	r10, [r10]\n\t" \
			\
			"str	r9, [r10, %[_msgo]]\n\t" \
			"mov	r9, #0\n\t" \
			"str	r9, [r10, %[_msgo] + 4]\n\t" \
			set_cause \
			"pop	{r9, r10}\n\t" \
			\
			".non_lazy_symbol_pointer\n" \
		"3:\n\t" \
			".indirect_symbol _gCRAnnotations\n\t" \
			".long 0\n\t" \
			".previous" \
			::	[_msgo] "i" (__builtin_offsetof(typeof(gCRAnnotations), message)), \
				[_msg] "i" (("" msg)), \
				## __VA_ARGS__); })

#define __os_set_crash_log_cause_and_message(ac, msg) \
		__os_set_crash_log_cause_and_message_impl(msg, \
				register long _ac asm("r8") = (long)(ac), \
				"strd	%[_ac], r9, [r10, %[_aco]]\n\t", \
				[_aco] "i" (__builtin_offsetof(typeof(gCRAnnotations), abort_cause)), \
				[_ac] "r" (_ac))
#define _os_set_crash_log_message(msg) \
			__os_set_crash_log_cause_and_message_impl(msg, (void)0, "")

#elif defined(__arm64__)

#define __os_set_crash_log_cause_and_message_impl(msg, ac_expr, set_cause, ...) \
		({ ac_expr; __asm__( \
			"stp	x20, x21, [sp, #-16]!\n\t" \
			"adrp	x20, %[_msg]@PAGE\n\t" \
			"add	x20, x20, %[_msg]@PAGEOFF\n\t" \
			"adrp	x21, %[_cr]@PAGE\n\t" \
			"add	x21, x21, %[_cr]@PAGEOFF\n\t" \
			"str	x20, [x21, %[_msgo]]\n\t" \
			set_cause \
			"ldp	x20, x21, [sp], #16" \
			::	[_cr] "i" (&gCRAnnotations), \
				[_msgo] "i" (__builtin_offsetof(typeof(gCRAnnotations), message)), \
				[_msg] "i" (("" msg)), \
				## __VA_ARGS__); })

#define __os_set_crash_log_cast_ac(ac) \
	_Generic(ac, \
			const void *: (uint64_t)(uintptr_t)(ac), \
			void *: (uint64_t)(uintptr_t)(ac), \
			default: (uint64_t)(ac))

#define __os_set_crash_log_cause_and_message(ac, msg) \
		__os_set_crash_log_cause_and_message_impl(msg, \
				register uint64_t _ac asm("x8") = __os_set_crash_log_cast_ac(ac), \
				"str	%[_ac], [x21, %[_aco]]\n\t", \
				[_aco] "i" (__builtin_offsetof(typeof(gCRAnnotations), abort_cause)), \
				[_ac] "r" (_ac))
#define _os_set_crash_log_message(msg) \
			__os_set_crash_log_cause_and_message_impl(msg, (void)0, "")

#else
#define __os_set_crash_log_cause_and_message(ac, msg) ({ \
			gCRAnnotations.abort_cause = (uint64_t)(int64_t)(ac); \
			CRSetCrashLogMessage(msg); \
		})
#define _os_set_crash_log_message(msg) CRSetCrashLogMessage(msg)
#endif

/*!
 * @macro _os_set_crash_log_cause_and_message
 *
 * @brief
 * Set an abort cause and message before likely crash.
 *
 * @discussion
 * This macro is really meant to minimize register clobbering making sure that
 * the context is minimally touched.
 *
 * - On Intel, %rax is used to store the abort cause
 * - On arm and arm64, r8/x8 is used to store the abort cause, other registers
 *   are left untouched.
 *
 * An excellent way to use this macro is for example using a wrapper such
 * as below:
 *
 * <code>
 *     OS_NOINLINE OS_NORETURN OS_COLD
 *     static void
 *     _my_type_corruption_abort(my_type_t object OS_UNUSED,
 *             my_other_type_t other OS_UNUSED, long code)
 *     {
 *         _os_set_crash_log_cause_and_message(code, "object is corrupt");
 *         __builtin_trap();
 *     }
 * </code>
 *
 * That wrapper when used:
 * - is understood as being unlikely and never inlined (OS_COLD OS_NOINLINE)
 * - captures the address of @a object as well as the one of the companion
 *   object @a other in registers that are easy to introspect in crash traces
 * - captures the abort cause / error code
 *
 * @param ac
 * The abort cause to set. If it is statically provably 0, then it's ignored.
 * If the argument type is narrower than long, then it is sign-extended to long.
 *
 * @param msg
 * The static string message to set
 */
#define _os_set_crash_log_cause_and_message(ac, msg) \
		__builtin_choose_expr(os_is_compile_time_constant(!(ac)), ({ \
				if (ac) { \
						__os_set_crash_log_cause_and_message(ac, msg); \
				} else { \
						_os_set_crash_log_message(msg); \
				} }), __os_set_crash_log_cause_and_message(ac, msg))

#define _os_set_crash_log_message_dynamic(msg) CRSetCrashLogMessage(msg)

#else

#define _os_set_crash_log_cause_and_message(ac, msg) ((void)(ac), (void)(msg))
#define _os_set_crash_log_message(msg) ((void)(msg))
#define _os_set_crash_log_message_dynamic(msg) ((void)(msg))

#endif // __has_include(<CrashReporterClient.h>)

/*!
 * @macro OS_BUG_INTERNAL
 *
 * @brief
 * Perform a register-preserving crash due to invalid library invariants.
 *
 * @param ac
 * The abort cause to set (see _os_set_crash_log_cause_and_message).
 *
 * @param lib
 * The name of the library.
 *
 * @param msg
 * The static string message to append.
 */
#define OS_BUG_INTERNAL(ac, lib, msg) ({ \
		_os_set_crash_log_cause_and_message(ac, "BUG IN " lib ": " msg); \
		os_prevent_tail_call_optimization(); \
		__builtin_trap(); \
})

/*!
 * @macro OS_BUG_CLIENT
 *
 * @brief
 * Perform a register-preserving crash due to an API misuse by a library client.
 *
 * @param ac
 * The abort cause to set (see _os_set_crash_log_cause_and_message).
 *
 * @param lib
 * The name of the library.
 *
 * @param msg
 * The static string message to append.
 */
#define OS_BUG_CLIENT(ac, lib, msg) ({ \
		_os_set_crash_log_cause_and_message(ac, "BUG IN CLIENT OF " lib ": " msg); \
		os_prevent_tail_call_optimization(); \
		__builtin_trap(); \
})

#endif // __OS_CRASHLOG_PRIVATE__
