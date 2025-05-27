#ifndef TINYFS_H
#define TINYFS_H

#include <stdint.h>
// #include "tinyFSExtraFeatures.h"  //extra feature are specified in a seperate header

/* The default size of the disk and file system block */
#define BLOCKSIZE 256

// /* Default size=40 blocks total*/
#define DEFAULT_DISK_SIZE (40 * BLOCKSIZE)

/* Use this name for a default emulated disk file name */
#define DEFAULT_DISK_NAME "tinyFSDisk"

#define MAGIC_NUMBER 68 //0x44 is the magic number

#define maxFileNameLength 8

//instructions suggest that the address of a block can fit inside of one
//byte, so maxNumber of blocks is 255, meaning we have a max disk size of
//255 blocks*BLOCKSIZE. A max disk size isn't specified anywhere, but this seem like a 
//reasonalbe parameter
#define noBlockLink 0

#define maxBlockCount 255   //0 specifies noBlockLink, 1-255 specify block index

#define maxDiskSize (BLOCKSIZE*maxBlockCount)

/* Use as a special type to keep track of files */
typedef int fileDescriptor;

/* TinyFS API function declarations */
int tfs_mkfs(char *filename, int nBytes);
int tfs_mount(char *diskname);
int tfs_unmount(void);
fileDescriptor tfs_openFile(char *name);
int tfs_closeFile(fileDescriptor FD);
int tfs_writeFile(fileDescriptor FD, char *buffer, int size);
int tfs_deleteFile(fileDescriptor FD);
int tfs_readByte(fileDescriptor FD, char *buffer);
int tfs_seek(fileDescriptor FD, int offset);


// --- Part a: Fragmentation Info and Defragmentation ---

// Displays a list of all disk blocks
uint8_t* tfs_displayFragments(int* len);

// Defragments the file system by moving data blocks
// such that all free blocks are contiguous at the end.
int tfs_defrag();


// --- Part b: Directory Listing and File Renaming ---

// Renames an open file to a new name.
int tfs_rename(int fileDescriptor, char* newName);

// Lists all files in the directory
int tfs_readdir();


// --- Part d: Read-only Support and Write Byte ---
// Makes the file read-only (disables writing/deleting).
int tfs_makeRO(char* name);

// Makes the file read-write (enables writing/deleting).
int tfs_makeRW(char* name);

// Writes one byte to a specific offset in the file.
int tfs_writeByteOffset(int fileDescriptor, int offset, unsigned int data);

// Writes one byte to the current file position.
int tfs_writeByte(int fileDescriptor, uint8_t data);


// --- Part e: Timestamps --- 

// Returns or prints info like creation, modification, and access times.
// You may implement this to return a struct or just print to stdout.
uint8_t* tfs_readFileInfo(int fileDescriptor, int* len);
void tfs_printFileInfo(uint8_t* inodeInfo);

#endif // TINYFS_H