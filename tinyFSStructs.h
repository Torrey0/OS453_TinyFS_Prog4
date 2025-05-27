
#ifndef TINYFS_STRUCTS_H
#define TINYFS_STRUCTS_H
#include "tinyFS.h"
#include <stdint.h>




typedef enum {
    SUPER_ID   = 1,
    INODE_ID        = 2,
    FILE_EXTENT_ID  = 4,    //file extent blocks can also be used to read inode blocks, but no the other way around (since inode block smaller than file extent)
    FREE_ID         = 8,
    GENERIC_ID      = 16    //for reading any kind of block
} blockType;

//determined by superBlock layout
#define maxFiles (BLOCKSIZE - 5) //superblock needs to store these indices, but its first 4 bytes are already taken

typedef struct superBlock superBlock;
struct superBlock{
    uint8_t blockID;    //stores blockType
    uint8_t magicNumber;   
    uint16_t padding;   //should always be empty bytes
    uint8_t numBlocks;  //how many blocks big is this disk? (includes the superBlock). So we know when to stop iterating over when scanning disk for more space
    uint8_t inodeIndices [maxFiles];   //with blockSize=256, this is 126.
} __attribute__((__packed__));



typedef struct timestamp {
    uint32_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} __attribute__((__packed__)) timestamp;

//for the readOnly field in inode block
typedef enum readFlag{
    readOnly    =1,
    readAndWrite=2
} readFlag;

typedef struct inodeBlock inodeBlock;
struct inodeBlock{
    uint8_t blockID;    //stores blockType
    uint8_t magicNumber;   
    uint8_t nextBlock;  //noLinkBlock=0 for no next block
    uint8_t padding;   //should always be empty byte
    char fileName[maxFileNameLength];  
    uint32_t fileSize;  //number of bytes stored in this file. Note some bytes in the last block may be unused as specified here

    //date and time info
    timestamp creationTime;
    timestamp accessTime;
    timestamp modificationTime;
    uint8_t readMode;   //marks a file as read only (disables writes)
} __attribute__((__packed__));

typedef struct freeBlock freeBlock;
struct freeBlock{
    uint8_t blockID;    //stores blockType
    uint8_t magicNumber;  
    uint8_t nextBlock;  //undefined
    uint8_t padding; 
} __attribute__((__packed__));

#define dataPerBlock (BLOCKSIZE-4)  //likewise, file extent block has 4 bytes of storing header, remaining store data

typedef struct fileExtentBlock fileExtentBlock;
struct fileExtentBlock{
    uint8_t blockID;    //stores blockType
    uint8_t magicNumber;   
    uint8_t nextBlock;
    uint8_t padding;
    uint8_t data [dataPerBlock];
} __attribute__((__packed__));

#define genericPadding (BLOCKSIZE-4)

//for reading any kind of block
typedef struct genericBlock genericBlock;
struct genericBlock{
    uint8_t blockID;    //stores blockType
    uint8_t magicNumber;  
    uint8_t nextBlock;  //undefined
    uint8_t padding; 
    uint8_t extraPadding[genericPadding];
} __attribute__((__packed__));

#endif