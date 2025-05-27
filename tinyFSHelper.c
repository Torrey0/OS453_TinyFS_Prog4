
#include "tinyFS.h"
#include "stdlib.h"
#include "safeDiskUtil.h"
#include "tinyFSStructs.h"
#include "tinyFS_errno.h"
#include "tinyFSOpenList.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

void formatName(char* name, char* formattedName){
    for(int i=0;i<maxFileNameLength;i++){   
        formattedName[i]=name[i];
        if(name[i] == '\0') {
            // zero out remaining bytes after null terminator
            for(int j = i ; j < maxFileNameLength; j++) {
                formattedName[j] = '\0';
            }
            break;
        }
    }
}

//returns the index of the first block with nodeID=freeBlock after startingPosition. targetDiskLength can be found in the superBlock
//returns ENOSPC if none found
int findFreeBlock(int targetDiskFD, int targetDiskLength, int startingPosition){
    if(targetDiskLength>maxBlockCount){
        return ECORRUPT;    //should never happen
    }
    genericBlock gb;
    for(int i=startingPosition+1;i<targetDiskLength;i++){
        int err=safeReadBlock(targetDiskFD, i, (void*) &gb, GENERIC_ID, MAGIC_NUMBER);
        if (err<0){  //read the block
            return err;
        }
        //check if the block is free
        if(gb.blockID==FREE_ID){
            return i;
        }
    } 
    return ENOSPC;  //if we cant find a block, assume the user is requesting one, and we can return no space to them
}

int setBlockFree(int targetBlock, int targetDiskFD){
    freeBlock fb;   
    fb.nextBlock=noBlockLink;
    return safeWriteBlock(targetDiskFD, targetBlock, &fb, FREE_ID, MAGIC_NUMBER);
}

//return < 0 for error. return 0 if not found. return fd > 0 if block is found.
//if 0 is returned, startingFreeLink will be populated with an inode link index in the super bock which has not been used
fileDescriptor searchSB(superBlock* sb, int targetDiskFD, char* formattedName, int32_t* startingFreeLink){
    if((*sb).magicNumber!=MAGIC_NUMBER){
        return ECORRUPT;
    }
    if((*sb).numBlocks== maxBlockCount){
        return ENOSPC;
    }
    inodeBlock in;  //used as a buffer when reading an inode block
    //first scan the file system to see if the block already exists.
    for(int i=0;i<(sb->numBlocks);i++){

        int inodeIndex=(*sb).inodeIndices[i];
        if(inodeIndex!=noBlockLink){
            int err=safeReadBlock(targetDiskFD, inodeIndex, (void*) &in, INODE_ID, MAGIC_NUMBER);
            if (err<0){  //read the inode block for this potential match
                printf("had an error reading an inode link\n");
                return err;
            }
            if(in.magicNumber!=MAGIC_NUMBER){
                return ECORRUPT;
            }

            if (memcmp(in.fileName, formattedName, maxFileNameLength) == 0) {
                //the names match, this file already exists, give them an fd to it
                fileDescriptor FD=openFDs_add(in.fileSize, formattedName, i, in.nextBlock);    //add the file, with starting size 0
                if(FD<0){
                    return EIO;
                }else{
                    return FD;
                }
            }
        }else if(*startingFreeLink ==-1){  //there is no link on this byte yet
            *startingFreeLink=i;    //update the startingLink for use later so we dont have to search again
        }
    }
    //The file does not exist. We will need add a new one.
    if(*startingFreeLink==-1){   //we didnt find a free link, we are already full of files, and wont be able to add a new one
        return ENOSPC;
    }
    return 0;
}

//seconds are how many seconds into that day (out of 86400)
void retrieveTime(timestamp* timeRecord){
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    (*timeRecord).second=tm.tm_sec +  (60* tm.tm_min)+ ( 3600 * tm.tm_hour);
    (*timeRecord).day=tm.tm_mday;
    (*timeRecord).month=tm.tm_mon;
    (*timeRecord).year=tm.tm_year;
}

int updateInodeAccess(tinyOpenFile* file, int targetDisk){
    superBlock sb;
    
    int err = safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER);
    if(err<0){
        return err;
    }
    inodeBlock inode;
    if((err=safeReadBlock(targetDisk, sb.inodeIndices[file->inodeLinkIndex], (void*)&inode, INODE_ID, MAGIC_NUMBER))){
        return err;
    }
    retrieveTime(&(inode.accessTime));  //update the access time
    if((err=safeWriteBlock(targetDisk, sb.inodeIndices[file->inodeLinkIndex], (void*) &inode, INODE_ID, MAGIC_NUMBER))){
        return err;
    }
    return 0;
}

int updateInode(tinyOpenFile* file, uint8_t nextBlock, int targetDisk, int size){
    superBlock sb;
    
    int err = safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER);
    if(err<0){
        return err;
    }
    inodeBlock inode;
    inode.fileSize=size;  
    // printf("fileSize: %d\n", size);
    memcpy(&inode.fileName, file->fileName, maxFileNameLength);
    inode.nextBlock=nextBlock;
    //initialize inode creation time. copy the same value to access and modification field
    retrieveTime(&(inode.creationTime));
    memcpy(&inode.accessTime, &inode.creationTime, sizeof(timestamp));
    memcpy(&inode.modificationTime, &inode.creationTime, sizeof(timestamp));
    int ret= safeWriteBlock(targetDisk, sb.inodeIndices[file->inodeLinkIndex], (void*) &inode, INODE_ID, MAGIC_NUMBER);
    if(ret){    //check for errors writing the block
        return EIO;
    }
    file->nextBlockIndex=nextBlock; //update the open FD
    file->fileSize=size;
    return 0;
}

//a helper function for createInode
//initiate an inode of the given name on the target disk's inodeBlockIndex index.
int initiateInode(int inodeBlockIndex, const char* inodeName, int targetDisk){
    inodeBlock inode;
    inode.fileSize=0;
    memcpy(&inode.fileName, inodeName, maxFileNameLength);
    inode.nextBlock=noBlockLink;
    inode.readMode=readAndWrite;
    //initialize inode creation time. copy the same value to access and modification field
    retrieveTime(&(inode.creationTime));
    memcpy(&inode.accessTime, &inode.creationTime, sizeof(timestamp));
    memcpy(&inode.modificationTime, &inode.creationTime, sizeof(timestamp));

    int ret= safeWriteBlock(targetDisk, inodeBlockIndex, (void*) &inode, INODE_ID, MAGIC_NUMBER);
    if(ret){    //check for errors writing the block
        return EIO;
    }
    return 0;
}

int createInode(int inodeLinkIndex, char* inodeName, int targetDisk, superBlock* sb){
    //first find a free block
    int32_t startingFreeBlock = findFreeBlock(targetDisk, (*sb).numBlocks, 0);
    if(startingFreeBlock<0){    //check for error while searching for block
        return startingFreeBlock;
    }
    int inodeStatus=initiateInode(startingFreeBlock, inodeName, targetDisk);
    if(inodeStatus){
        return inodeStatus;     //indicate the detected error while initiating the inode
    }

    //update the superBlock:
    ((*sb).inodeIndices)[inodeLinkIndex]=startingFreeBlock;  //set the found open Link to point to the found open block.
    int writeErr=safeWriteBlock(targetDisk, 0,(void*) sb, SUPER_ID, MAGIC_NUMBER);  //write the starting block back
    if(writeErr){
        return EIO;
    }
    //add the inode to the FD list
    int FD=openFDs_add(0, inodeName, inodeLinkIndex, noBlockLink);
    if(FD<0){
        return EIO;
    }
    return FD;
}

//erases the chain of datablocks pointed to by this inode. Does not update the size of the inode on disk or in open-file list.
int eraseFileData(tinyOpenFile* file, int targetDisk){
    if(file==NULL){
        return ENOENT;
    }
    uint8_t nextBlockIndex=file->nextBlockIndex;
    fileExtentBlock in;
    while(nextBlockIndex!=noBlockLink){ //iterate over all blocks for this inode, freeing them
        //read the data block
        int err=safeReadBlock(targetDisk, nextBlockIndex, (void*) &in, FILE_EXTENT_ID | INODE_ID | FREE_ID, MAGIC_NUMBER); //this read could be an inode or FE
        if(err){ //failing a read here would be really bad, would lead to floating blocks. realistically should never happen? Could add a function that removes floating blocks as one of the add-ons i think?
            return err;       
        }
        //aquire the nextBlock this block points to
        int previousBlock=nextBlockIndex;
        nextBlockIndex=in.nextBlock;
        //free the data block
        if (setBlockFree(previousBlock, targetDisk)){
            return EIO;
        } 
    }
    return 0;
}

int removeInode(tinyOpenFile* file, int mountedDiskFD){
    superBlock sb;  //read the super block
    if(safeReadBlock(mountedDiskFD, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER)<0){
        return EIO;
    }
    //update the inodeIndex of the superBlock
    uint8_t inodeIndex=(sb.inodeIndices)[file->inodeLinkIndex];

    sb.inodeIndices[file->inodeLinkIndex]=noBlockLink;
    //write back the new super block, and overwrite the inode with a freeblock
    if(safeWriteBlock(mountedDiskFD, 0, &sb, SUPER_ID, MAGIC_NUMBER)<0){
        return EIO;
    }
    return setBlockFree(inodeIndex, mountedDiskFD);    //return the status of this last call
}

int32_t getTargetDiskBlocks(int targetDisk){
    superBlock sb;
    if(safeReadBlock(targetDisk,0,(void*)&sb, SUPER_ID, MAGIC_NUMBER)){
        return EIO;
    }
    return sb.numBlocks;
}