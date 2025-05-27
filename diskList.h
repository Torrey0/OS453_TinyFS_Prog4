#ifndef pp 
#define pp
#include <stdlib.h>
#include "libDisk.h"
#define arrLType Disk



struct arrayList{
    int length;
    int capacity;
    arrLType* arr;
};

//generic arrayList
struct arrayList* array_list_new();

void arrayList_add(struct arrayList* arrL, int diskFd, int diskSize);
void arrayList_free(struct arrayList* arrL);

//functions specific to diskLib
Disk* arrayList_find_by_fd(struct arrayList* arrL, int fd);
int arrayList_remove_by_fd(struct arrayList* arrL, int fd);
#endif
