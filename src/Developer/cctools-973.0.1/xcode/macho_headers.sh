#!/bin/sh
#
# Install extra content along with the libmacho headers.
#
# This used to include the architecture-specific mach-o headers. Currently,
# it is (or will be) only used to install the mach-o module.

install_files() {
  src_dir=$1; shift
  dst_dir=$1; shift
  files=$*

  (
    cd ${src_dir}
    mkdir -p ${dst_dir}
    install -c -m 444 ${files} ${dst_dir}
  )
}

# The following logic has been removed as part of the following:
#   <rdar://problem/59894566> cctools_ofiles does not install <mach-o/ldsyms.h> during installhdrs
# This script remains, as we'll need to reenable the module workflow that
# follows below.
#
# src=${SRCROOT}/include/mach-o
# dst=${DSTROOT}/usr/include/mach-o
#
# install_files ${src} ${dst} ldsyms.h nlist.h
# install_files ${src}/arm ${dst}/arm reloc.h
# install_files ${src}/arm64 ${dst}/arm64 reloc.h
# install_files ${src}/i386 ${dst}/i386 swap.h
# install_files ${src}/x86_64 ${dst}/x86_64 reloc.h
#
# dst=${DSTROOT}/usr/local/include/mach-o
#
# install_files ${src}/hppa ${dst}/hppa reloc.h swap.h
# install_files ${src}/i860 ${dst}/i860 reloc.h swap.h
# install_files ${src}/m68k ${dst}/m68k swap.h
# install_files ${src}/m88k ${dst}/m88k reloc.h swap.h
# install_files ${src}/sparc ${dst}/sparc reloc.h swap.h
# install_files ${src}/ppc ${dst}/ppc reloc.h swap.h
#
# src=${SRCROOT}/include/stuff
# dst=${DSTROOT}/usr/local/include/dyld
#
# install_files ${src} ${dst} bool.h

# install the /usr/include/mach-o module map
#
# This code was disabled due to
#   <rdar://problem/58579774> Cannot build against iOSHostAdditions SDK : Redefinition of module MachO
# The problem is clang does not support modules for sparse SDKs, as tracked
# here:
#   <rdar://problem/58622988> Clang module redefinition error when module exists in multiple SDK
#
# src=${SRCROOT}/include/modules
# dst=${DSTROOT}/usr/include/mach-o
# install -c -m 444 ${src}/mach-o.modulemap ${dst}/module.modulemap
