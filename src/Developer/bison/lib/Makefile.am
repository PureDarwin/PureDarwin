## Copyright (C) 2001, 2002, 2003, 2004 Free Software Foundation, Inc.

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

AM_CFLAGS = $(WARNING_CFLAGS)

BUILT_SOURCES =
EXTRA_DIST =
MOSTLYCLEANFILES =

lib_LIBRARIES = $(YACC_LIBRARY)
EXTRA_LIBRARIES = liby.a
noinst_LIBRARIES = libbison.a

liby_a_SOURCES = main.c yyerror.c

libbison_a_SOURCES = $(lib_SOURCES)
lib_SOURCES = \
  get-errno.h get-errno.c \
  subpipe.h subpipe.c \
  $(bitsets_sources) $(additional_bitsets_sources) $(timevars_sources)

# Implementation of bitsets
bitsets_sources = \
  abitset.c abitset.h bbitset.h bitset.c bitset.h bitset_stats.c	\
  bitset_stats.h bitsetv.c bitsetv.h ebitset.c ebitset.h lbitset.c	\
  lbitset.h libiberty.h vbitset.c vbitset.h

# Additional bitset operations.
additional_bitsets_sources = \
	bitsetv-print.h bitsetv-print.c

# timevars, stolen from GCC.
timevars_sources = \
	timevar.h timevar.c timevar.def

libbison_a_LIBADD = $(LIBOBJS) $(ALLOCA)
libbison_a_DEPENDENCIES = $(libbison_a_LIBADD)

include gnulib.mk
