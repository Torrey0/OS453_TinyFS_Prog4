
#include <stdio.h>
#include <string.h>

#include "tinyFS.h"
#include "tinyFSStructs.h"
#include "tinyFS_errno.h"

#include "tinyFSOpenList.h"
#include "safeDiskUtil.h"
#include "tinyFSHelper.h"

//add-ons
#include "tinyFSExtraFeatures.h"

static int mountedDiskFD=-1;    //which disk is mounted? will be used as the file descriptor for all calls to libdisk (outside of mkfs and mount/unmount)

/* Makes a blank TinyFS file system */
//rounds nBytes down to nearest 256
int tfs_mkfs(char *filename, int nBytes) {
    if(nBytes<BLOCKSIZE){   //not enough room for a super block
        return ENOSPC;
    }
    //Create the disk
    int diskFd = openDisk(filename, nBytes); 
    if (diskFd < 0) {
        printf("Failed to open or create disk file: %s\n", filename);
        return EIO;
    }

    // Create the super block
    superBlock sb;
    memset(&sb, 0, sizeof(superBlock)); //initialize to all 0's. Necessary for inode indices to start out 0 (unlinked)
    uint32_t numBlocks=nBytes/BLOCKSIZE;
    if(numBlocks>maxBlockCount){
        return ENOSPC;
    }
    sb.numBlocks=numBlocks;
    //write the super block
    if (safeWriteBlock(diskFd, 0, (void *)&sb, SUPER_ID, MAGIC_NUMBER) != 0) {
        printf("Failed to write superblock\n");
        closeDisk(diskFd);
        return EIO;
    }

    //write the free blocks
    for(int i=1;i<numBlocks;i++){
        if(setBlockFree(i, diskFd)<0){
            printf("Failed while writing free blocks\n");
            return ECORRUPT;
        }
    }

    // Close the disk
    if (closeDisk(diskFd) != 0) {
        printf("Failed to close disk\n");
        return EIO;
    }
    return 0;  // success
}

/* Mounts a TinyFS file system */
int tfs_mount(char *diskname) {
    if(mountedDiskFD>=0){   
        return EALREADY;    //indicate another disk is already monuted
    }
    //otherwise, no disk is mounted
    if (!opendFDListInitialized()){ //if we have already initialized our FDs list, no need to re-initialize
        openFDs_Reset();    //setup the open files structure
    }
    
    int fd=openDisk(diskname, 0);  //open an already existing disk
    if(fd<0){
        return ENOENT;
    }
    mountedDiskFD=fd;   //update our mounted disk
    return 0;
}

/* Unmounts the currently mounted file system */
int tfs_unmount(void) {
    if(mountedDiskFD>=0){
        if (closeDisk(mountedDiskFD) != 0) {
            printf("Failed to close disk\n");
            return EIO;
        }
        openFDs_Reset();
    }
    mountedDiskFD=-1;
    return 0;
}

/* Opens or creates a file, returning a file descriptor */
//iterates through the disk, searching for a file with matching name. if no file is found, one is created. returns the fileDescriptor opened or created on success
fileDescriptor tfs_openFile(char *name) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }
    if(name==NULL){
        return EPARAM;
    }
    char formattedName[8];
    formatName(name, formattedName);    //format the name in line with how they are stored in our inodes
    //check if the file is already open
    tinyOpenFile* file=openFDs_searchByName(formattedName);
    if(file!=NULL){ //if this file is already open
        return  file->fd;   //return its fd
    }
    
    //scan the superblock to see if the file is already present. If so, return that FD
    superBlock sb;
    int err=safeReadBlock(mountedDiskFD,0,(void*) (&sb), SUPER_ID, MAGIC_NUMBER);
    if (err<0){   //read the superblock of currently mounted disk
        return err;
    }
    //search the superBlock
    int32_t startingFreeLink=-1; //populated during searchSB with first unused link in superblock
    fileDescriptor sbErr=searchSB(&sb, mountedDiskFD, formattedName, &startingFreeLink);
    if(sbErr){  //error searching or file descriptor, or we obtained the file descriptor
        return sbErr;
    }

    //Since the file is not already present, create a new inode linked at startingFreeLink with name formattedName
    return createInode(startingFreeLink, formattedName, mountedDiskFD, &sb);    //return FD on for new file on success, or an error code on failure    
}

/* Closes the specified file */
int tfs_closeFile(fileDescriptor FD) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }
    if(openFDs_removeByFD(FD)<0){  //remove the file. This also de-allocates its resources
        return ENOENT;
    }
    return 0;
}


/* Writes entire buffer to the file, overwriting previous content */
int tfs_writeFile(fileDescriptor FD, char *buffer, int size) {
    int err;
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }
    int32_t diskSize=getTargetDiskBlocks(mountedDiskFD);
    if(diskSize<0){
        return EIO;
    }
    tinyOpenFile* file=openFDs_searchByFD(FD);
    if(file==NULL){
        return ENOENT;
    }
    if((err=checkPermission(mountedDiskFD, file))){
        return err;
    }
    if(eraseFileData(file, mountedDiskFD)){    //erase all contents of a given file (except for the inode)
        return EIO;
    }
    int nextBlock=findFreeBlock(mountedDiskFD, diskSize, 0);
    if(nextBlock<0){
        return nextBlock;
    }
    //update the inode to point at the next block & update its time
    err=updateInode(file, nextBlock, mountedDiskFD, size);
    if(err<0){
        return err;
    }
    file->location=0;   //reset the file location

    fileExtentBlock fe;

    while(size!=0){ //while there is still more of the block to write
        int blockLen= (size>= dataPerBlock)? dataPerBlock : size;    //pick max of our current remaining text length, or the maxTextLength
        memcpy(fe.data,buffer,blockLen);  //copy data into our fe block

        int currentBlock=nextBlock; //save which block we want to write to
        if(size<=dataPerBlock){ //if we are on the last block (no more writes after this), dont link it
            nextBlock=noBlockLink;
            if(size!=dataPerBlock){ //dont pollute with a bunch of trash values
                memset(fe.data + size, 0, dataPerBlock-size);
            }
        }else{  //find the next free block
            nextBlock=findFreeBlock(mountedDiskFD, diskSize, nextBlock);   
            if(nextBlock<0){    //error check.
                return nextBlock;   //hopefully no files get half written :)
            }
        }
        fe.nextBlock=nextBlock; //pointing at next block
        //write the currentblock
        if(safeWriteBlock(mountedDiskFD, currentBlock, (void*)&fe, FILE_EXTENT_ID, MAGIC_NUMBER)<0){
            return EIO;
        }
        buffer+=blockLen;   //move up text ptr after sending that text
        size-=blockLen;
    }
    return 0;
}

/* Deletes a file and frees its blocks */
int tfs_deleteFile(fileDescriptor FD) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }
    tinyOpenFile* file=openFDs_searchByFD(FD);
    if(file==NULL){
        return ENOENT;
    }
    if(eraseFileData(file, mountedDiskFD)){    //erase all contents of a given file (except for the inode)
        return EIO;
    }
    if(removeInode(file, mountedDiskFD)){
        return EIO;
    }
    if(openFDs_removeByFD(FD)){
        return ECORRUPT;    //our LL is corrupt.?
    }
    return 0;
}

/* Reads one byte from the file at the current file pointer */
int tfs_readByte(fileDescriptor FD, char *buffer) {
    int err;
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }
    tinyOpenFile* file = openFDs_searchByFD(FD);
    if(file==NULL){ //the file isn't open
        return ENOENT;
    }
    if((err=updateInodeAccess(file, mountedDiskFD))){    //update the access time of this inode
        return err;
    }
    int32_t byteIndex=file->location;   //retrieve needed file info
    int32_t fileSize=file->fileSize;
    if(byteIndex>fileSize){ //dont allow out of bounds reads
        return ERANGE;  
    }
    fileExtentBlock fe;
    uint8_t nextBlock=file->nextBlockIndex;

    while(byteIndex>=dataPerBlock){ //iterate to the targetBlock
        
        if(nextBlock==noBlockLink){ //we expect a block link sinc
            return ECORRUPT;
        }
        err=safeReadBlock(mountedDiskFD, nextBlock, (void*) &fe, FILE_EXTENT_ID, MAGIC_NUMBER);
        if(err<0){   //read the next block
            return err;
        }
        byteIndex-=dataPerBlock;
        nextBlock=fe.nextBlock;       
    }
    //this really should be a do while loop but for some reason when I do it its cooked. I dont have the brain power for this right now. The file system will survive with an unecessary block read.
    err=safeReadBlock(mountedDiskFD, nextBlock, (void*) &fe, FILE_EXTENT_ID, MAGIC_NUMBER);
    if(err<0){
        return err;
    }
    //we have reached the target block, retrieve the target byte
    // printf("byteIndex: %d nextBlockIndex: %d\n", byteIndex, nextBlock);
    (*buffer)= (fe.data)[byteIndex];
    // printf("target byte: %c\n", (fe.data)[byteIndex]);
    (file->location)++;
    return 0;
}

/* Seeks to an absolute offset within the file */
int tfs_seek(fileDescriptor FD, int offset) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }
    tinyOpenFile* file = openFDs_searchByFD(FD);
    if(file==NULL){ //the file isn't open
        return ENOENT;
    }
    if((offset<0) || (offset>file->fileSize)){
        return ERANGE;
    }
    file->location=offset;
    return 0;
}

//redirections to part (a) add-ons
uint8_t* tfs_displayFragments(int* len){
    if(mountedDiskFD<0){    //check that we have a file system mounted
        *len=ENODEV;
        return NULL;
    }    
    return tfs_displayFragments_a(len, mountedDiskFD);
}

//redirections to files containing add-on features. I didn't want to make mounted disk extern.
int tfs_defrag() {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }   
    return tfs_defrag_a(mountedDiskFD);
}


// --- Part b ---
int tfs_rename(fileDescriptor fd, char* newName){
        if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }   
    return tfs_rename_b(mountedDiskFD, fd, newName);
}

int tfs_readdir() {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }   
    return tfs_readdir_b(mountedDiskFD);
}


// --- Part d ---
int tfs_makeRO(char* name) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }   
    return tfs_makeRO_d(mountedDiskFD, name);
}

int tfs_makeRW(char* name) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }   
    return tfs_makeRW_d(mountedDiskFD, name);
}

//write one byte to the current location
int tfs_writeByte(fileDescriptor fd, uint8_t data) {
    if(mountedDiskFD<0){    //check that we have a file system mounted
        return ENODEV;
    }   
    return tfs_writeByte_d(mountedDiskFD, fd, data);
}


// --- Part e ---
uint8_t* tfs_readFileInfo(int fileDescriptor, int* len) {
        if(mountedDiskFD<0){    //check that we have a file system mounted
            *len =ENODEV;
            return NULL;
        }   
    *len=0;
    return tfs_readFileInfo_e(mountedDiskFD, fileDescriptor, len);
}

void tfs_printFileInfo(uint8_t* inodeInfo){
    tfs_printFileInfo_e(inodeInfo);
}

