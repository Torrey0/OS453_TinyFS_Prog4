#ifndef TINYFS_EXTRAS_H
#define TINYFS_EXTRAS_H
#include "tinyFS.h"
#include "tinyFSOpenList.h"

//add-on a
uint8_t* tfs_displayFragments_a(int* len, int targetDisk);
int tfs_defrag_a(int targetDisk);
//

//add-on b
int tfs_readdir_b(int mountedDiskFD);
int tfs_rename_b(int mountedDiskFD, fileDescriptor fd, char* newName);
//

//add-on d
int checkPermission(int targetDisk, tinyOpenFile* file);

int tfs_makeRW_d(int targetDisk, char* name);

int tfs_makeRO_d(int targetDisk, char* name);
//writes at current location of filePtr
int tfs_writeByte_d(int targetDisk, fileDescriptor fd, uint8_t byte);
//

//add-on e
//returns the raw data stored at inode. dynamically allocated uint8_t array
uint8_t* tfs_readFileInfo_e(int mountedDiskFD, fileDescriptor fd, int* len);

//I provide a function that can be used to print this information nicely (so we can also verify timestamps)
void tfs_printFileInfo_e(uint8_t* inodeInfo);
//
#endif