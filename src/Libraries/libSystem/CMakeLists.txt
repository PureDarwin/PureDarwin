add_subdirectory(libc)
add_subdirectory(libdispatch)
add_subdirectory(libplatform)
add_subdirectory(libmalloc)
add_subdirectory(libsystem_kernel)



# This is really an exercise in dependency resolution and circular dependencies...
# Effectively, there are a few "core" libraries that all depend on each other.
# To get them to build, we need to build static subsets of functionality, and
#   build the dynamic version of ex. dyld with these static subsets. Then, we
#   can build libdyld and circularly go around building things back.

# Note: not all libraries reexported by libSystem are found here, only those that
#   are exclusively libraries. Things like dyld, the system log, configd, etc.
#   are found where the tool they are built against should actually go. So, libmacho
#   sources can be found with cctools, which is placed in the Developer directory.

# syscall   (none)
# libm      (none)
# c/darwin/ <-- compiler_rt dyld syscall libm malloc platform pthread
#   coll.   --> dispatch macho asl blocks info notify xpc corecrypto trace
# malloc    <-- compiler_rt dyld syscall platform pthread
#           --> c featureflags
# pthread   <-- syscall platform dyld
#           --> macho
# platform  <-- ?dyld? syscall
# dyld      <-- CrashReporterClient
#           --> platform malloc c pthread xpc blocks syscalls dispatch compiler_rt
# blocks    <-- platform, malloc, dyld
#           --> c objc
# macho     --> compiler_rt malloc dyld c syscalls

# We should basically split each library above into two or three pieces:
# 1. public/private header target   2. static part for dyld     3. dynamic library
# Then, we can attempt to cross compile each in a big circle.

# platform, dispatch -- upward depends unknown

# CommonCrypto
# corecrypto

# Libc-1507.100.9
# libdispatch-1325.100.36
# libmalloc-374.100.5
# libplatform-273.100.5
# libpthread-486.100.11
# dyld-955


# libm-2026 (/usr/lib/system/libsystem_m.dylib)
# libSystem-1311.100.3 (/usr/lib/libSystem.B.dylib)
# libnotify-301 (/usr/lib/system/libsystem_notify.dylib)
# libinfo-554 (/usr/lib/system/libsystem_info.dylib)
# libclosure-79.1 (/usr/lib/system/libsystem_blocks.dylib)
# libunwind-201 (/usr/lib/system/libsystem_unwind.dylib)
# removefile-60 (/usr/lib/system/libremovefile.dylib)
# keymgr-31 (/usr/lib/system/libkeymgr.dylib)
# copyfile-180.100.3 (/usr/lib/system/libcopyfile.dylib)
# UserNotification-31 (/usr/lib/system/libunc.dylib)

# configd-1163.100.19.0.2 (/usr/lib/system/libsystem_configuration.dylib)
# syslog-392.100.2 (/usr/lib/system/libsystem_asl.dylib)


# dyld, libmalloc, libplatform, libdispatch, libc, libpthread

# dyld (/usr/lib/system/libdyld.dylib)

# xnu (/usr/lib/system/libsystem_kernel.dylib)
#   - (/usr/lib/system/libkxld.dylib)

# libpthread (/usr/lib/system/libsystem_pthread.dylib)

# libplatform (/usr/lib/system/libsystem_platform.dylib)

# libmalloc (/usr/lib/system/libsystem_malloc.dylib)

# Libc (/usr/lib/system/libsystem_c.dylib)
#   - (/usr/lib/system/libsystem_darwin.dylib)
#   - (/usr/lib/system/libsystem_collections.dylib)

# cctools (/usr/lib/system/libmacho.dylib)

# Currently then, we have/don't have...

# HAVE, required:
# commonCrypto
# copyfile
# dispatch
# dyld
# macho
# removefile
# system_asl
# system_blocks
# system_c
# system_collections
# system_configuration
# system_darwin
# system_info
# system_kernel
# system_m
# system_malloc
# system_platform
# system_pthread
# unwind

# UNCLEAR, required:
# corecrypto
# compiler_rt
# system_dnssd

# DONT HAVE, required:
# cache
# system_coreservices
# system_notify
# system_sandbox
# system_trace
# xpc

# HAVE, optional:
# keymgr

# DONT HAVE, required:
# quarantine
# system_containermanager
# system_featureflags
# system_networkextension
# system_product_info_filter
# system_secinit
# system_symptoms



# We can stub out at least system_featureflags, quarantine, and system_sandbox.
# Other than this, we need at least XPC
