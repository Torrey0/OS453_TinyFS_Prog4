
#include "safeDiskUtil.h"
#include "tinyFS_errno.h"
#include "libDisk.h"


#include <string.h>
#include <stdio.h>
//This file wraps read and write of libDisk with safety checks. Does not check if blocks have write permissions disabled (this would only be labeled on the inode, so not very useful here)

//errors due to invalid parameters here return EIO as opposed to EPARAM, since I am considering this a part of the IO
//EPARAM is more specifically reserved for invalid inputs passed by the user to the TinyFS API functions.

//when passing ANY_BLOCK read, you must provide a buffer of size file_extent generic block, or file-extent block, which both by design are (the full size of a block)

//confirm that bNum is in the range of blocks on the disk. used by safeRead/Write
int checkbNum(int disk, int bNum, blockType type){
    // printf("type: %d, bNum: %d, SUPER_ID: %d, type==SUPER_ID: %d\n", type, bNum, SUPER_ID, type==SUPER_ID);
    if(bNum==0){
        return (type!=SUPER_ID);
    }
    if(bNum<0){
        printf("Error: attepmting to read/write block <0\n");
        return EIO;
    }
    uint8_t recvBlock [BLOCKSIZE];
    if(readBlock(disk, 0, recvBlock)<0){
        printf("readBlock Error in checkbNum\n");
        return EIO;
    }
    superBlock* sb=(superBlock*) recvBlock; //interpret the block as a super block
    if(!((sb->numBlocks)>bNum)){    //numBlocks includes the superBlock (so is 0 indexed)
        printf("Error: attempting to read/write beyond the size of disk, as indicated by its superblock\n");
        return EIO;
    }
    return 0;   //Otherise bNum is in bounds
}

int max(int a, int b){
    return a>b? a: b;
}

//retrieve the blocksize of the give blockType. used by safeRead/Write
int getBlockSize(int disk, blockType type, int bNum, int writeFlag){
    if(writeFlag && (type==GENERIC_ID)){
        printf("You need to specify what u are writing!\n");
        return EIO;
    }
    int blockSize=-1;
    if((type & SUPER_ID) == SUPER_ID){
        blockSize   =max(blockSize, sizeof(superBlock));
        if(bNum!=0){
            printf("can only read superBlock from 0\n");
            return EIO;
        }
    }
    if ((type & INODE_ID) ==INODE_ID){
        blockSize   =max(blockSize, sizeof(inodeBlock));
    }
    if ((type & FREE_ID) == FREE_ID){
        blockSize   =max(blockSize, sizeof(freeBlock));
    }
    if ((type & FILE_EXTENT_ID) ==FILE_EXTENT_ID){
        blockSize   =max(blockSize, sizeof(fileExtentBlock));
    }
    if(type==GENERIC_ID){
        blockSize=BLOCKSIZE;
    }
    if(blockSize==-1){
        printf("Error, unrecognized blockType passed into read/write\n");
        return EIO;
    }
    return blockSize;
}

//readBlock with + error detection
int safeReadBlock(int disk, int bNum, void *returnedBlock, blockType type, uint8_t magicNumber){
    if(checkbNum(disk, bNum, type)){
        return EIO;
    }
    int blockSize=getBlockSize(disk, type, bNum, 0);
    if(blockSize<0){
        return EIO;
    }
    //
    uint8_t recvBlock [BLOCKSIZE];  //block used for reading out of read block must be BLOCKSIZE. returnedBlock is only required to be size of blockType, which is BLOCKSIZE for ANY_BLOCK
    int err=readBlock(disk, bNum, recvBlock);
    if(err){
        printf("Error: readBlockError in safeRead\n");
        return EIO;
    }
    freeBlock* fb=(freeBlock*) recvBlock;//interpret the block as a free block, which still contains the basic fields of all block
    if ((fb->magicNumber!=MAGIC_NUMBER) || (fb->padding!=0)){   //check padding and magic number
        printf("incorrect padding or magicNo in read\n");
        return ECORRUPT;
    }
    if((type!=GENERIC_ID) &&(fb->blockID!= (type & fb->blockID))){   //the block ID doesnt match what the system expects the block to be
        printf("incorrect block Type in read. Wanted to read: %d, actually read: %d\n", type, fb->blockID);
        return ECORRUPT;
    }    

    memcpy(returnedBlock, recvBlock, blockSize);    //copy the value over for the user
    return 0;
}


//checks that the write being performed is safe. Still up to user to check what blocks are read enabled.
// checks that bNum is in bounds, and type is acceptable
// Also sets the magicNumber, padding, and type of the block of the block. DOES NOT SET NEXT BLOCK! (calling function must set next block)

int safeWriteBlock(int disk, int bNum, void *inputBlock, blockType type, uint8_t magicNumber){
    if(checkbNum(disk, bNum, type)){
        printf("invalid bnum\n");
        return EIO;
    }
    int blockSize=getBlockSize(disk, type, bNum, 1);
    if(blockSize<0){
        printf("invalid type\n");
        return EIO;
    }

    //assign values to their input block
    freeBlock* fe=(freeBlock*) inputBlock;    //interpret the block as a free block, which still contains the basic fields of all block
    fe->magicNumber=MAGIC_NUMBER;
    fe->padding=0;
    fe->blockID=type;
    // printf("writing to block %d, with type: %d\n", bNum, type);
    uint8_t writingBlock [BLOCKSIZE]; //we will write this block over
    if(blockSize!=BLOCKSIZE){   //write rest of block to be zeros if the block doesn't take up the whole space
        memset(writingBlock, 0, BLOCKSIZE);
    }
    memcpy(writingBlock, inputBlock, blockSize);    //copy the value over for the user
    if (writeBlock(disk, bNum, writingBlock)<0){
        printf("Error: writeBlockError in safeWrite\n");
        return EIO;
    }
    return 0;
}