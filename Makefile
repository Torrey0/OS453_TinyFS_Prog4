# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall -std=gnu99
LIBS = 

OBJS = diskList.o libDisk.o tinyFS.o tinyFSHelper.o tinyFSExtraFeatures.o tinyFSOpenList.o safeDiskUtil.o

all:   testDisk

testDisk: testDisk.c $(OBJS)
	$(CC) $(CFLAGS) -o testDisk testDisk.c  $(OBJS) $(LIBS)


.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f testDisk *.o




