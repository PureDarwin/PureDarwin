cmake_minimum_required(VERSION 2.8)


project(libobjc)
enable_language(ASM)

set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fno-inline ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE "-O3 ${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS "-std=gnu99 ${CMAKE_C_FLAGS}")

set(libobjc_VERSION 4.6)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fexceptions")
# Build configuration
add_definitions( -DGNUSTEP -D__OBJC_RUNTIME_INTERNAL__=1)
# Probably not needed anymore?
add_definitions( -D_XOPEN_SOURCE=700 -D__BSD_VISIBLE=1 -D_BSD_SOURCE=1)

set(libobjc_ASM_SRCS 
	block_trampolines.S
	objc_msgSend.S)
set(libobjc_OBJC_SRCS 
	NSBlocks.m
	Protocol2.m
	arc.m
	associate.m
	blocks_runtime.m
	properties.m)
set(libobjc_C_SRCS 
	abi_version.c
	alias_table.c
	block_to_imp.c
	caps.c
	category_loader.c
	class_table.c
	dtable.c
	eh_personality.c
	encoding2.c
	hash_table.c
	hooks.c
	ivar.c
	legacy_malloc.c
	loader.c
	mutation.m
	protocol.c
	runtime.c
	sarray2.c
	selector_table.c
	sendmsg2.c
	statics_loader.c
	toydispatch.c)
set(libobjc_HDRS
	objc/Availability.h
	objc/Object.h
	objc/Protocol.h
	objc/blocks_private.h
	objc/blocks_runtime.h
	objc/capabilities.h
	objc/developer.h
	objc/encoding.h
	objc/hooks.h
	objc/message.h
	objc/objc-api.h
	objc/objc-arc.h
	objc/objc-auto.h
	objc/objc.h
	objc/runtime-deprecated.h
	objc/runtime.h
	objc/slot.h
)

set(libobjcxx_CXX_SRCS objcxx_eh.cc)

# For release builds, we disable spamming the terminal with warnings about
# selector type mismatches
if (CMAKE_BUILD_TYPE STREQUAL Release)
	add_definitions(-DNO_SELECTOR_MISMATCH_WARNINGS)
else ()
	add_definitions(-DGC_DEBUG)
endif ()

set(TYPE_DEPENDENT_DISPATCH TRUE CACHE BOOL
	"Enable type-dependent dispatch")
if (TYPE_DEPENDENT_DISPATCH)
	add_definitions(-DTYPE_DEPENDENT_DISPATCH)
endif ()
set(ENABLE_TRACING FALSE CACHE BOOL
	"Enable tracing support (slower, not recommended for deployment)")
if (ENABLE_TRACING)
	add_definitions(-DWITH_TRACING=1)
endif (ENABLE_TRACING)
set(LOW_MEMORY FALSE CACHE BOOL
	"Enable low-memory profile *HIGHLY EXPERIMENTAL*")
if (LOW_MEMORY)
	add_definitions(-D__OBJC_LOW_MEMORY__)
endif ()

set(BOEHM_GC FALSE CACHE BOOL
	"Enable garbage collection support (not recommended)")
if (BOEHM_GC)
	include(FindPkgConfig)
	pkg_check_modules(GC REQUIRED bdw-gc)
	link_directories(${GC_LIBRARY_DIRS})
	# If there's a threaded version, use it
	find_library(LIBGC gc-threaded PATHS "${GC_LIBRARY_DIRS}")
	if (LIBGC)
	else ()
		find_library(LIBGC gc PATHS GC_LIBRARY_DIRS)
	endif ()
	message(STATUS "Using Boehm GC library: ${LIBGC}")
	include_directories(GC_INCLUDE_DIRS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${GC_CFLAGS}")
	set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-gc")
	set(objc_LINK_FLAGS "${objc_LINK_FLAGS} ${GC_CFLAGS}")
	add_definitions(-DENABLE_GC)
	list(APPEND libobjc_OBJC_SRCS gc_boehm.c)
else ()
	list(APPEND libobjc_OBJC_SRCS gc_none.c)
endif ()

set(LEGACY_COMPAT FALSE CACHE BOOL
	"Enable legacy compatibility features")
if (LEGACY_COMPAT)
	list(APPEND libobjc_C_SRCS legacy_malloc.c)
else ()
	add_definitions(-DNO_LEGACY)
endif ()

find_package(LLVM QUIET)
set(DEFAULT_ENABLE_LLVM ${LLVM_FOUND})
if (DEFAULT_ENABLE_LLVM)
	exec_program(llvm-config
		ARGS --version
		OUTPUT_VARIABLE LLVM_VER)
	if (LLVM_VER MATCHES ".*svnn")
		set(DEFAULT_ENABLE_LLVM FALSE)
		message(STATUS "svn version of LLVM found.")
		message(STATUS "Disabling LLVM options unless explicitly enabled.")
	elseif (LLVM_VER VERSION_GREATER 3.3)
		set(DEFAULT_ENABLE_LLVM FALSE)
		message(STATUS "Untested version of LLVM (${LLVM_VER}) found.")
		message(STATUS "Disabling LLVM options unless explicitly enabled.")
	endif()
endif()
set(LLVM_OPTS ${DEFAULT_ENABLE_LLVM} CACHE BOOL
	"Build LLVM Objective-C optimisations")
if (LLVM_OPTS)
	add_subdirectory(opts)
	message(STATUS "Found LLVM, enabling LLVM optimisations")
endif ()

set(LIBOBJC_NAME "objc" CACHE STRING 
	"Name of the Objective-C runtime library (e.g. objc2 for libobjc2)")

set(INCLUDE_DIRECTORY "objc" CACHE STRING 
	"Subdirectory of the include path to install the headers.")


if (${CMAKE_C_COMPILER_ID} MATCHES Clang*)
	set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -Wno-deprecated-objc-isa-usage -Wno-objc-root-class")
	if (${CMAKE_C_COMPILER_VERSION} VERSION_GREATER 3.1)
		set(CMAKE_OBJC_FLAGS "${CMAKE_OBJC_FLAGS} -fobjc-runtime=gnustep-1.7")
	endif ()
	if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=i586")
	endif ()
else (${CMAKE_C_COMPILER_ID} MATCHES Clang*)
	MESSAGE("WARNING: It is strongly recommended that you compile with clang")
endif (${CMAKE_C_COMPILER_ID} MATCHES Clang*)

set(INSTALL_TARGETS objc)

set_source_files_properties(
	${libobjc_OBJC_SRCS}
	PROPERTIES LANGUAGE C
	COMPILE_FLAGS "${CMAKE_OBJC_FLAGS}"
)

#
# C++ Runtime interaction
#

set(ENABLE_OBJCXX true CACHE BOOL
	"Enable support for Objective-C++")
set(FORCE_LIBOBJCXX false CACHE BOOL
	"Force building a separate Objective-C++ runtime library")
if (ENABLE_OBJCXX)
	# Try to find libcxxrt.so.  We can link to this to provide the C++ ABI
	# layer, if it exists.
	find_library(CXX_RUNTIME NAMES libcxxrt.so)
	# If it doesn't, then look for GNU libsupc++.so instead (either works,
	# they're ABI compatible).
	if (NOT CXX_RUNTIME)
		find_library(CXX_RUNTIME NAMES libsupc++.so)
	endif (NOT CXX_RUNTIME)

	# If we have a C++ ABI library, then we can produce a single libobjc that
	# works for Objective-C and Objective-C++.  If not, then we need to provide
	# a separate libobjcxx.
	if (CXX_RUNTIME)
		message(STATUS "Using ${CXX_RUNTIME} as the C++ runtime library")
		try_compile( USERUNTIME 
			"${CMAKE_BINARY_DIR}/CMake"
			"${CMAKE_SOURCE_DIR}/CMake"
			test_cxx_runtime
			CMAKE_FLAGS "-DCXX_RUNTIME=${CXX_RUNTIME}")
		message(STATUS "Is runtime useable? ${USERUNTIME}")
		if (${FORCE_LIBOBJCXX} OR NOT ${USERUNTIME})
			message(STATUS "Forcing build of stand-alone libobjcxx")
			add_library(objcxx SHARED ${libobjcxx_CXX_SRCS})
			set_target_properties(objcxx PROPERTIES
				LINKER_LANGUAGE C
				SOVERSION ${libobjc_VERSION}
				)
			target_link_libraries(objcxx ${CXX_RUNTIME})
			set(CXX_RUNTIME "")
			list(APPEND INSTALL_TARGETS objcxx)
		else ()
			set(libobjc_CXX_SRCS ${libobjcxx_CXX_SRCS})
			# We don't want to link the STL implementation (e.g. libstdc++) if
			# we have a separate C++ runtime.
			set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "")
		endif ()
	else ()
		message(STATUS "No C++ runtime library found")
		add_library(objcxx SHARED ${libobjcxx_CXX_SRCS})
		set_target_properties(objcxx PROPERTIES
			LINKER_LANGUAGE C
			SOVERSION ${libobjc_VERSION}
			)
		set(CXX_RUNTIME "")
		list(APPEND INSTALL_TARGETS objcxx)
	endif ()
endif (ENABLE_OBJCXX)



# Currently, we actually need pthreads, but we should use the platform's native
# threading implementation (we do for everything except thread-local storage)
set(CMAKE_THREAD_PREFER_PTHREAD)
include(FindThreads)
set(objc_LINK_FLAGS "${objc_LINK_FLAGS} ${CMAKE_THREAD_LIBS_INIT}")



add_library(objc SHARED ${libobjc_C_SRCS} ${libobjc_ASM_SRCS} ${libobjc_OBJC_SRCS} ${libobjc_CXX_SRCS})

set_target_properties(objc PROPERTIES
	LINKER_LANGUAGE C
	SOVERSION ${libobjc_VERSION}
	OUTPUT_NAME ${LIBOBJC_NAME}
	LINK_FLAGS "${objc_LINK_FLAGS}"
	)

set_property(TARGET PROPERTY NO_SONAME true)

set(BUILD_STATIC_LIBOBJC false CACHE BOOL
	"Build the static version of libobjc")
if (BUILD_STATIC_LIBOBJC)
	add_library(objc-static STATIC ${libobjc_C_SRCS} ${libobjc_ASM_SRCS} ${libobjc_OBJC_SRCS} ${libobjc_CXX_SRCS})
	set_target_properties(objc-static PROPERTIES
		POSITION_INDEPENDENT_CODE true
		OUTPUT_NAME ${LIBOBJC_NAME})
	list(APPEND INSTALL_TARGETS objc-static)
endif ()



# Explicitly link the C++ runtime and libgc if we are compiling with gc support.
target_link_libraries(objc ${CXX_RUNTIME})
if (LIBGC)
	target_link_libraries(objc ${LIBGC})
endif ()

# Link libdispatch if available (we'll miss symbols for toydispatch otherwise)
find_library(LIBDISPATCH dispatch)
if (LIBDISPATCH)
	target_link_libraries(objc ${LIBDISPATCH})
endif ()

# Make weak symbols work on OS X
if (APPLE)
	set(CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS
		"${CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS} -undefined dynamic_lookup")
	set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,-undefined,dynamic_lookup")
	set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -Wl,-undefined,dynamic_lookup")
endif ()

#
# Installation
#


find_program(GNUSTEP_CONFIG gnustep-config)
if (GNUSTEP_CONFIG)
	EXEC_PROGRAM(gnustep-config
		ARGS "--installation-domain-for=libobjc2"
		OUTPUT_VARIABLE DEFAULT_INSTALL_TYPE)
endif ()


# If we have GNUstep environment variables, then default to installing in the
# GNUstep local environment.
if (DEFAULT_INSTALL_TYPE)
else ()
	set(DEFAULT_INSTALL_TYPE "NONE")
endif ()

if (NOT CMAKE_INSTALL_LIBDIR)
	set(CMAKE_INSTALL_LIBDIR lib)
endif ()


set(GNUSTEP_INSTALL_TYPE ${DEFAULT_INSTALL_TYPE} CACHE STRING
	"GNUstep installation type.  Options are NONE, SYSTEM, NETWORK or LOCAL.")
if (${GNUSTEP_INSTALL_TYPE} STREQUAL "NONE")
	SET(LIB_INSTALL_PATH "${CMAKE_INSTALL_LIBDIR}" CACHE STRING
		"Subdirectory of the root prefix where libraries are installed.")
	SET(HEADER_INSTALL_PATH "include")
else ()
	EXEC_PROGRAM(gnustep-config
		ARGS "--variable=GNUSTEP_${GNUSTEP_INSTALL_TYPE}_LIBRARIES"
		OUTPUT_VARIABLE LIB_INSTALL_PATH)
	EXEC_PROGRAM(gnustep-config
		ARGS "--variable=GNUSTEP_${GNUSTEP_INSTALL_TYPE}_HEADERS"
		OUTPUT_VARIABLE HEADER_INSTALL_PATH)
endif ()
message(STATUS "GNUstep install type set to ${GNUSTEP_INSTALL_TYPE}")

install(TARGETS ${INSTALL_TARGETS}
	LIBRARY DESTINATION ${LIB_INSTALL_PATH}
	ARCHIVE DESTINATION ${LIB_INSTALL_PATH})
install(FILES ${libobjc_HDRS}
	DESTINATION "${HEADER_INSTALL_PATH}/${INCLUDE_DIRECTORY}")



set(CPACK_GENERATOR TGZ CACHE STRING
	"Installer types to generate.  Sensible options include TGZ, RPM and DEB")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "GNUstep Objective-C Runtime")
set(CPACK_PACKAGE_VENDOR "The GNUstep Project")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "7")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_CONTACT "GNUstep Developer <gnustep-dev@gnu.org>")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "CMake ${CMake_VERSION_MAJOR}.${CMake_VERSION_MINOR}")
if (UNIX)
	set(CPACK_STRIP_FILES true CACHE BOOL "Strip libraries when packaging")
endif ()
include (CPack)

# uninstall target
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)


set(TESTS TRUE CACHE BOOL
	"Enable building the tests")

if (TESTS)
	enable_testing()
	add_subdirectory(Test)
endif (TESTS)

