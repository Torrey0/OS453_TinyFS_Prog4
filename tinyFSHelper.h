#ifndef TINYFS_HELPER_H
#define TINYFS_HELPER_H

#include "tinyFSStructs.h"
#include "tinyFSOpenList.h"

//format the provided name so it match that of the file system, where any bytes after the null byte up to 8 (if any) are zero
void formatName(char* name, char* formattedName);  //expects formatted name to be an array of length 8


//stuff for modifying inodes
int updateInode(tinyOpenFile* file, uint8_t nextBlock, int targetDisk, int size); //update the next block this inode points to, and the time it was last accessed, and its size

int updateInodeAccess(tinyOpenFile* file, int targetDisk); //just change the access time to the current time

int createInode(int inodeLinkIndex, char* inodeName, int targetDisk, superBlock* sb);

int removeInode(tinyOpenFile* file, int mountedDiskFD);
//
//stuff for searching
fileDescriptor searchSB(superBlock* sb, int targetDiskFD, char* formattedName, int32_t* startingFreeLink);

int findFreeBlock(int targetDiskFD, int targetDiskLength, int startingPosition);
//
//stuff for erasing
int setBlockFree(int targetBlock, int targetDiskFD);

int eraseFileData(tinyOpenFile* FD, int targetDisk);
//
int32_t getTargetDiskBlocks(int targetDisk);

//retrieves the current time as it is stored in an inode
void retrieveTime(timestamp* timeRecord);

#endif