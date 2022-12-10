## Process this file with automake to produce Makefile.in -*-Makefile-*-
## Copyright (C) 2005, 2006 Free Software Foundation, Inc.

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

## ------------------------------------- ##
## Running the bison from this tarball.  ##
## ------------------------------------- ##

BISON = $(top_builddir)/tests/bison
BISON_IN = $(top_srcdir)/tests/bison.in

$(BISON): $(BISON_IN)
	cd $(top_builddir)/tests && $(MAKE) $(AM_MAKEFLAGS) bison

## ------------ ##
## Extracting.  ##
## ------------ ##

doc = $(top_srcdir)/doc/bison.texinfo
extexi = $(top_srcdir)/examples/extexi
# Extract in src.
$(calc_extracted): $(doc) $(extexi)
	cd $(srcdir) && \
	   $(AWK) -f ../extexi ../../doc/bison.texinfo -- \
	     calc++-parser.yy \
	     calc++-scanner.ll calc++.cc calc++-driver.hh calc++-driver.cc


## ------------------- ##
## Parser generation.  ##
## ------------------- ##

BUILT_SOURCES = $(calc_extracted) $(calc_sources_generated)
CLEANFILES = $(srcdir)/*.output *.tmp
MAINTAINERCLEANFILES = $(srcdir)/*.stamp $(BUILT_SOURCES)

# Compile the parser and save cycles.
# This code comes from "Handling Tools that Produce Many Outputs",
# from the Automake documentation.
EXTRA_DIST = $(srcdir)/calc++-parser.stamp $(srcdir)/calc++-parser.yy
# Don't depend on $(BISON) otherwise we would rebuild these files
# in srcdir, including during distcheck, which is forbidden.
$(srcdir)/calc++-parser.stamp: $(srcdir)/calc++-parser.yy $(BISON_IN)
	@rm -f calc++-parser.tmp
	@touch calc++-parser.tmp
	$(BISON) -d -ra -S lalr1.cc -o $(srcdir)/calc++-parser.cc \
	  $(srcdir)/calc++-parser.yy
	@mv -f calc++-parser.tmp $@

$(calc_sources_generated): $(srcdir)/calc++-parser.stamp
	@if test -f $@; then :; else \
	  rm -f $(srcdir)/calc++-parser.stamp && \
	  $(MAKE) $(AM_MAKEFLAGS) $(srcdir)/calc++-parser.stamp; \
	fi


## --------------------------- ##
## Building & testing calc++.  ##
## --------------------------- ##

check_PROGRAMS = calc++

calc_sources_extracted = $(srcdir)/calc++-scanner.ll $(srcdir)/calc++.cc \
	$(srcdir)/calc++-driver.hh $(srcdir)/calc++-driver.cc
calc_extracted = $(calc_sources_extracted) $(srcdir)/calc++-parser.yy
calc_sources_generated = \
	$(srcdir)/stack.hh $(srcdir)/position.hh $(srcdir)/location.hh \
	$(srcdir)/calc++-parser.hh $(srcdir)/calc++-parser.cc

calc___SOURCES = $(calc_sources_extracted) $(calc_sources_generated)

TESTS = test
EXTRA_DIST += $(TESTS)
