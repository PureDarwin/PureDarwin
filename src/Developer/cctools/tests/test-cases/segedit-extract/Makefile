# PLATFORM: MACOS IOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	$(CC) -arch $(ARCH) -Wl,-sectcreate,Memento,Mori,MementoMori1.txt     \
		-o main main.c
	if [ "$(ARCH)" == "x64_64" ]; 					      \
	then								      \
	    ./main | grep "Remember that you will die." | ${FAIL_IF_EMPTY};   \
	fi
	${SEGEDIT} -e Memento Mori MementoMori2.txt main
	${PASS_IFF_SUCCESS} diff MementoMori1.txt MementoMori2.txt

clean:
	rm -rf main MementoMori2.txt
