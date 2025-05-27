#ifndef pp 
#define pp
#include <stdlib.h>
#include "libDisk.h"
#include "tinyFS.h"

typedef struct tinyOpenFile tinyOpenFile;

//the user's fd of this file will be its index in the arrayList
struct tinyOpenFile{
    fileDescriptor fd;
    int32_t location;   //byte location of reading/writing ptr within this file, starts at 0
    int32_t inodeLinkIndex;
    uint8_t nextBlockIndex; //stores the block that this inode points to.
    int32_t fileSize;   //size of the file in bytes, retrieved from the inode upon opening. starts at 0 when new file created.
    char fileName [maxFileNameLength];   //name of disk. for length<8, all unused bytes are null, so can use memcmp. retrieved from the inode upon opening
    tinyOpenFile* next;
};

#define LLType tinyOpenFile

typedef struct LinkedList LL;

struct LinkedList{
    int32_t currentFD;  //starts at 1
    tinyOpenFile* head;
};

int opendFDListInitialized();  //indicate if Reset has been called yet

void openFDs_Reset();  //allocates the arraylist on first call. resets everything if called again. (free all allocated tinyOpenFiles)

int32_t openFDs_add(int32_t fileSize, char* fileName, int inodeLinkIndex, uint8_t nextBlockIndex);  //returns the fd of the added node. each fd is one greater than the last added fd
tinyOpenFile* openFDs_searchByName(char* targetName);   //returns fd of matching name, otherwise NULL
tinyOpenFile* openFDs_searchByFD(fileDescriptor targetFD);    //returns the nextBlockIndex matching the FD, otherwise -1 on failure.

int openFDs_removeByFD(int32_t targetFD);   //removes target FD. 0 on success, -1 on failure. if arrayList capacity/2.2 <length, resizes arrayList to make it the smaller size once again
//functions specific to diskLib

#endif
