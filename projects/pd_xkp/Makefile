PROGRAM	= pd_xkp 
CC	= gcc
CFLAGS	= -g -fno-objc-gc -fno-replace-objc-classes -O3 
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
