#include "record_mgr.h"
#include "record_mgr_private.h"
#include "storage_mgr.h"
#include "buffer_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int getSchemaMetaBinSize(Schema *schema)
{
    int numAttr = schema->numAttr;
    int maxNameLen = 0;
    for (int i = 0; i < numAttr; ++i)
    {
        int nameLen = strlen(schema->attrNames[i]);
        if (maxNameLen < nameLen)
        {
            maxNameLen = nameLen;
        }
    }
    int maxNameSize = maxNameLen + 1;

    int binSize = 0;
    binSize += sizeof(schema->numAttr);  // numAttr
    binSize += sizeof(int); // maxNameSize
    binSize += sizeof(char) * maxNameSize * numAttr; // attrNames
    binSize += sizeof(DataType) * numAttr; // dataTypes
    binSize += sizeof(int) * numAttr; // typeLength
    binSize += sizeof(int) * schema->keySize; // keyAttrs
    binSize += sizeof(int); // keySize;

    return binSize;
}

/*
typedef struct Schema
{
	int numAttr;
	char **attrNames;
	DataType *dataTypes;
	int *typeLength;
	int *keyAttrs;
	int keySize;
} Schema;
*/

int readSchemaFromBuffer(Schema *schema, char *buffer)
{
    int cursor = 0;

    int numAttr = 0;
    int maxNameSize = 0;

    int accessSize = 0;
    
    accessSize = sizeof(schema->numAttr);
    memcpy(&schema->numAttr, buffer + cursor, accessSize);
    cursor += accessSize;
    numAttr = schema->numAttr;

    schema->attrNames = (char**)malloc(sizeof(char*) * numAttr);
    schema->dataTypes = (DataType*)malloc(sizeof(DataType) * numAttr);
    schema->typeLength = (int*)malloc(sizeof(int) * numAttr);

    accessSize = sizeof(maxNameSize);
    memcpy(&maxNameSize, buffer + cursor, accessSize);
    cursor += accessSize;

    for (int i = 0; i < numAttr; ++i)
    {
        schema->attrNames[i] = (char*)malloc(maxNameSize);
        accessSize = maxNameSize;
        strncpy(schema->attrNames[i], buffer + cursor, maxNameSize - 1);
        cursor += accessSize;
    }

    accessSize = numAttr * sizeof(*schema->dataTypes);
    memcpy(schema->dataTypes, buffer + cursor, accessSize);
    cursor += accessSize;

    accessSize = numAttr * sizeof(*schema->typeLength);
    memcpy(schema->typeLength, buffer + cursor, accessSize);
    cursor += accessSize;

    accessSize = sizeof(schema->keySize);
    memcpy(&schema->keySize, buffer + cursor, accessSize);
    cursor += accessSize;

    schema->keyAttrs = (int*)malloc(sizeof(int) * schema->keySize);
    accessSize = schema->keySize * sizeof(*schema->keyAttrs);
    memcpy(schema->keyAttrs, buffer + cursor, accessSize);
    cursor += accessSize;

    return cursor;
}

int writeSchemaToBuffer(Schema *schema, char *buffer)
{
    int cursor = 0;

    int numAttr = schema->numAttr;
    int maxNameLen = 0;
    for (int i = 0; i < numAttr; ++i)
    {
        int nameLen = strlen(schema->attrNames[i]);
        if (maxNameLen < nameLen)
        {
            maxNameLen = nameLen;
        }
    }
    int maxNameSize = maxNameLen + 1;

    int accessSize = 0;
    
    accessSize = sizeof(schema->numAttr);
    memcpy(buffer + cursor, &schema->numAttr, accessSize);
    cursor += accessSize;

    accessSize = sizeof(maxNameSize);
    memcpy(buffer + cursor, &maxNameSize, accessSize);
    cursor += accessSize;

    for (int i = 0; i < numAttr; ++i)
    {
        accessSize = maxNameSize;
        strcpy(buffer + cursor, schema->attrNames[i]);
        cursor += accessSize;
    }

    accessSize = numAttr * sizeof(*schema->dataTypes);
    memcpy(buffer + cursor, schema->dataTypes, accessSize);
    cursor += accessSize;

    accessSize = numAttr * sizeof(*schema->typeLength);
    memcpy(buffer + cursor, schema->typeLength, accessSize);
    cursor += accessSize;

    accessSize = sizeof(schema->keySize);
    memcpy(buffer + cursor, &schema->keySize, accessSize);
    cursor += accessSize;

    accessSize = schema->keySize * sizeof(*schema->keyAttrs);
    memcpy(buffer + cursor, schema->keyAttrs, accessSize);
    cursor += accessSize;

    return cursor;
}

RC updateTableMetaData(TableMgtInfo *mgtInfo)
{
    BM_PageHandle pageHandle;

    RC result = pinPage(mgtInfo->bufferPool, &pageHandle, 0);
    if (result != RC_OK)
    {
        return result;
    }
    char* metaBuffer = pageHandle.data;
    memcpy(metaBuffer, &mgtInfo->metaData, sizeof(mgtInfo->metaData));

    markDirty(mgtInfo->bufferPool, &pageHandle);
    unpinPage(mgtInfo->bufferPool, &pageHandle);

    return RC_OK;
}

void closeTableMgtInfo(TableMgtInfo *tableMgtInfo)
{
    shutdownBufferPool(tableMgtInfo->bufferPool);
    free(tableMgtInfo);
}

RC writeRecordToTable(TableMgtInfo *mgtInfo, Record *record)
{
    TableMetaData* metaData = &mgtInfo->metaData;

    BM_PageHandle pageHandle;

    int page = record->id.page;
    RC result = pinPage(mgtInfo->bufferPool, &pageHandle, page);
    if (result != RC_OK)
    {
        return result;
    }

    char* pageBuffer = pageHandle.data;
    int recordOffset = record->id.slot * metaData->recordSize;
    memcpy(pageBuffer + recordOffset, record->data, metaData->recordSize);

    result = markDirty(mgtInfo->bufferPool, &pageHandle);
    if (result != RC_OK)
    {
        return result;
    }    
    result = unpinPage(mgtInfo->bufferPool, &pageHandle);
    if (result != RC_OK)
    {
        return result;
    }    

    return RC_OK;
}

void closeScaneHandleMgtInfo(ScanHandleMgtInfo *mgtInfo)
{
    free(mgtInfo);
}

int* getRecordItemOffsets(Schema* schema)
{
    int* result = (int*)malloc(sizeof(int) * schema->numAttr);
    int offset = 0;
    for (int i = 0; i < schema->numAttr; ++i)
    {
        int valSize = getDataSize(schema, i);
        result[i] = offset;

        offset += valSize;
    }

    return result;
}

RC initRecordManager(void *mgmtData)
{
    return RC_OK;
}

RC shutdownRecordManager()
{
    return RC_OK;
}

RC createTable(char *name, Schema *schema)
{
    int metaSize = sizeof(TableMetaData);
    int schemaSize = getSchemaMetaBinSize(schema);
    char* pageBuffer = (char*)malloc(PAGE_SIZE);
    TableMetaData* metaData = (TableMetaData*) pageBuffer;

    metaData->lastFreePage = 1; // record data starts at page 1
    metaData->lastFreeSlot = 0;
    metaData->recordSize = getRecordSize(schema);
    metaData->recordSlotsPerPage = PAGE_SIZE / metaData->recordSize;
    metaData->schemaSize = schemaSize;

    char* shcemaBuffer = pageBuffer + metaSize;

    writeSchemaToBuffer(schema, shcemaBuffer);

    RC result = createPageFile(name);
    if (result != RC_OK)
    {
        return result;
    }

    SM_FileHandle fh;
    result = openPageFile(name, &fh);
    if (result != RC_OK)
    {
        return result;
    }

    result = writeBlock(0, &fh, pageBuffer);
    if (result != RC_OK)
    {
        closePageFile(&fh);
        return result;
    }

    closePageFile(&fh);

    free(pageBuffer);

    return RC_OK;
}

RC openTable(RM_TableData *rel, char *name)
{
    rel->name = name;
    rel->mgmtData = (TableMgtInfo*)malloc(sizeof(TableMgtInfo));
    rel->schema = (Schema*)malloc(sizeof(Schema));

    BM_PageHandle pageHandle;

    TableMgtInfo* mgtInfo = (TableMgtInfo*)rel->mgmtData;
    mgtInfo->bufferPool = (BM_BufferPool*)malloc(sizeof(BM_BufferPool));
    RC result = initBufferPool(mgtInfo->bufferPool, name, 32, RS_FIFO, NULL);
    if (result != RC_OK)
    {
        return result;
    }
    result = pinPage(mgtInfo->bufferPool, &pageHandle, 0);
    if (result != RC_OK)
    {
        return result;
    }
    char* pageBuffer = pageHandle.data;

    char* metaBuffer = pageBuffer;
    memcpy(&mgtInfo->metaData, metaBuffer, sizeof(mgtInfo->metaData));
    char* schemaBuffer = pageBuffer + sizeof(mgtInfo->metaData);
    readSchemaFromBuffer(rel->schema, schemaBuffer);

    unpinPage(mgtInfo->bufferPool, &pageHandle);

    for (int page = 0; page < 32; ++page)
    {
        for (int slot = 0; slot < 500; ++slot)
        {
            RID id;
            id.page = page;
            id.slot = slot;
            mgtInfo->pageSlotRemap[page][slot] = id;
        }
    }

    return RC_OK;
}

RC closeTable(RM_TableData *rel)
{
    closeTableMgtInfo((TableMgtInfo*)rel->mgmtData);
    freeSchema(rel->schema);

    return RC_OK;
}

RC deleteTable(char *name)
{
    destroyPageFile(name);
    return RC_OK;
}

int getNumTuples(RM_TableData *rel)
{
    TableMgtInfo* mgtInfo = (TableMgtInfo*)rel->mgmtData;
    TableMetaData* metaData = &mgtInfo->metaData;
    int page = metaData->lastFreePage;
    int slot = metaData->lastFreeSlot;

    int numTuples = page * metaData->recordSlotsPerPage + slot;

    return numTuples;
}

RC insertRecord(RM_TableData *rel, Record *record)
{
    TableMgtInfo* mgtInfo = (TableMgtInfo*)rel->mgmtData;
    TableMetaData* metaData = &mgtInfo->metaData;

    record->id.page = metaData->lastFreePage;
    record->id.slot = metaData->lastFreeSlot;

    RC result = writeRecordToTable(mgtInfo, record);
    if (result != RC_OK)
    {
        return result;
    }

    // advance to next free (page, slot)
    getNextPageSlot(metaData->lastFreePage, metaData->lastFreeSlot, &metaData->lastFreePage, &metaData->lastFreeSlot, metaData->recordSlotsPerPage);

    updateTableMetaData(mgtInfo); 

    return RC_OK;
}

RC deleteRecord(RM_TableData *rel, RID id)
{
    TableMgtInfo* mgtInfo = (TableMgtInfo*)rel->mgmtData;
    TableMetaData* metaData = &mgtInfo->metaData;
    id = mgtInfo->pageSlotRemap[id.page][id.slot];

    int globalSlot = (id.page * metaData->recordSlotsPerPage + id.slot) * metaData->recordSize;
    int freeGlobalSlot = (metaData->lastFreePage *  metaData->recordSlotsPerPage + metaData->lastFreeSlot) * metaData->recordSize;
    if (globalSlot >= freeGlobalSlot)
    {
        return RC_RM_NO_MORE_TUPLES;
    }
    BM_PageHandle pageHandle0;
    BM_PageHandle pageHandle1;
    RC result = pinPage(mgtInfo->bufferPool, &pageHandle0, id.page);
    if (result != RC_OK)
    {
        return result;
    }
    RID lastId;
    getPreviousValidPageSlot(metaData->lastFreePage, metaData->lastFreeSlot, &metaData->lastFreePage, &metaData->lastFreeSlot, metaData->recordSlotsPerPage);
    lastId.page = metaData->lastFreePage;
    lastId.slot = metaData->lastFreeSlot;
    result = pinPage(mgtInfo->bufferPool, &pageHandle1, lastId.page);
    if (result != RC_OK)
    {
        return result;
    }

    char* pageBuffer0 = pageHandle0.data;
    char* pageBuffer1 = pageHandle1.data;

    int rco0 = id.slot * metaData->recordSize;
    int rco1 = lastId.slot * metaData->recordSize;
    memmove(pageBuffer0 + rco0, pageBuffer1 + rco1, metaData->recordSize);

    markDirty(mgtInfo->bufferPool, &pageHandle0);

    unpinPage(mgtInfo->bufferPool, &pageHandle0);
    unpinPage(mgtInfo->bufferPool, &pageHandle1);

    updateTableMetaData(mgtInfo);
    if (id.page != lastId.page || id.slot != lastId.slot)
    {
        mgtInfo->pageSlotRemap[lastId.page][lastId.slot] = id;
    } 

    return RC_OK;
}

RC updateRecord(RM_TableData *rel, Record *record)
{
    TableMgtInfo* mgtInfo = (TableMgtInfo*)rel->mgmtData;
    RID id = record->id;
    record->id = mgtInfo->pageSlotRemap[id.page][id.slot];

    RC result = writeRecordToTable(mgtInfo, record);
    if (result != RC_OK)
    {
        return result;
    }

    return RC_OK;
}

RC getRecord(RM_TableData *rel, RID id, Record *record)
{
    record->id = id;
    TableMgtInfo* mgtInfo = (TableMgtInfo*)rel->mgmtData;
    TableMetaData* metaData = &mgtInfo->metaData;
    record->id = mgtInfo->pageSlotRemap[id.page][id.slot];
    id = record->id;

    int globalSlot = (id.page * metaData->recordSlotsPerPage + id.slot) * metaData->recordSize;
    int freeGlobalSlot = (metaData->lastFreePage * metaData->recordSlotsPerPage + metaData->lastFreeSlot) * metaData->recordSize;
    if (globalSlot >= freeGlobalSlot)
    {
        return RC_RM_NO_MORE_TUPLES;
    }
    BM_PageHandle pageHandle;
    RC result = pinPage(mgtInfo->bufferPool, &pageHandle, id.page);
    if (result != RC_OK)
    {
        return result;
    }

    char* pageBuffer = pageHandle.data;
    int recordOffset = id.slot * metaData->recordSize;
    memcpy(record->data, pageBuffer + recordOffset, metaData->recordSize);

    result = unpinPage(mgtInfo->bufferPool, &pageHandle);
    if (result != RC_OK)
    {
        return result;
    }    

    return RC_OK;
}

RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond)
{
    scan->rel = rel;
    ScanHandleMgtInfo* mgtInfo = (ScanHandleMgtInfo*)malloc(sizeof(ScanHandleMgtInfo));
    scan->mgmtData = mgtInfo;
    mgtInfo->expr = cond;
    mgtInfo->nextId.page = 1;
    mgtInfo->nextId.slot = 0;

    return RC_OK;
}

RC next(RM_ScanHandle *scan, Record *record)
{
    RC result = RC_OK;
    bool bFound = false;
    ScanHandleMgtInfo* mgtInfo = (ScanHandleMgtInfo*)scan->mgmtData;

    Value* valResult;

    TableMgtInfo* tableMgtInfo = (TableMgtInfo*)scan->rel->mgmtData;
    do
    {
        result = getRecord(scan->rel, mgtInfo->nextId, record);
        if (result != RC_OK)
        {
            return result;
        }
        result = evalExpr(record, scan->rel->schema, mgtInfo->expr, &valResult);
        if (result != RC_OK)
        {
            return result;
        }
        if (valResult->v.boolV)
        {
            bFound = true;
        }
        
        freeVal(valResult);
        getNextPageSlot(mgtInfo->nextId.page, mgtInfo->nextId.slot, &mgtInfo->nextId.page, &mgtInfo->nextId.slot, tableMgtInfo->metaData.recordSlotsPerPage);

    } while(!bFound);

    return RC_OK;
}

RC closeScan(RM_ScanHandle *scan)
{
    closeScaneHandleMgtInfo((ScanHandleMgtInfo*)scan->mgmtData);
    scan->rel = NULL;
    return RC_OK;
}

int getRecordSize(Schema *schema)
{
    if (!schema)
        return 0;

    int result = 0;
    for (int i = 0; i < schema->numAttr; ++i)
    {
        switch (schema->dataTypes[i])
        {
        case DT_INT:
            result += sizeof(int);
            break;
        case DT_FLOAT:
            result += sizeof(float);
            break;
        case DT_BOOL:
            result += sizeof(bool);
            break;
        case DT_STRING:
            result += schema->typeLength[i];
            break;

        default:
            break;
        }
    }

    return result;
}

Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes, int *typeLength, int keySize, int *keys)
{
    Schema* schema = (Schema*)malloc(sizeof(Schema));
    schema->numAttr = numAttr;
    schema->attrNames = attrNames;
    schema->dataTypes = dataTypes;
    schema->typeLength = typeLength;
    schema->keySize = keySize;
    schema->keyAttrs = keys;

    return schema;
}

RC freeSchema(Schema *schema)
{
    if (!schema)
        return -1;

    for (int i = 0; i < schema->numAttr; ++i)
    {
        free(schema->attrNames[i]);
    }
    free(schema->attrNames);
    free(schema->dataTypes);
    free(schema->typeLength);
    free(schema->keyAttrs);

    free(schema);

    return RC_OK;
}

RC createRecord(Record **record, Schema *schema)
{
    Record* tmpRecord = (Record*)malloc(sizeof(Record));

    tmpRecord->data = (char*)malloc(getRecordSize(schema));
    tmpRecord->id.page = -1; // Intialize to invalid value
    tmpRecord->id.slot = -1; // Intialize to invalid value
    *record = tmpRecord;

    return RC_OK;
}

RC freeRecord(Record *record)
{
    if (!record)
        return -1;
    free(record->data);
    free(record);

    return RC_OK;
}

RC getAttr(Record *record, Schema *schema, int attrNum, Value **value)
{
    if (attrNum >= schema->numAttr)
        return -1;

    Value* tmpValue = (Value*)malloc(sizeof(Value));
    *value = tmpValue;
    
    int* offsets = getRecordItemOffsets(schema);
    char* startAddr = record->data + offsets[attrNum];
    DataType dataType = schema->dataTypes[attrNum];
    int dataSize = getDataSize(schema, attrNum);

    if (dataType == DT_STRING)
    {
        tmpValue->dt = dataType;
        tmpValue->v.stringV = (char*)malloc(dataSize + 1);
        tmpValue->v.stringV[dataSize] = '\0';
        memcpy(tmpValue->v.stringV, startAddr, dataSize);
    }
    else
    {
        tmpValue->dt = dataType;
        memcpy(&tmpValue->v, startAddr, dataSize);
    }

    return RC_OK;
}

RC setAttr(Record *record, Schema *schema, int attrNum, Value *value)
{
    if (attrNum >= schema->numAttr)
        return -1;
    
    int* offsets = getRecordItemOffsets(schema);
    char* startAddr = record->data + offsets[attrNum];
    int dataSize = getDataSize(schema, attrNum);

    DataType dataType = schema->dataTypes[attrNum];
    if (dataType == DT_STRING)
    {
        memcpy(startAddr, value->v.stringV, dataSize);
    }
    else
    {
        memcpy(startAddr, &value->v, dataSize);
    }

    free(offsets);

    return RC_OK;
}
