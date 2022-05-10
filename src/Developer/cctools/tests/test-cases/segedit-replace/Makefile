# PLATFORM: MACOS IOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# Build a test binary that contains a zero-length __DATA_DIRTY section
	# as well as a small (single page?) replacable section.
	$(CC) -arch $(ARCH) -Wl,-sectcreate,Memento,Mori,MementoMori1.txt    \
		-Wl,-dirty_data_list,dirty_data.txt			     \
		-o main main.c
	$(OTOOL) -lv main | grep __DATA_DIRTY -A4 -B2 | $(CHECK)
# CHECK:       cmd LC_SEGMENT_64
# CHECK:   segname __DATA_DIRTY
# CHECK:   fileoff 0
# CHECK:  filesize 0
# CHECK-NEXT: --

	# Replace a section without resizing.
	if [ "$(ARCH)" == "x64_64" ];					     \
	then								     \
	    ./main | grep "Remember that you will die." | ${FAIL_IF_EMPTY};  \
	fi
	${SEGEDIT} -r Memento Mori MementoMori2.txt -o main2 main
	if [ "$(ARCH)" == "x64_64" ];					     \
	then								     \
	    ./main2 | grep "Always remember that you will die." |	     \
	        ${FAIL_IF_EMPTY};					     \
	fi

	# Attempt to resize a section
	$(CC) -arch $(ARCH) -Wl,-sectcreate,Memento,Mori,/dev/null	     \
		-o main main.c
	${FAIL_IF_SUCCESS} ${SEGEDIT} -r Memento Mori MementoMori2.txt	     \
		-o main2 main 2>&1

	${PASS_IFF_SUCCESS} true

clean:
	rm -rf main main2
