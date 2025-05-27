#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "libDisk.h"
#include "tinyFS.h"
#include "tinyFS_errno.h"
#define TEST_DISK "test.disk"
#define TEST_DISK2 "test2.disk"
#define TEST_BLOCK_NUM 2
#define BLOCK_CHAR_A 'A'
#define BLOCK_CHAR_B 'B'
//these three arrays are written into files, so that the files are clearly distinct and identifiable
char* Alphabet="abcdefghijklmnopqrstuvwxyz";
char* AB="AB";
char* numbers="123456789";

//fill block with string, repeated however many times to reach blockSize characters
void fillBlock(char *block, char* string, int blockSize) {
    int strLen=strlen(string);
    while(blockSize!=0){
        int copySize= strLen>blockSize? blockSize : strLen;
        memcpy(block, string, copySize);
        blockSize-=copySize;
        block+=copySize;
    }
}

//helper function used to show testing is working (by printing out file layout). Also showcases the working display fragments function (for extra credit a.)
void printBitMap(){
    int len;
    uint8_t* bitMap=tfs_displayFragments(&len);
    printf("fragMap:\n");
    if(bitMap==NULL){
        printf("display Err: %d", len);
        return;
    }
    for(int i=0;i<len;i++){
        printf(" %u ", bitMap[i]);
    }
    printf("\n");
    free(bitMap);
}

int main() {
    //BLOCKSIZE is slightly larger than the the amount of data fitting in each block (four to be precise). 
    //So file should overflow a bit, with a only a few characters on their list file.

    //the data that will be written for the 5 created files
    char AlphabetShort[BLOCKSIZE];
    char AlphabetLong[BLOCKSIZE*11];
    char numShort[BLOCKSIZE];
    char numLong[BLOCKSIZE*3];
    char ABShort[BLOCKSIZE];
    fillBlock(AlphabetShort, Alphabet, BLOCKSIZE);
    fillBlock(AlphabetLong, Alphabet, BLOCKSIZE*11);
    fillBlock(numShort, numbers, BLOCKSIZE);
    fillBlock(numLong, numbers, BLOCKSIZE*3);
    fillBlock(ABShort, AB, BLOCKSIZE);
    
    //test1: showing fragmentation on file system named fragD
    char* dirName= "fragD";
    tfs_mkfs(dirName, BLOCKSIZE*30);   //size can be anything up to maxBlockCount, for this impl, (255*BLOCKSIZE)
    tfs_mount(dirName); //mount fragD
    printf("starting directory is empty\n");
    printBitMap();

    //add files to cause fragmentation
    printf("\nadd two files, then write to them to cause some fragmentation\n");
    char* ABShort_Name="ABShort";
    char* numShort_Name="numShort";
    int ABShortFD=tfs_openFile(ABShort_Name);
    int numShortFD=tfs_openFile(numShort_Name);
    tfs_writeFile(ABShortFD, ABShort, BLOCKSIZE); //should just barely take up two blocks
    tfs_writeFile(numShortFD, numShort, BLOCKSIZE);   //this one as well
    //remove the first one (AB short)
    tfs_deleteFile(ABShortFD);
    printf("should now have: super 0 numS_Inode 0 0 numS numS numS 0 ...\n");
    printBitMap();       
    printf("the above map is correct! is shows the super bock, and the inode block and data blocks (2) of the numShortFile, which were split by the inode and data blocks of ABShort\n\n");

    
    // //add a new larger file, causing fragmentation:
    printf("add a larger file causing fragmentation before, in between, and after file 2\n Also note that these maps are being printed with the tfs_displayFragments function, which does not care about the FD of each file, the labeled nubmers are based solely on whichever file it encounters first, so in this case, the file that was 2 on the previous map is now 3\n");
    char* alphabetLong_Name="AlphLong";
    int alphabetLongFD=tfs_openFile(alphabetLong_Name);
    tfs_writeFile(alphabetLongFD, AlphabetLong, BLOCKSIZE*3);  

    printf("should now have: super alphLong_Inode numShortInode alphLong alphLong numS numS alphLong alphLong alphLong 0 0 0...");
    printBitMap();
    printf("\n");
    printf("lets remove file 2 (alphabet Long), which is well-mixed in between file 3 and the supernode, and then add small AB files until we get an error from the FS being full\n");
    tfs_deleteFile(alphabetLongFD);


    int spammedFDs [20]={0};
    char* uniqueNameGenOG="abcdefghijklmnopqrstuvwxyz123456789";    //let each of the names be chars from here, so these are all distinct files, and dont just keep overwriting eachother
    char* uniqueNameGen=uniqueNameGenOG;
    for(int i=0; ;i++){    //while there is still space, keep adding files
        spammedFDs[i]=tfs_openFile(uniqueNameGen++);
        if(tfs_writeFile(spammedFDs[i], ABShort, BLOCKSIZE) <0){
            break;
        }
    }
    printf("we now expect 1 2 3 2 2 3 3 4 4 4 5 5 5 6 ... \n");
    printBitMap();
    printf("that looks great!\n\nlets also show we can close and re-open files by closing two of the AB-files and then re-opening. \nThen we read the first 5 bytes from them: (look at the code to confirm I use closeFile here\n");
    uniqueNameGen=uniqueNameGenOG;   //reset namgeGen
    tfs_closeFile(spammedFDs[3]);
    tfs_closeFile(spammedFDs[0]);
    spammedFDs[3]=tfs_openFile(uniqueNameGen+3);
    spammedFDs[0]=tfs_openFile(uniqueNameGen);
    char tempBuffer1[6]={0};
    char* ptr1=tempBuffer1;
    char tempBuffer2[6]={0};
    char* ptr2=tempBuffer2;
    //read the first 5 bytes from each of these
    for(int i=0;i<5;i++){
        tfs_readByte(spammedFDs[0], ptr1++);
        tfs_readByte(spammedFDs[3], ptr2++);
    }
    printf("the resulting bytes we read are: %s and %s. Looking good!\n", tempBuffer1, tempBuffer2);
        
    printf("\nNow lets remove all the odd numbered files we added, and ad in one big one (alphabetLongFD, extra long this time) back to take their place\n");

    for(int i=0; spammedFDs[i]!=0;i+=2){
        if(tfs_deleteFile(spammedFDs[i])){
            printf("error\n");
            while(1);
        }
    }
    alphabetLongFD=tfs_openFile(alphabetLong_Name);
    tfs_writeFile(alphabetLongFD, AlphabetLong, BLOCKSIZE*11);
    printBitMap();
    printf("lookin good! We see block two streching across the entire fileSystem. \n\nLets seal the deal, by reading all of file2, byte-by-byte (using readByte), and confirming, it does still spell out the alphabet:\n");

    printf("we will also unmount the FD, remount it, and then re-open this file to show that the data persists across closing and re-opening the FS and FD's:\n\n*closes Disk and re-opens file* <-look at the code to confirm this happens, sry Im not sure how to print this\n\n");
    tfs_unmount();
    tfs_mount(dirName);
    alphabetLongFD= tfs_openFile(alphabetLong_Name); //open new FD
    if(alphabetLongFD<0){
        printf("alphaBet open FD: %d\n",alphabetLongFD);
    }
        char alphabetLargeBuffer[BLOCKSIZE*11 +1]={0}; //ensure it the string is null-terminated by adding 1
    char* largePtr=alphabetLargeBuffer;
    for(int i=0;i<BLOCKSIZE*11;i++){
        int err = tfs_readByte(alphabetLongFD, largePtr);
        if (err != 0) {
            printf("Read failed at byte %d with err %d\n", i, err);
            break;
        }
        largePtr++;
    }
    printf("heres our file retrived by reptitively calling readByte(sorry if this fries ur computer): \n\n%s\n\n", alphabetLargeBuffer);
    printf("looking Good!\n\nNow, lets confirm we can correctly use the seek functoin by seeking to byte 5 of our file, we should see the byte printed here match the 5th char of the block printed above\n");
    tfs_seek(alphabetLongFD, 5);
    char readByte=0;
    tfs_readByte(alphabetLongFD, &readByte);
    printf("byte at index 5: %c", readByte);
    printf("\nThis concludes the verification of the basic functions supported by tinyFS\n");

    printf("\nnext, let move of to each of the extra credit parts I completed, parts a, b, d, and e, for 70+40 = 110%% total credit\n");

    printf("first, lets verify part (a), defragmentation.\n\n We have clearly already seen that displayFragments is working (by using it in all the previous parts)\n\n");

    printf("lets verify the use of defragment. As a reminder, here is our current FS: \n");

    printBitMap();
    printf("\n lets quickly remove some of the files, so we have something to defragment\n");
    int err;
    if((err=tfs_deleteFile(spammedFDs[1]))){printf("err%d\n", err);}
    if((err=tfs_deleteFile(spammedFDs[5]))){printf("err%d\n", err);}
    printf("I forgor I unmounted the file system, but this shows that I cant deleteFiles that havent been opened, and that some of the error codes make sense, in this case, -4= ENOENT, for now entered file, as seen in tinyFS_errno.h\n");
    printf("lets re-open the files, and then delete them correctly this time: (still checking that there is no errors)\n");
    uniqueNameGen=uniqueNameGenOG;
    //recall that the even ones are already all closed, so lets close the odd ones:
    spammedFDs[1]=tfs_openFile(uniqueNameGen+1);
    spammedFDs[5]=tfs_openFile(uniqueNameGen+5);
    if((err=tfs_deleteFile(spammedFDs[1]))){printf("err%d\n", err);}
    if((err=tfs_deleteFile(spammedFDs[5]))){printf("err%d\n", err);}
    printf("here is our new FS:\n");
    printBitMap();
    printf("\ndefragmenetation yields:\n");
    tfs_defrag();   //defragment the currently mounted disk
    printBitMap();
    printf("A success! we see that the defragmentation had to move a jumble of both file extent blocks, and inodes to the left\n");
    
    printf("\nnext, we test part b and e together, lets run readdir to see what were currently working with:\n");
    
    tfs_readdir();
    printf("the first two names make sense, and were from the original test, the second two were my \"automatically\" generated ones, read off the alphabet\n");
    printf("\nlets use the functions from part e to print the information of each of these files, including creation, access, and modification time\n");
    //we need to re-open these file, since we unmounted and re-mounted earlier
    // alphabetLongFD= tfs_openFile(alphabetLong_Name);
    numShortFD= tfs_openFile(numShort_Name);
    spammedFDs[3]=tfs_openFile(uniqueNameGen+3);
    spammedFDs[7]=tfs_openFile(uniqueNameGen+7);

    //aquire and print each info
    int len;
    uint8_t* infoAlph=tfs_readFileInfo(alphabetLongFD, &len);
    tfs_printFileInfo(infoAlph);
    uint8_t* infoNum=tfs_readFileInfo(numShortFD, &len);
    tfs_printFileInfo(infoNum);
    uint8_t* infoSpam1=tfs_readFileInfo(spammedFDs[3], &len);
    tfs_printFileInfo(infoSpam1);
    uint8_t* infoSpam2=tfs_readFileInfo(spammedFDs[7], &len);
    tfs_printFileInfo(infoSpam2);

    printf("this looks good, before we move on to testing part b, which is renaming files, lets sleep for 3s so that the changes we make will cause the modification and accessTime to update\n");
    printf("we will also read one byte from each of the older files, numShort, and AlpLong, to see that there acess is update, but not their modification\n");
    sleep(3);
    printf("lets add a file named \"mochi\", and rename the wierd character ones to \"philip\" and \"nico\"\n");
    char* mochiName="mochi";
    fileDescriptor mochiFD= tfs_openFile(mochiName);
    char* philName="philip";
    char* nicoName="nico";

    if((err=tfs_rename(spammedFDs[3], philName))){printf("err: %d\n", err);};    //if you look back through what was deleted, these are indeed the remaing "spammed FDs"
    tfs_rename(spammedFDs[7], nicoName);
    tfs_readdir();
    printf("\nWe can see that tfs_readdir reflects the new changes correctly.\n");
    printf("we also print the info to verify that part (e) is working, since we will see the updated modification and access times\n");
    //free our old info
    free(infoAlph);
    free(infoNum);
    free(infoSpam1);
    free(infoSpam2);
    char buff[1];    //a buffer to take in the byte, so that we update access time
    tfs_readByte(alphabetLongFD, buff);
    tfs_readByte(numShortFD, buff);
    infoAlph=tfs_readFileInfo(alphabetLongFD, &len);
    tfs_printFileInfo(infoAlph);
    infoNum=tfs_readFileInfo(numShortFD, &len);
    tfs_printFileInfo(infoNum);

    infoSpam1=tfs_readFileInfo(spammedFDs[3], &len);
    tfs_printFileInfo(infoSpam1);
    infoSpam2=tfs_readFileInfo(spammedFDs[7], &len);
    tfs_printFileInfo(infoSpam2);
    uint8_t* infoMochi=tfs_readFileInfo(mochiFD, &len);
    tfs_printFileInfo(infoMochi);
    printf("everything here looks good! This concludes the verification of parts b and e.\n");
    printf("\nlets move on to part d, which is toggling r/w permission, and a writeByte function\n");
    printf("first, we disable writing \"for nico\". We will then attempt both a writeFile (replacing the whole file with 3 blocks of the alphabet");
    printf("Lets foloow this with an lseek to near the end (lets pick 5th to last byte) of each file (philip and nico), where we then attempt to perform a writeByte, of character 'A'\n");
    printf("then, when printing all the bytes for each file, we should expect nico to still be ABABAB, and philip, sould now be 3 blocks of numbers 1-9, with one A inserted near the end\n");
    if(tfs_makeRO(nicoName)){  //make Nico read only
        printf("error making nico RO\n\n");
    }
    printf("*changed Nico to RO\n");
    //perform the same action on both files
    printf("errW?: %d\n", tfs_writeFile(spammedFDs[3], numLong, BLOCKSIZE*3));
    printf("errW?: %d\n", tfs_writeFile(spammedFDs[7], numLong, BLOCKSIZE*3));
    tfs_seek(spammedFDs[3], BLOCKSIZE*3-5);
    tfs_seek(spammedFDs[7], BLOCKSIZE*3-5);
    printf("errW?: %d\n", tfs_writeByte(spammedFDs[3], 'A'));
    printf("errW?: %d\n", tfs_writeByte(spammedFDs[7], 'A'));
    //return to the start of each file:
    tfs_seek(spammedFDs[3], 0);
    tfs_seek(spammedFDs[7], 0);
    
    printf("reading nico, which was read only\n");
    char nicoBuffer[BLOCKSIZE*3 +1]={0}; //ensure it the string is null-terminated by adding 1
    char* nicoPtr=nicoBuffer;
    for(int i=0;i<BLOCKSIZE*3;i++){
        int err = tfs_readByte(spammedFDs[7], nicoPtr);
        if (err != 0) {
            printf("Read failed at byte %d with err %d\n", i, err);
            break;
        }
        nicoPtr++;
    }

    printf("Nico reads: \n%s\n", nicoBuffer);

    printf("reading philip\n");
    char philipBuffer[BLOCKSIZE*3 +1]={0}; //ensure it the string is null-terminated by adding 1
    char* philipPtr=philipBuffer;
    for(int i=0;i<BLOCKSIZE*3;i++){
        int err = tfs_readByte(spammedFDs[3], philipPtr);
        if (err != 0) {
            printf("Read failed at byte %d with err %d\n", i, err);
            break;
        }
        philipPtr++;
    }
    printf("Philip reads: \n%s\n", philipBuffer);

    printf("\nas you can see above, we recieved error code=9 (which is EPERM=permission denied) on both writes to the second file (the NICO file), which had its write permission turned of\n\n");
    printf("the read failed at byte 257 for NICO was because we never re-wrote the file, so its size was only BLOCKSIZE, but we tried to print until BLOCKSIZE*3 bytes, so we get an ERANGE=-2 error\n\n");
    printf("as expected, the nico file did not update, but the philip file did update! Also, if you look a the 5th to last character of the philip file, you see the A that we wrote using writeByte\n\n");
    printf("also note- All the extra credit status stuff (read-write status, and the time status) are only stored on the hard disk (not in open files), so there is no need to close or unmount and re-open things and do it again, like we did with the main test cases\n");
    printf("^you can view the struct of an open file in tinyFSOpenList.h to confirm this, or test it yourself ig idk?\n");
    printf("\nThis concludes the the testing of my tinyFS, its not realistic to show all possible cases, so pls take a look at the code, or run a few of your own tests if you are not satisfied\n");
    
    return 0;
}
