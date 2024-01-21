#include "storage_mgr.h"
#include "dberror.h"
#include <stdio.h>
#include <stdlib.h>

static char initPageContent[PAGE_SIZE] = {'\0'};

/* manipulating page files */
void initStorageManager (void)
{
    printf("Init Storage Manager\n");
}

RC createPageFile(char *fileName)
{
    FILE* fp = NULL;

    fp = fopen(fileName, "w");
    if (fp == NULL)
        return RC_WRITE_FAILED;

    if (fwrite(initPageContent, 1, PAGE_SIZE, fp) != PAGE_SIZE)
    {
        fclose(fp);
        remove(fileName);
        return RC_WRITE_FAILED;
    }
    
    fclose(fp);

    return RC_OK;
}

RC openPageFile(char *fileName, SM_FileHandle *fHandle)
{
    FILE* fp = NULL;

    if (fHandle == NULL)
    {
        return RC_FILE_HANDLE_NOT_INIT;
    }

    fp = fopen(fileName, "r+");
    if (fp == NULL)
    {
        return RC_FILE_NOT_FOUND;
    }

    if (fseek(fp, 0, SEEK_END) != 0)
    {
        fclose(fp);
        return RC_FILE_NOT_FOUND;
    }
    size_t fileSize = ftell(fp);
    fHandle->fileName = fileName;
    fHandle->curPagePos = 0;
    fHandle->totalNumPages = fileSize / PAGE_SIZE;
    fHandle->mgmtInfo = fp;

    return RC_OK;
}

RC closePageFile(SM_FileHandle *fHandle)
{
    if (!fHandle)
        return RC_FILE_HANDLE_NOT_INIT;
    
    if (fclose((FILE*)fHandle->mgmtInfo) != 0)
        return RC_FILE_HANDLE_NOT_INIT;

    fHandle->curPagePos = 0;
    fHandle->fileName = NULL;
    fHandle->mgmtInfo = NULL;
    fHandle->totalNumPages = 0;

    return RC_OK;
}

RC destroyPageFile(char *fileName)
{
    if (remove(fileName) != 0)
        return RC_FILE_NOT_FOUND;
    
    return RC_OK;
}

/* reading blocks from disc */

RC readBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_READ_NON_EXISTING_PAGE;

    int foff = pageNum * PAGE_SIZE;
    FILE* fp = (FILE*)fHandle->mgmtInfo;
    fseek(fp, foff, SEEK_SET);
    fread(memPage, 1, PAGE_SIZE, fp);
    fHandle->curPagePos = pageNum;

    return RC_OK;
}

int getBlockPos(SM_FileHandle *fHandle)
{
    return fHandle->curPagePos;;
}

RC readFirstBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int pageNum = 0;

    return readBlock(pageNum, fHandle, memPage);
}

RC readPreviousBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int pageNum = fHandle->curPagePos - 1;

    return readBlock(pageNum, fHandle, memPage);
}

RC readCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int pageNum = fHandle->curPagePos;

    return readBlock(pageNum, fHandle, memPage);
}

RC readNextBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int pageNum = fHandle->curPagePos + 1;

    return readBlock(pageNum, fHandle, memPage);
}

RC readLastBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int pageNum = fHandle->totalNumPages - 1;

    return readBlock(pageNum, fHandle, memPage);
}

/* writing blocks to a page file */

RC writeBlock(int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    if (pageNum < 0 || pageNum >= fHandle->totalNumPages)
        return RC_READ_NON_EXISTING_PAGE;

    int foff = pageNum * PAGE_SIZE;
    FILE* fp = (FILE*)fHandle->mgmtInfo;
    fseek(fp, foff, SEEK_SET);
    fwrite(memPage, 1, PAGE_SIZE, fp);
    fHandle->curPagePos = pageNum;

    return RC_OK;
}

RC writeCurrentBlock(SM_FileHandle *fHandle, SM_PageHandle memPage)
{
    int pageNum = fHandle->curPagePos;

    return writeBlock(pageNum, fHandle, memPage);
}

RC appendEmptyBlock(SM_FileHandle *fHandle)
{
    FILE* fp = (FILE*)fHandle->mgmtInfo;
    fseek(fp, 0, SEEK_END);
    fwrite(initPageContent, 1, PAGE_SIZE, fp);
    fHandle->totalNumPages += 1;

    return RC_OK;
}

RC ensureCapacity(int numberOfPages, SM_FileHandle *fHandle)
{
    int expandNumPages = numberOfPages - fHandle->totalNumPages;

    if (expandNumPages <= 0)
    {
        /* Do nothing if page number is large enough. */
        return RC_OK;
    }
    
    FILE* fp = (FILE*)fHandle->mgmtInfo;
    fseek(fp, 0, SEEK_END);
    fwrite(initPageContent, 1, PAGE_SIZE * expandNumPages, fp);
    fHandle->totalNumPages = numberOfPages;

    return RC_OK;
}
