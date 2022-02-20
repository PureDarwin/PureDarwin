# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile
LD ?= ld

all:
	# build an app with duplicate local static symbols
	${CC} -arch $(ARCH) -g -o foo.o -c foo.c
	${CC} -arch $(ARCH) -g -o bar.o -c bar.c
	${CC} -arch $(ARCH) -g -o main.o -c main.c
	${LD} -lSystem -o main foo.o bar.o main.o 2>&1

	# verify the linker uniqed the local strings, and if so,
	# strip the binary and verify the strings are still unqiued
	uniq=`nm -px main | awk '{ print $$2 " " $$5 " " $$6 }' |	     \
		grep '^0e ' | uniq | wc -l | tr -d " "`;		     \
	if [ "$$uniq" = "1" ];						     \
	then								     \
	  ${STRIP} -S main;						     \
	  nm -px main | awk '{ print $$2 " " $$5 " " $$6 }' | grep '^0e ' |  \
	    uniq | wc -l | grep 1 | $(FAIL_IF_EMPTY);			     \
	else								     \
	  echo WARNING: ld did not uniq the strings;			     \
	  echo WARNING: silently giving up;				     \
	fi

	$(PASS_IFF) true

clean:
	rm -rf foo.o bar.o main.o main
