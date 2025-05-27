#ifndef LIBDISK_H
#define LIBDISK_H

#define BLOCKSIZE 256

typedef struct disk Disk;
struct disk{
    int diskFd;
    int diskSize;   //number of blocks
};


int openDisk(char *filename, int nBytes);
int closeDisk(int disk);
int readBlock(int disk, int bNum, void *block);
int writeBlock(int disk, int bNum, void *block);

#endif // LIBDISK_H
