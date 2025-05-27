#include <stdio.h>

#include "diskList.h"
#include "unistd.h"

struct arrayList* array_list_new(){ //create a new arraylist of size 10. return points to the create arrayList struct
    struct arrayList *newList = malloc(sizeof(struct arrayList));
    newList->length=0;
    newList->capacity=10;
    newList->arr=malloc(sizeof(arrLType) * (newList->capacity));
    return newList;
}

//append an element to the end of the arraylist
//made to be custom for libDisk, taking in the two wanted parameters
void arrayList_add(struct arrayList* arrL, int diskFd, int diskSize){
    if(arrL->capacity == arrL->length){
        arrL->capacity *= 2;  // double the size of the list if it is full to make space
        arrL->arr = realloc(arrL->arr, sizeof(arrLType) * arrL->capacity);
    }

    arrLType newDisk;
    newDisk.diskFd = diskFd;
    newDisk.diskSize = diskSize;

    arrL->arr[arrL->length] = newDisk;
    arrL->length++;
}



void arrayList_free(struct arrayList* arrL){
    if (arrL) {
        free(arrL->arr);  // free the internal array
        free(arrL);       // free the struct itself
    }
}

//Functions specific to the Disk struct

//returns the matching struct based on provided fd
Disk* arrayList_find_by_fd(struct arrayList* arrL, int fd){
    for (int i = 0; i < arrL->length; i++) {
        if (arrL->arr[i].diskFd == fd) {
            return &(arrL->arr[i]);
        }
    }
    return NULL;
}

//removes a disk (for disk close)
int arrayList_remove_by_fd(struct arrayList* arrL, int fd){
    for (int i = 0; i < arrL->length; i++) {
        if (arrL->arr[i].diskFd == fd) {
            // Shift remaining elements left
            for (int j = i; j < arrL->length - 1; j++) {
                arrL->arr[j] = arrL->arr[j + 1];
            }
            arrL->length--;
            close(fd);
            return 0;  // success
        }
    }
    return -1;  // fd not found
}
