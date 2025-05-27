#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "libDisk.h"
#include "diskList.h"

static struct arrayList* openFDs;
int listInit=0;
int openDisk(char *filename, int nBytes){
    //check if this is the first time this is being called
    //if it is, we need to setup the arraylist of disks
    if(!listInit){ 
        openFDs=array_list_new();
        listInit=1;
    }

    int diskFD;

    if(nBytes==0){ //attempt to open a file

        diskFD= open(filename, O_RDWR, 0666);
        if(diskFD<0){
            return -1;
        }

        // Get file size to determine number of blocks
        off_t fileSize = lseek(diskFD, 0, SEEK_END);
        if (fileSize < 0 || fileSize % BLOCKSIZE != 0) {
            printf("fileSize is not matching numBlocks! This should never happen\n");
            close(diskFD);
            return -1;
        }

        arrayList_add(openFDs, diskFD, fileSize / BLOCKSIZE);
        return diskFD;
    }

    //otherwise, the user want to create a new disk
    if (nBytes < BLOCKSIZE){    //return failure for invalid nBytes size
        return -1;
    } 

     //make nBytes the nearest multiple off BLOCKSIZE rounded down
    nBytes=nBytes - (nBytes%BLOCKSIZE);

    diskFD = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(diskFD<0){
        return -1;
    }
    // Initialize file size
    if (ftruncate(diskFD, nBytes) < 0) {
        close(diskFD);
        return -1;
    }
    arrayList_add(openFDs, diskFD, nBytes / BLOCKSIZE);

    return diskFD;
}

int closeDisk(int disk){
    if(!listInit){
        return -1;
    }

    return arrayList_remove_by_fd(openFDs, disk);
}

//expects a block of size BLOCKSIZE
int readBlock(int disk, int bNum, void *block){
    if(!listInit){
        return -1;
    }
    Disk* targetDisk= arrayList_find_by_fd(openFDs, disk);
    if(targetDisk==NULL){   //return failure if disk isn't open
        return -1;
    }
    if (bNum < 0 || bNum >= targetDisk->diskSize) { //invalid read location
        return -1;
    }

    //otherwise, we are good to read the block number bNum into block
    off_t offset = bNum * BLOCKSIZE;         // Calculate the byte offset and seek to that location
    if (lseek(targetDisk->diskFd, offset, SEEK_SET) < 0) {
        return -1;
    }

    // Read BLOCKSIZE bytes into block
    ssize_t bytesRead = read(targetDisk->diskFd, block, BLOCKSIZE);
    if (bytesRead != BLOCKSIZE) {
        return -1;
    }

    return 0; 
}

int writeBlock(int disk, int bNum, void *block){
    if (!listInit) {
        return -1;
    }

    Disk* targetDisk = arrayList_find_by_fd(openFDs, disk);
    if (targetDisk == NULL) {   //return failure if disk isn't open
        return -1;
    }

    if (bNum < 0 || bNum >= targetDisk->diskSize) { // invalid write location
        return -1;
    }

    // Seek to the correct block offset
    off_t offset = bNum * BLOCKSIZE;
    if (lseek(targetDisk->diskFd, offset, SEEK_SET) < 0) {
        return -1;
    }

    // Write exactly BLOCKSIZE bytes from block
    ssize_t bytesWritten = write(targetDisk->diskFd, block, BLOCKSIZE);
    if (bytesWritten != BLOCKSIZE) {
        printf("WARNING: had an issue writing the precise number of blocks. This should never happen\n");
        return -1;
    }

    return 0; // success
}