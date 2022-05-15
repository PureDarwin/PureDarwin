#
## B & I Makefile for gcc_select
#
# Copyright Apple Inc. 2002, 2003, 2007, 2009, 2011

#------------------------------------------------------------------------------#

SHIMSROOT = $(SRCROOT)/clang-shims
DEVELOPER_DIR ?= /
DEV_DIR = $(DEVELOPER_DIR)
DT_TOOLCHAIN_DIR ?= $(DEVELOPER_DIR)
DEST_DIR = $(DSTROOT)$(DT_TOOLCHAIN_DIR)
GCC_LIBDIR = $(DSTROOT)$(DEV_DIR)/usr/lib/llvm-gcc/4.2.1
CLTOOLS_DIR = /Library/Developer/CommandLineTools

.PHONY: all install install-toolchain install-developer-dir \
	installhdrs installdoc clean installsym

all: install

$(OBJROOT)/c89.o : $(SHIMSROOT)/c89.c
	$(CC) -c $^ -Wall -Os -g $(RC_CFLAGS) -o $@

$(OBJROOT)/c99.o : $(SHIMSROOT)/c99.c
	$(CC) -c $^ -Wall -Werror -Os -g $(RC_CFLAGS) -o $@

$(OBJROOT)/ld.o : $(SHIMSROOT)/ld.c
	$(CC) -c $^ -Wall -Werror -Os -g $(RC_CFLAGS) -o $@

$(OBJROOT)/gcc.o : $(SHIMSROOT)/gcc.c
	$(CC) -c $^ -Wall -Werror -Os -g $(RC_CFLAGS) -o $@

$(OBJROOT)/libgcc.o : $(SHIMSROOT)/libgcc.c
	$(CC) -c $^ -arch x86_64 -arch i386 -o $@

$(OBJROOT)/libgcc.a: $(OBJROOT)/libgcc.o
	rm -f $@
	ar cru $@ $^
	ranlib $@

% : %.o
	$(CC) $^ -g $(RC_CFLAGS) -o $@

%.dSYM : %
	dsymutil $^

install: install-toolchain install-developer-dir $(OBJROOT)/ld installsym
	install -s -c -m 555 $(OBJROOT)/ld $(DSTROOT)$(DEV_DIR)/usr/bin/ld
	$(MAKE) DT_TOOLCHAIN_DIR=$(CLTOOLS_DIR) install-toolchain
	$(MAKE) DEV_DIR=$(CLTOOLS_DIR) install-developer-dir

install-toolchain: installdoc $(OBJROOT)/c99 $(OBJROOT)/c89
	mkdir -p $(DEST_DIR)/usr/bin
	install -s -c -m 555 $(OBJROOT)/c99 $(DEST_DIR)/usr/bin/c99
	install -s -c -m 555 $(OBJROOT)/c89 $(DEST_DIR)/usr/bin/c89
	install -c -m 555 $(SHIMSROOT)/cpp $(DEST_DIR)/usr/bin/cpp

install-developer-dir: $(OBJROOT)/gcc $(OBJROOT)/libgcc.a
	mkdir -p $(DSTROOT)$(DEV_DIR)/usr/bin
	install -s -c -m 555 $(OBJROOT)/gcc $(DSTROOT)$(DEV_DIR)/usr/bin/gcc
	ln -s gcc $(DSTROOT)$(DEV_DIR)/usr/bin/g++
	mkdir -p $(GCC_LIBDIR)/include
	for f in $(SHIMSROOT)/gcc-headers/*; do \
	  install -c -m 444 $$f $(GCC_LIBDIR)/include; \
	done
	install -c -m 444 $(OBJROOT)/libgcc.a $(GCC_LIBDIR)

installsym: $(OBJROOT)/c99.dSYM $(OBJROOT)/c89.dSYM $(OBJROOT)/gcc.dSYM $(OBJROOT)/ld.dSYM
	cp -rp $^ $(SYMROOT)

installdoc:
	mkdir -p $(DEST_DIR)/usr/share/man/man1
	install -c -m 444 $(SHIMSROOT)/c99.1 $(DEST_DIR)/usr/share/man/man1/c99.1
	install -c -m 444 $(SHIMSROOT)/c89.1 $(DEST_DIR)/usr/share/man/man1/c89.1

installhdrs:

clean:
	rm -rf $(OBJROOT)/c[89]9{,.dSYM}
