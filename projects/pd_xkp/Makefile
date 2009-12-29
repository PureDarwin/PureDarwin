PROGRAM	= pd_xkp 
CC	= gcc
ARCHS   = -arch i386
#ARCHS   = -arch i386 -arch x86_64 -arch ppc
CFLAGS	= -g -fno-objc-gc -fno-replace-objc-classes -O3 $(ARCHS)
LIBS	= -lobjc

SRC=Scrambler.m Crash.m Usage.m main.m 
OBJ=Scrambler.o Crash.o Usage.o main.o 

.SUFFIXES: .o .m
.m.o:
	$(CC) -W -Wall -Wno-import $(CFLAGS) -c $<
	

objects:	$(OBJ)
	$(CC) $(CFLAGS) -o $(PROGRAM) $(OBJ) $(LIBS)

clean:
	rm -f $(OBJ)
	rm -f $(PROGRAM)

.SUFFIXES: .m .o 
