
#ifndef TINYFS_ERRNO_H
#define TINYFS_ERRNO_H

typedef enum {
    ENOSPC         = -1,     //not enough space on the disk. can be triggered during creation, or when trying to add new files, if there isnt enough space
    ERANGE         = -2,     //value is outside of the range of bytes stored on the file .
    EIO            = -3,     //error with opening, reading, writing, or closing a file at "hardware" level (in our case, this is open(), read(), write(), or close() errors)
    ENOENT         = -4,     //file or disk not found
    EALREADY       =-5,      //a file system is already mounted
    ENODEV         =-6,      //no device is currently mounted
    ECORRUPT       =-7,       //invalid state of file system detected (like incorrect magic number)
    EPARAM         =-8,       //invalid paramets, for example, passing NULL when NULL not acceptable
    EPERM          =-9       //invalid permisions (tried write in read only mode)
} tinyFS_errno;

#endif