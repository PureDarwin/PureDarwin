## Process this file with automake to produce Makefile.in -*-Makefile-*-
## Copyright (C) 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
## 02110-1301  USA

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = build-aux po runtime-po lib data src doc examples tests

# Files installed for use by Automake.
aclocaldir = @aclocaldir@
aclocal_DATA = m4/bison-i18n.m4

EXTRA_DIST = GNUmakefile Makefile.cfg Makefile.maint \
  OChangeLog PACKAGING \
  djgpp/Makefile.maint djgpp/README.in djgpp/config.bat \
  djgpp/config.sed djgpp/config.site djgpp/config_h.sed \
  djgpp/subpipe.c djgpp/subpipe.h djgpp/djunpack.bat \
  djgpp/fnchange.lst


.PHONY: maintainer-check
maintainer-check:
	cd tests && $(MAKE) $(AM_MAKEFLAGS) $@
