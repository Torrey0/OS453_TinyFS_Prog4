#include <stdlib.h>
#include <string.h>
#include "tinyFS.h"
#include "libDisk.h"
#include "tinyFSOpenList.h"

static LL* openFDList = NULL;
int opendFDListInitialized(){
    return openFDList!=NULL;    //indicate if the FDList has been initialized
}
void openFDs_Reset() {
    if (openFDList) {
        tinyOpenFile* curr = openFDList->head;
        while (curr) {
            tinyOpenFile* temp = curr;
            curr = curr->next;
            free(temp);
        }
        free(openFDList);
    }

    openFDList = malloc(sizeof(LL));
    if (!openFDList) return;
    openFDList->head = NULL;
    openFDList->currentFD = 1;
}

fileDescriptor openFDs_add(int32_t fileSize, char* fileName, int inodeLinkIndex, uint8_t nextBlockIndex) {
    if (!openFDList) return -1;

    tinyOpenFile* newNode = malloc(sizeof(tinyOpenFile));
    if (!newNode) return -1;

    newNode->fd = openFDList->currentFD++;
    newNode->location = 0;
    newNode->fileSize = fileSize;
    newNode->inodeLinkIndex=inodeLinkIndex;
    newNode->nextBlockIndex=nextBlockIndex;
    memset(newNode->fileName, 0, maxFileNameLength);
    memcpy(newNode->fileName, fileName, maxFileNameLength);
    newNode->next = openFDList->head;
    openFDList->head = newNode;

    return newNode->fd;
}

tinyOpenFile* openFDs_searchByName(char* targetName) {
    if (!openFDList) return NULL;

    tinyOpenFile* curr = openFDList->head;
    while (curr) {
        if (memcmp(curr->fileName, targetName, maxFileNameLength) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

//returns the openFile for the corresponding inode so we can quickly go to it on disk
tinyOpenFile* openFDs_searchByFD(fileDescriptor targetFD){
    if (!openFDList) return NULL;
    tinyOpenFile* curr = openFDList->head;
    while (curr) {
        if(curr->fd ==targetFD){    //we found a match
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

int openFDs_removeByFD(int32_t targetFD) {
    if (!openFDList) return -1;

    tinyOpenFile* curr = openFDList->head;
    tinyOpenFile* prev = NULL;

    while (curr) {
        if (curr->fd == targetFD) {
            if (prev) {
                prev->next = curr->next;
            } else {
                openFDList->head = curr->next;
            }
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    return -1;
}
