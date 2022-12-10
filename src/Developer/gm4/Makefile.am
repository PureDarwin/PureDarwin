## This file is part of GNU M4.
##
## Copyright (C) 2000, 2001, 2003, 2004, 2005, 2006 Free Software
## Foundation, Inc.
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; see the file COPYING.  If not, write to
## the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
## Boston, MA 02110-1301, USA.
##
## Written by Gary V. Vaughan <gary@gnu.org>

SUBDIRS = . examples lib src doc checks
EXTRA_DIST = bootstrap c-boxes.el gendocs.sh GNUmakefile Makefile.maint \
	m4/gnulib-cache.m4
DISTCLEANFILES = stamp-h
## maintainer-clean should remove as much as possible that ./bootstrap can
## recreate.  In the m4 directory, keep only gnulib-cache.m4.
MAINTAINERCLEANFILES = COPYING INSTALL Makefile.in aclocal.m4 \
	config-h.in configure depcomp doc/fdl.texi gendocs.sh install-sh \
	lib/[a-z]* m4/[a-fh-z]* m4/g[a-mo-z]* m4/gnulib-comp.m4 \
	m4/gnulib-tool.m4 missing stamp-h.in

ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --enable-changeword
