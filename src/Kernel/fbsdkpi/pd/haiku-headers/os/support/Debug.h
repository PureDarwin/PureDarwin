/*
 * Copyright 2007 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _DEBUG_H
#define _DEBUG_H


#include <BeBuild.h>
#include <OS.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


/* Private */
#ifdef __cplusplus
extern "C" {
#endif
	extern bool _rtDebugFlag;

	bool _debugFlag(void);
	bool _setDebugFlag(bool);

#if __GNUC__
	int _debugPrintf(const char *, ...) _PRINTFLIKE(1, 2);
	int _sPrintf(const char *, ...) _PRINTFLIKE(1, 2);
#else
	int _debugPrintf(const char *, ...);
	int _sPrintf(const char *, ...);
#endif
	int _xdebugPrintf(const char *, ...);
	int _debuggerAssert(const char *, int, const char *);
#ifdef __cplusplus
}
#endif

/* Debug macros */
#if DEBUG
	#define SET_DEBUG_ENABLED(FLAG)	_setDebugFlag(FLAG)
	#define	IS_DEBUG_ENABLED()		_debugFlag()

	#define SERIAL_PRINT(ARGS)		_sPrintf ARGS
	#define PRINT(ARGS) 			_debugPrintf ARGS
	#define PRINT_OBJECT(OBJ)		if (_rtDebugFlag) {			\
										PRINT(("%s\t", #OBJ));	\
										(OBJ).PrintToStream(); 	\
									} ((void)0)
	#define TRACE()					_debugPrintf("File: %s, Line: %d, Thread: %" \
										B_PRId32 "\n", __FILE__, __LINE__, \
										find_thread(NULL))

	#define SERIAL_TRACE()			_sPrintf("File: %s, Line: %d, Thread: %" \
										B_PRId32 "\n", __FILE__, __LINE__, \
										find_thread(NULL))

	#define DEBUGGER(MSG)			if (_rtDebugFlag) debugger(MSG)
	#if !defined(ASSERT)
		#define ASSERT(E)			(!(E) ? _debuggerAssert(__FILE__,__LINE__, #E) \
										: (int)0)
	#endif

	#define ASSERT_WITH_MESSAGE(expr, msg) \
								(!(expr) ? _debuggerAssert( __FILE__,__LINE__, msg) \
										: (int)0)

	#define TRESPASS()			DEBUGGER("Should not be here");

	#define DEBUG_ONLY(arg)		arg

#else /* DEBUG == 0 */
	#define SET_DEBUG_ENABLED(FLAG)			(void)0
	#define	IS_DEBUG_ENABLED()				(void)0

	#define SERIAL_PRINT(ARGS)				(void)0
	#define PRINT(ARGS)						(void)0
	#define PRINT_OBJECT(OBJ)				(void)0
	#define TRACE()							(void)0
	#define SERIAL_TRACE()					(void)0

	#define DEBUGGER(MSG)					(void)0
	#if !defined(ASSERT)
		#define ASSERT(E)					(void)0
	#endif
	#define ASSERT_WITH_MESSAGE(expr, msg)	(void)0
	#define TRESPASS()						(void)0
	#define DEBUG_ONLY(x)
#endif

/* STATIC_ASSERT is a compile-time check that can be used to             */
/* verify static expressions such as: STATIC_ASSERT(sizeof(int64) == 8); */
#if __GNUC__ >= 5 && !defined(__cplusplus)
#	define STATIC_ASSERT(x) _Static_assert(x, "static assert failed!")
#elif defined(__cplusplus) && __cplusplus >= 201103L
#	define STATIC_ASSERT(x) static_assert(x, "static assert failed!")
#else
#	define STATIC_ASSERT(x)								\
		do {												\
			struct __staticAssertStruct__ {					\
				char __static_assert_failed__[2*(x) - 1];	\
			};												\
		} while (false)
#endif


#endif	/* _DEBUG_H */
