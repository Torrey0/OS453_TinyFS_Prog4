
#ifndef LIBDISK_SAFE_H
#define LIBDISK_SAFE_H

#include "tinyFSStructs.h"
#include <stdint.h>

//adds checks ensuring the blocktype matches what is expected, that magicNumber is not changed, and that padding remains 0
int safeReadBlock(int disk, int bNum, void *block, blockType type, uint8_t magicNumber);
int safeWriteBlock(int disk, int bNum, void *block, blockType type, uint8_t magicNumber);


#endif