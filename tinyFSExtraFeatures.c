
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#include "tinyFS.h"
#include "tinyFSStructs.h"
#include "tinyFSHelper.h"
#include "safeDiskUtil.h"
#include "tinyFS_errno.h"




// --- Part a ---
//dynamically allocates dsplay map with malloc. returns pointer to displayMap, with length of display map stored in len
//on error, returns Null, and sets len=the error code. On error, displayMap wont be allocated
//On the map: 0=freeBLock. 1=superBlock. >1 = a file, each file indicated with its own number
uint8_t* tfs_displayFragments_a(int* len, int targetDisk){
    //read the super block
    superBlock sb;
    int err=safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER);
    if(err<0){
        *len=err;
        return NULL;
    }
        *len=sb.numBlocks;
    //set up map
    uint8_t* displayMap=malloc((*len) * sizeof(uint8_t));
    memset(displayMap, 0, sb.numBlocks);
    displayMap[0]=SUPER_ID;    //set super block
    uint8_t currFileID=2;    //map assigns the blocks for each file an ID, that allows you to identify that block
    //if we have maxBlockCount seperate files, the last file will be displayed with a 0, so still distinct ig? (come srsly, who would ever allocate 255 empty files?)
    //loop over all files
    for(int i=0;i<sb.numBlocks;i++){
        uint8_t currBlock=sb.inodeIndices[i];
        if(currBlock != noBlockLink){
            //we found a file
            fileExtentBlock fe;
            //iterate over file
            while (currBlock != noBlockLink){
                displayMap[currBlock]=currFileID;  //+1 since inodeIndices don't include the superblock
                int err=safeReadBlock(targetDisk, currBlock, (void*)&fe, FILE_EXTENT_ID | FREE_ID | INODE_ID, MAGIC_NUMBER);
                if(err<0){
                    printf("error of %d while reading File No %d, Ending early, you can still view what was mapped\n", err, currFileID);
                    return displayMap;
                    //conisder making this an ECORRUPT
                }
                currBlock=fe.nextBlock;
            }
            currFileID++;
        }
    }
    return displayMap;
}

//helper functions for defragmenting the disk
int swapInode(int inodeLinkIndex, int inIndex, int freeIndex, int targetDiskFD){
    superBlock sb;
    int  errCheck=safeReadBlock(targetDiskFD, 0,    (void*)&sb, SUPER_ID, MAGIC_NUMBER);
    if (errCheck<0){  //read the inode block for this potential match
        printf("issue while swapping Inode's??\n");
        return errCheck;
    }
    sb.inodeIndices[inodeLinkIndex]=freeIndex;    //update where the superBlock points

    inodeBlock targIN;//retrieve the inode
    errCheck=safeReadBlock(targetDiskFD, inIndex, (void*) &targIN, INODE_ID, MAGIC_NUMBER);
    if (errCheck<0){  //read the inode block for this potential match
        printf("issue while swapping Inode's??\n");
        return errCheck;
    }
    //write the inode to the free block
    errCheck=safeWriteBlock(targetDiskFD, freeIndex, (void*)&targIN, INODE_ID, MAGIC_NUMBER);
    if(errCheck<0){
        printf("issue while swapping Inode's??\n");
        return errCheck;
    }

    //update the super block
    errCheck=safeWriteBlock(targetDiskFD, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER);
    if(errCheck<0){
        printf("issue while swapping Inode's??\n");
        return errCheck;
    }
    //replace the old inode block with free block
    errCheck=setBlockFree(inIndex, targetDiskFD);
    if(errCheck<0){
        printf("issue while swapping Inode's??\n");
    }
    return errCheck;
}


int swapFileExtent(int prevIndex, int feIndex, int freeIndex, int targetDiskFD){
    //retrieve both blocks
    genericBlock prevBlock;
    int errCheck=safeReadBlock(targetDiskFD, prevIndex, (void*) &prevBlock, GENERIC_ID, MAGIC_NUMBER);
    if (errCheck<0){  //read the inode block for this potential match
        printf("issue while swapping FE's??\n");
        return errCheck;
    }
    fileExtentBlock targFE;
    errCheck=safeReadBlock(targetDiskFD, feIndex, (void*) &targFE, FILE_EXTENT_ID, MAGIC_NUMBER);
    if (errCheck<0){  //read the inode block for this potential match
        printf("issue while swapping FE's??\n");
        return errCheck;
    }
    //

    //write the target FE to the location of the freeBlock
    errCheck=safeWriteBlock(targetDiskFD, freeIndex, (void*)&targFE, FILE_EXTENT_ID, MAGIC_NUMBER);
    if(errCheck<0){
        printf("issue in writing to swap FE's??\n");
        return errCheck;
    }

    //update the pointer of the previousBlock
    prevBlock.nextBlock=freeIndex;  //update the pointer block to point to the free block location
    errCheck=safeWriteBlock(targetDiskFD, prevIndex, (void*)&prevBlock, prevBlock.blockID, MAGIC_NUMBER);
    if(errCheck<0){
        printf("issue in writing to swap FE's??\n");
        return errCheck;
    }
    //
    //replace the fe with the a freeBlock:
    errCheck= setBlockFree(feIndex, targetDiskFD);
    if(errCheck<0){
        printf("issue seeting free in FE swap\n");
    }
    return errCheck;
}

int iterateFEs(int feIndex, int fePrevIndex, int targetDiskFD, int* freeBlockIndex, int targetDiskNumBlocks){
    //iterate over all file extents for this inode, swapping with freeIndex if they are in high location
    int errCheck=0;
    fileExtentBlock fe;
    while(feIndex!=noBlockLink){
        errCheck=safeReadBlock(targetDiskFD, feIndex, (void*) &fe, FILE_EXTENT_ID, MAGIC_NUMBER);
        if (errCheck<0){  //read the inode block for this potential match
            printf("issue while linking through FE's??\n");
            return errCheck;
        }
        if(feIndex>(*freeBlockIndex)){
            if((errCheck=swapFileExtent(fePrevIndex, feIndex, (*freeBlockIndex), targetDiskFD))){   //params are: how reach block, block, where block go
                return errCheck;
            }
            fePrevIndex=*freeBlockIndex;    //update previous to be at previousfreeBlock
            (*freeBlockIndex)=findFreeBlock(targetDiskFD, targetDiskNumBlocks, (*freeBlockIndex));
            if((*freeBlockIndex)<0){
                printf("There is almost certainly a logic error in tfs_defrag pt 2.\n");
                return (*freeBlockIndex);
            }
        }else{
            fePrevIndex=feIndex;    //update previous to be at current
        }

        feIndex=fe.nextBlock;
    }
    return 0;
}

//de-fragments the disk
int tfs_defrag_a(int targetDisk) {

    int errCheck=0;
    int32_t targetDiskNumBlocks= getTargetDiskBlocks(targetDisk);
    if(targetDiskNumBlocks<0){
        return targetDiskNumBlocks;
    }
    //defragment by iterating over all files. Any time we reach a block with index larger than the next free block, swap those two blocks
    //start from begging for free blocks
    int freeBlockIndex=findFreeBlock(targetDisk, targetDiskNumBlocks, 0);
    if(freeBlockIndex==ENOSPC){ //nothing to defragment
        return 0;
    }
    superBlock sb;
    if((errCheck=safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER))){
        return errCheck;
    }
    inodeBlock in;  //used as a buffer when reading an inode block

    //iterate over all inodes
    for(int i=0;i<(sb.numBlocks);i++){
        int inodeIndex=sb.inodeIndices[i];   
        if(inodeIndex==noBlockLink){
            continue;
        }
        errCheck=safeReadBlock(targetDisk, inodeIndex, (void*)&in, INODE_ID, MAGIC_NUMBER);
        if(errCheck){
            return errCheck;
        }
        //check if this inode needs swapping
        if(inodeIndex>freeBlockIndex){
            if((errCheck=swapInode(i, inodeIndex, freeBlockIndex, targetDisk))){ //params are: how reach block, block, where block go
                return errCheck;
            }
            inodeIndex=freeBlockIndex;
            freeBlockIndex=findFreeBlock(targetDisk, sb.numBlocks, freeBlockIndex);
            if(freeBlockIndex<0){
                printf("There is almost certainly a logic error in tfs_defrag\n");
                return freeBlockIndex;
            }
        }
        iterateFEs(in.nextBlock, inodeIndex, targetDisk, &freeBlockIndex, sb.numBlocks);    //iterate over FE's of this inode, swapping as needed
    }
    return 0;
}

//part B

int tfs_readdir_b(int targetDisk){
    int errCheck;
    superBlock sb;
    if((errCheck=safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER))){
        return errCheck;
    }
    inodeBlock in;  //used as a buffer when reading an inode block

    //iterate over all inodes
    printf("Home Directory\n");
    for(int i=0;i<(sb.numBlocks);i++){
        int inodeIndex=sb.inodeIndices[i];   
        if(inodeIndex==noBlockLink){
            continue;
        }
        //read the inode
        errCheck=safeReadBlock(targetDisk, inodeIndex, (void*)&in, INODE_ID, MAGIC_NUMBER);
        if(errCheck){
            return errCheck;
        }
        char fileNameTerminated[maxFileNameLength+1]={0};   //+1 ensures this will be a null terminated string
        memcpy(fileNameTerminated, in.fileName, maxFileNameLength);
        printf("%s\n", fileNameTerminated);
    }
    return 0;
}

int tfs_rename_b(int targetDisk, fileDescriptor fd, char* newName){
    //open the sb
    int errCheck;
    superBlock sb;
    if((errCheck=safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER))){
        return errCheck;
    }
    //returns the nextBlockIndex of the corresponding inode so we can quickly go to it on disk
    tinyOpenFile* file=openFDs_searchByFD(fd);
    if(file==NULL){ //check if fd isn't open
        return ENOENT;
    }


    inodeBlock in;  //read the target Inoede
    if((errCheck=safeReadBlock(targetDisk, (sb.inodeIndices)[file->inodeLinkIndex], (void*)&in, INODE_ID, MAGIC_NUMBER))){
        return errCheck;
    }
    //format input string to match our way of storing names
    char formattedName[8]={0};
    formatName(newName, formattedName);
    //write the new name to and access time info to inode
    memcpy(in.fileName, formattedName, maxFileNameLength);  
    retrieveTime(&(in.modificationTime));
    memcpy(&in.accessTime, &in.modificationTime, sizeof(timestamp));

    if((errCheck=safeWriteBlock(targetDisk, (sb.inodeIndices)[file->inodeLinkIndex], (void*)&in, INODE_ID, MAGIC_NUMBER))){
        return errCheck;
    }
    //update the open file descriptor with the new name
    memcpy(file->fileName, formattedName, maxFileNameLength);
    return 0;
}

//part hellper function (write only mode) and write byte

//needing by writing functions to check if they can write
int checkPermission(int targetDisk, tinyOpenFile* file){
    //Open the super block, which we will need to find the file
    superBlock sb;
    int err=safeReadBlock(targetDisk,0,(void*) (&sb), SUPER_ID, MAGIC_NUMBER);
    if (err<0){   //read the superblock of currently mounted disk
        return err;
    }
    //read the inode
    int inodeIndex= (sb.inodeIndices)[file->inodeLinkIndex];    //get the index of this inode on disk
    inodeBlock in;
    if((err=readBlock(targetDisk, inodeIndex, (void*)&in))){
        return err;
    }
    if(in.readMode==readOnly){
        return EPERM;
    }
    return 0;
}

int changePermissions(int targetDisk, char* name, readFlag readMode){
    if(name==NULL){
        return EPARAM;
    }
    char formattedName[8];
    formatName(name, formattedName);    //format the name in line with how they are stored in our inodes

    //check if the file is already open
    fileDescriptor tempFD=-1;    //stores temperorary FD of opened file
    //Open the super block, which we will need to find the file
    superBlock sb;
    int err=safeReadBlock(targetDisk,0,(void*) (&sb), SUPER_ID, MAGIC_NUMBER);
    if (err<0){   //read the superblock of currently mounted disk
        return err;
    }

    tinyOpenFile* file=openFDs_searchByName(formattedName);
    if(file==NULL){ //if this file isnt already open, open it
        //search the superBlock
        int32_t startingFreeLink=-1; //populated during searchSB with first unused link in superblock
        int sbErr=searchSB(&sb, targetDisk, formattedName, &startingFreeLink);
        if(sbErr<0){  //error searching or file descriptor
            return sbErr;
        }
        if(sbErr==0){   //we couldnt find the file
            return ENOENT;
        }
        //otherwise, we found the file. Lets open it
        tempFD=tfs_openFile(formattedName);
        file=openFDs_searchByFD(tempFD);
        if(file==NULL){
            return EIO;
        }
    }
    //we now have an open FD for the file
    int inodeIndex= (sb.inodeIndices)[file->inodeLinkIndex];    //get the index of this inode on disk
    inodeBlock in;
    if((err=readBlock(targetDisk, inodeIndex, (void*)&in))){
        return err;
    }
    in.readMode=readMode;
    //update the permission
    if((err=writeBlock(targetDisk, inodeIndex,  (void*)&in))){
        return err;
    }
    if(tempFD!=-1){
        //close the file if we opened it:
        tfs_closeFile(tempFD);
    }
    return 0;

}

//part d:
int tfs_makeRW_d(int targetDisk, char* name){
    return changePermissions(targetDisk, name, readAndWrite);
}

int tfs_makeRO_d(int targetDisk, char* name){
    return changePermissions(targetDisk, name, readOnly);
}
//simialr to read byte
int tfs_writeByte_d(int targetDisk, fileDescriptor fd, uint8_t byte){
    int err;
    tinyOpenFile* file=openFDs_searchByFD(fd);
    if(file==NULL){
        return ENOENT;
    }
    if((err=checkPermission(targetDisk, file))){
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
        err=safeReadBlock(targetDisk, nextBlock, (void*) &fe, FILE_EXTENT_ID, MAGIC_NUMBER);
        if(err<0){   //read the next block
            return err;
        }
        byteIndex-=dataPerBlock;
        nextBlock=fe.nextBlock;       
    }
    //this really should be a do while loop but for some reason when I do it its cooked. I dont have the brain power for this right now. The file system will survive with an unecessary block read.
    err=safeReadBlock(targetDisk, nextBlock, (void*) &fe, FILE_EXTENT_ID, MAGIC_NUMBER);
    if(err<0){
        return err;
    }
    //we have reached the target block. Write to it!
    (fe.data)[byteIndex] = byte;

    return safeWriteBlock(targetDisk, nextBlock, (void*)&fe, FILE_EXTENT_ID, MAGIC_NUMBER);
}

//part e:
uint8_t* tfs_readFileInfo_e(int targetDisk, fileDescriptor fd, int* len){
    //open the sb
    superBlock sb;
    if(((*len)=safeReadBlock(targetDisk, 0, (void*)&sb, SUPER_ID, MAGIC_NUMBER))){
        return NULL;
    }
    //returns the nextBlockIndex of the corresponding inode so we can quickly go to it on disk
    tinyOpenFile* file=openFDs_searchByFD(fd);
    if(file==NULL){ //check if fd isn't open
        (*len)= ENOENT;
        return NULL;
    }

    inodeBlock* in=malloc(sizeof(inodeBlock));  //dynamically allocate the inode
    //read off the disk, directly into the allocated inode
    if(((*len)=safeReadBlock(targetDisk, (sb.inodeIndices)[file->inodeLinkIndex], in, INODE_ID, MAGIC_NUMBER))){
        return NULL;
    }

    *len=sizeof(inodeBlock);
    return (uint8_t*)in;    //return the inode
}
 //this is the offset for tm.year
#define yearOffset 1900
void printTime(timestamp time){
    printf("time of day: %d:%d:%d, date: %d:%d:%d\n",  time.second/3600, (time.second%3600)/60, time.second%60, time.month, time.day, time.year + yearOffset);
}

//I provide a function that can be used to print this information nicely (so we can also verify timestamps)
void tfs_printFileInfo_e(uint8_t* inodeInfo){
    inodeBlock* in=(inodeBlock*) inodeInfo;
    //print all the fields:
    printf("\nPrinting File Info\n");
    char terminatedName[9]={0};
    memcpy(terminatedName, in->fileName, maxFileNameLength);
    printf("fileName: %s, size: %u", terminatedName, in->fileSize);
    if(in->readMode == readOnly){
        printf(", readStatus=readOnly\n");
    }else{
        printf(", readStatus= read and write\n");
    }
    printf("Creation: \t\t");
    printTime(in->creationTime);
    printf("last Modification: \t");
    printTime(in->modificationTime);
    printf("Last Access: \t\t");
    printTime(in->accessTime);
    printf("\n");
}