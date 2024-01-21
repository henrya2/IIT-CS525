#include "storage_mgr.h"
#include <stdlib.h>
#include <stdio.h>

#define TEST_FILE_NAME "test1.bin"

void printFileHandle(SM_FileHandle* fHandle)
{
    if (!fHandle)   
        return;

    printf("SM_FileHandle: fileName: %s, totalNumPages: %d, curPagePos: %d, mgmtInfo: %0llx\n", fHandle->fileName, fHandle->totalNumPages, fHandle->curPagePos, (long long)fHandle->mgmtInfo);
}

int main()
{
    initStorageManager();
    SM_FileHandle* fHandle = (SM_FileHandle*)malloc(sizeof(SM_FileHandle));

    createPageFile(TEST_FILE_NAME);
    openPageFile(TEST_FILE_NAME, fHandle);
    printFileHandle(fHandle);

    free(fHandle);
    return 0;
}