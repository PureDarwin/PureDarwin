## This file is part of GNU M4.
##
## Copyright (C) 2006 Free Software Foundation, Inc.
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
## This file written by Eric Blake <ebb9@byu.net>

AM_CPPFLAGS = -I$(top_srcdir)/lib -I../lib
bin_PROGRAMS = m4
m4_SOURCES = m4.h m4.c builtin.c debug.c eval.c format.c freeze.c input.c \
macro.c output.c path.c symtab.c
if STACKOVF
m4_SOURCES += stackovf.c
endif
m4_LDADD = ../lib/libm4.a
