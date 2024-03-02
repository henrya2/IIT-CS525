#include "buffer_mgr.h"
#include "buffer_mgr_stat.h"
#include "storage_mgr.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define OP_NEW_PIN_FRAME 0
#define OP_EXIST_PIN_FRAME 1

int evictPageFrame_FIFOLRU(BM_BufferPool* const bm);

typedef struct CircularBuffer {
    int* data;
    int head;
    int tail;

    int numElements;
} CircularBuffer;

CircularBuffer* createCircularBuffer(int numElements)
{
    CircularBuffer* buffer = (CircularBuffer*)malloc(sizeof(CircularBuffer));
    buffer->data = (int*)malloc(sizeof(int) * (numElements + 1));
    buffer->head = 0;
    buffer->tail = 0;
    buffer->numElements = numElements;

    return buffer;
}

void destroyCircularBuffer(CircularBuffer* buffer)
{
    free(buffer->data);

    free(buffer);
}

bool isEmptyCircularBuffer(CircularBuffer* buffer)
{
    return buffer->head == buffer->tail;
}

bool isFullCircularBuffer(CircularBuffer* buffer)
{
    int next = (buffer->head + 1) % buffer->numElements;
    
    return next == buffer->tail;
}

int pushValueCircularBuffer(CircularBuffer* buffer, int value)
{
    int next;

    next = (buffer->head + 1) % buffer->numElements;

    if (next == buffer->tail)
        return -1;

    buffer->data[buffer->head] = value;
    buffer->head = next;
    return 0;
}

int popValueCircularBuffer(CircularBuffer* buffer, int* value)
{
    int next;

    if (buffer->head == buffer->tail)
        return -1;

    next = (buffer->tail + 1) % buffer->numElements;

    *value = buffer->data[buffer->tail];
    buffer->tail = next;
    return 0;
}

typedef struct FIFOLRLUStruct {
    unsigned* frameTime;
    unsigned lastTime;
} FIFOLRLUStruct;

FIFOLRLUStruct* createFIFOLRLUStruct(int num)
{
    FIFOLRLUStruct* attrib = (FIFOLRLUStruct*)malloc(sizeof(FIFOLRLUStruct));

    attrib->frameTime = (unsigned*)malloc(sizeof(unsigned) * num);
    attrib->lastTime = 0;

    return attrib;
}

void destroyFIFOLRLUStruct(FIFOLRLUStruct* attrib)
{
    free(attrib->frameTime);
    free(attrib);
}

typedef struct BufferPoolInternal {
    SM_FileHandle* pageFileHandle;

    union
    {
        FIFOLRLUStruct* attrib;
        int k;
    } strategyInternal;

    PageNumber* pages;
    bool* dirtyflags;
    int* fixCounts;

    char** pageData;

    int numReadIO;
    int numWriteIO;

    int numPages;
} BufferPoolInternal;

BufferPoolInternal* createBufferPoolInternal()
{
    BufferPoolInternal* poolInernal = (BufferPoolInternal*)malloc(sizeof(BufferPoolInternal));

    return poolInernal;
}

void initBufferPoolInternal(BufferPoolInternal* poolInternal, const int numPages, ReplacementStrategy strategy, void *stratData)
{
    poolInternal->pageFileHandle = (SM_FileHandle*)malloc(sizeof(SM_FileHandle));

    poolInternal->strategyInternal.attrib = NULL;
    poolInternal->strategyInternal.k = (int)(long long)stratData;

    if (strategy == RS_FIFO || strategy == RS_LRU || strategy == RS_LRU_K)
    {
        poolInternal->strategyInternal.attrib = createFIFOLRLUStruct(numPages);
    }

    poolInternal->pageData = (char**)malloc(sizeof(char**) * numPages);
    for (int i = 0; i < numPages; ++i)
    {
        poolInternal->pageData[i] = (char*)malloc(sizeof(char) * PAGE_SIZE);
        memset(poolInternal->pageData[i], 0, sizeof(char) * PAGE_SIZE);
    }

    poolInternal->pages = (PageNumber*)malloc(sizeof(PageNumber) * numPages);
    memset(poolInternal->pages, -1, sizeof(PageNumber) * numPages);
    poolInternal->dirtyflags = (bool*)malloc(sizeof(bool) * numPages);
    memset(poolInternal->dirtyflags, 0, sizeof(bool) * numPages);
    poolInternal->fixCounts = (int*)malloc(sizeof(int) * numPages);
    memset(poolInternal->fixCounts, 0, sizeof(int) * numPages);

    poolInternal->numReadIO = 0;
    poolInternal->numWriteIO = 0;

    poolInternal->numPages = numPages;
}

void destroyBufferPoolInternal(BM_BufferPool *const bm, BufferPoolInternal* poolInternal)
{
    closePageFile(poolInternal->pageFileHandle);

    for (int i = 0; i < poolInternal->numPages; ++i)
    {
        free(poolInternal->pageData[i]);
    }
    free(poolInternal->pageData);

    if (bm->strategy == RS_FIFO || bm->strategy == RS_LRU || bm->strategy == RS_LRU_K)
    {
        destroyFIFOLRLUStruct(poolInternal->strategyInternal.attrib);
    }

    free(poolInternal->pageFileHandle);
    free(poolInternal->pages);
    free(poolInternal->dirtyflags);
    free(poolInternal->fixCounts);

    free(poolInternal);
}

int flushPage(BM_BufferPool* const bm, int frameNum)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    int pageNum = poolInternal->pages[frameNum];
    char* memPage = poolInternal->pageData[frameNum];
    RC rc = writeBlock(pageNum, poolInternal->pageFileHandle, memPage);
    if (rc != 0)
    {
        return rc;
    }
    poolInternal->numWriteIO += 1;

    poolInternal->dirtyflags[frameNum] = false;

    return RC_OK;
}

int evictPageFrame_FIFOLRU(BM_BufferPool* const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    int minIndex = -1;
    unsigned minTime = UINT_MAX;

    int* fixCounts = poolInternal->fixCounts;
    bool* dirtyFlags = poolInternal->dirtyflags;
    FIFOLRLUStruct* attrib = poolInternal->strategyInternal.attrib;

    for (int i = 0; i < poolInternal->numPages; ++i)
    {
        if (fixCounts[i] > 0)
            continue;

        if (attrib->frameTime[i] < minTime)
        {
            minIndex = i;
            minTime = attrib->frameTime[i];
        }
    }

    if (minIndex >= 0)
    {
        if (dirtyFlags[minIndex])
        {
            flushPage(bm, minIndex);
        }
    }

    return minIndex;
}

int evictPageFrame(BM_BufferPool* const bm)
{
    if (bm->strategy == RS_FIFO || bm->strategy == RS_LRU || bm->strategy == RS_LRU_K)
    {
        return evictPageFrame_FIFOLRU(bm);
    }

    return -1;
}

void updateStrategyAttribPin(BM_BufferPool* const bm, int frameNum, int op)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;

    FIFOLRLUStruct* FIFOLRUattrib = poolInternal->strategyInternal.attrib;

    if (op == OP_NEW_PIN_FRAME)
    {
        if (bm->strategy == RS_FIFO || bm->strategy == RS_LRU || bm->strategy == RS_LRU_K)
        {
            FIFOLRUattrib->frameTime[frameNum] = FIFOLRUattrib->lastTime;
            FIFOLRUattrib->lastTime +=1;
        }
    }
    else if (op == OP_EXIST_PIN_FRAME)
    {
        if (bm->strategy == RS_LRU || bm->strategy == RS_LRU_K)
        {
            FIFOLRUattrib->frameTime[frameNum] = FIFOLRUattrib->lastTime;
            FIFOLRUattrib->lastTime +=1;            
        }
    }
}

int findPageFrame(BM_BufferPool* const bm, int pageNum)
{
    int frameNum = -1;
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;

    for (int i = 0; i < bm->numPages; ++i)
    {
        if (poolInternal->pages[i] == pageNum)
        {
            frameNum = i;
            break;
        }
    }

    return frameNum;
}

int readPageBufferBlock(BM_BufferPool* const bm, int frameNum)
{
    RC rc = RC_OK;
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;

    rc = readBlock(poolInternal->pages[frameNum], poolInternal->pageFileHandle, poolInternal->pageData[frameNum]);
    poolInternal->numReadIO += 1;

    return rc;
}

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData)
{
    bm->pageFile = (char * )pageFileName;
    bm->numPages = numPages;
    bm->strategy = strategy;
    BufferPoolInternal* poolInternal = createBufferPoolInternal();
    initBufferPoolInternal(poolInternal, numPages, strategy, stratData);
    bm->mgmtData = poolInternal;

    RC rc = openPageFile(bm->pageFile, poolInternal->pageFileHandle);
    if (rc != RC_OK)
    {
        destroyBufferPoolInternal(bm, poolInternal);
        bm->mgmtData = NULL;
    }

    return rc;
}

RC shutdownBufferPool(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return RC_FILE_HANDLE_NOT_INIT;

    forceFlushPool(bm);
    
    destroyBufferPoolInternal(bm, poolInternal);
    return RC_OK;
}

RC forceFlushPool(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return RC_FILE_HANDLE_NOT_INIT;
    
    RC rc = RC_OK;
    
    for (int i = 0; i < bm->numPages; ++i)
    {
        if (poolInternal->fixCounts[i] != 0)
            continue;

        if (poolInternal->pages[i] == -1)
            continue;

        if (poolInternal->dirtyflags[i])
        {
            rc = flushPage(bm, i);
            if (rc != RC_OK)
            {
                break;
            }
        }
    }

    return rc;
}

RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return RC_FILE_HANDLE_NOT_INIT;

    if (page == NULL)
        return RC_FILE_HANDLE_NOT_INIT;        

    int frameNum = findPageFrame(bm, page->pageNum);
    if (frameNum >= 0)
    {
        poolInternal->dirtyflags[frameNum] = true;
    }
    return (frameNum >= 0 ? RC_OK : -1);
}

RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return RC_FILE_HANDLE_NOT_INIT;

    if (page == NULL)
        return RC_FILE_HANDLE_NOT_INIT;

    int frameNum = findPageFrame(bm, page->pageNum);
    if (frameNum >= 0)
    {
        if ((poolInternal->fixCounts[frameNum] - 1) < 0)
            return RC_OK;
        
        poolInternal->fixCounts[frameNum] -= 1;
    }
    return (frameNum >= 0 ? RC_OK : -1);
}

RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return RC_FILE_HANDLE_NOT_INIT;

    if (page == NULL)
        return RC_FILE_HANDLE_NOT_INIT;        
        
    int frameNum = findPageFrame(bm, page->pageNum);
    if (frameNum >= 0)
    {
        flushPage(bm, frameNum);
    }
    return (frameNum >= 0 ? RC_OK : -1);
}

RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
    RC rc = RC_OK;
    if (pageNum < 0)
        return RC_READ_NON_EXISTING_PAGE;

    if (page == NULL)
        return RC_FILE_HANDLE_NOT_INIT;        

    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return RC_FILE_HANDLE_NOT_INIT;

    int opCode = OP_NEW_PIN_FRAME;
        
    bool needReadBlock = false;
    int frameNum = -1;
    // First find existing page data
    frameNum = findPageFrame(bm, pageNum);

    if (frameNum >= 0)
    {
        opCode = OP_EXIST_PIN_FRAME;
    }

    if (frameNum < 0)
    {
        // Then find an empty page
        for (int i = 0; i < bm->numPages; ++i)
        {
            if (poolInternal->pages[i] == -1)
            {
                frameNum = i;
                needReadBlock = true;
                opCode = OP_NEW_PIN_FRAME;
                break;
            }
        }
    }

    if (frameNum < 0)
    {
        // try to evit a page
        frameNum = evictPageFrame(bm);
        if (frameNum >= 0)
        {
            opCode = OP_NEW_PIN_FRAME;
            needReadBlock = true;
        }
    }

    if (frameNum >= 0)
    {
        page->data = poolInternal->pageData[frameNum];
        page->pageNum = pageNum;

        poolInternal->fixCounts[frameNum] += 1;

        poolInternal->pages[frameNum] = pageNum; // Occupy the buffer frame

        if (needReadBlock)
        {
            readPageBufferBlock(bm, frameNum);
        }        

        updateStrategyAttribPin(bm, frameNum, opCode);
    }
    else
    {
        rc = RC_IM_NO_MORE_ENTRIES;
    }

    return rc;
}

PageNumber *getFrameContents(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return NULL;

    return poolInternal->pages;
}

bool *getDirtyFlags(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return NULL;

    return poolInternal->dirtyflags;
}

int *getFixCounts(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return NULL;

    return poolInternal->fixCounts;
}

int getNumReadIO(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return 0;

    return poolInternal->numReadIO;
}

int getNumWriteIO(BM_BufferPool *const bm)
{
    BufferPoolInternal* poolInternal = (BufferPoolInternal*)bm->mgmtData;
    if (poolInternal == NULL)
        return 0;

    return poolInternal->numWriteIO;
}
