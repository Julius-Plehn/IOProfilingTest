# Common definitions
CC = mpicc
# Compiler flags, paths and libraries
CFLAGS = -std=c11 -pedantic -Wall -Wextra -O3

mpi: CC=mpicc

scorep: CC=scorep mpicc

scorep: CFLAGS = -std=c11 -pedantic -Wall -Wextra -g


LFLAGS = $(CFLAGS)
LIBS   = #-lm

TGTS = writefile writefile-scorep

# Targets ...
all: writefile

mpi: writefile

scorep: writefile-scorep

writefile: writefile.o Makefile
	$(CC) $(LFLAGS) -o $@ writefile.o $(LIBS)

writefile-scorep: writefile-scorep.o Makefile
	$(CC) $(LFLAGS) -o $@ writefile-scorep.o $(LIBS)

writefile-scorep.o: writefile.c
	$(CC) -o writefile-scorep.o -c $(CFLAGS) writefile.c

# Rule to create *.o from *.c
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c




clean:
	$(RM) *.o
	$(RM) $(TGTS)
