.SUFFIXES: .gc32 .32
GCC=/usr/bin/gcc

SRC = escape.m escape2.m escape3.m escape4.m escape5.m escape6.m

first: all

ALL = $(SRC:.m=.32)

$(ALL): common.o

.m.gc32:
	$(GCC) $< -g -std=gnu99 -fobjc-gc-only -o $@ common.o -framework Foundation
.m.32:
	$(GCC) $<  -g -std=gnu99 -o $@ common.o -framework Foundation

all:  $(ALL)
	./escape.32
	./escape2.32
	./escape3.32
	./escape4.32
	./escape5.32
	./escape6.32

clean:
	rm -fr *.o $(ALL) *.dSYM

common.o: common.m
	$(GCC) -std=gnu99 -fobjc-gc -g -c common.m