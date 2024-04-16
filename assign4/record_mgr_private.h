#ifndef RECORD_MGR_PRIVATE_H
#define RECORD_MGR_PRIVATE_H

#include "buffer_mgr.h"

typedef struct TableMetaData
{
    int schemaSize;  // schema size in disk

    int lastFreePage;
    int lastFreeSlot;

    int recordSize;
    int recordSlotsPerPage;
} TableMetaData;

typedef struct TableMgtInfo
{
    BM_BufferPool* bufferPool; // Buffer pool
    TableMetaData metaData; // Meta data

    RID pageSlotRemap[32][500];
    
} TableMgtInfo;

int getDataSize(Schema* schema, int i)
{
    int result = 0;
    switch (schema->dataTypes[i])
    {
    case DT_INT:
        result = sizeof(int);
        break;
    case DT_FLOAT:
        result = sizeof(float);
        break;
    case DT_BOOL:
        result = sizeof(bool);
        break;
    case DT_STRING:
        result = schema->typeLength[i] * sizeof(char);
        break;

    default:
        break;
    }

    return result;
}

int* getRecordItemOffsets(Schema* schema);

typedef struct BinSerializationContext
{
    char* buffer; // char is one byte

    int cursor; // current access byte index location (relative from buffer pointer)

    int maxSize; // bytes number
} BinSerializationContext;

// get the schema itself size in disk
/*
    binSize += sizeof(schema->numAttr);  // numAttr
    binSize += sizeof(int); // maxNameSize
    binSize += sizeof(char) * maxNameSize * numAttr; // attrNames
    binSize += sizeof(DataType) * numAttr; // dataTypes
    binSize += sizeof(int) * numAttr; // typeLength
    binSize += sizeof(int) * schema->keySize; // keyAttrs
    binSize += sizeof(int); // keySize;
*/
int getSchemaMetaBinSize(Schema* schema);

int readSchemaFromBuffer(Schema* schema, char* buffer);

int writeSchemaToBuffer(Schema* schema, char* buffer);

RC updateTableMetaData(TableMgtInfo* tableMgtInfo);

void closeTableMgtInfo(TableMgtInfo* tableMgtInfo);

RC writeRecordToTable(TableMgtInfo* tableMgtInfo, Record *record);

bool getPreviousValidPageSlot(int page, int slot, int* outPage, int* outSlot, int recordSlotsPerPage)
{
    bool validResult = true;
    slot -= 1;
    if (slot < 0)
    {
        page -= 1;
        if (page < 1) // record always starts at page 1
        {
            validResult = false;
        }
        slot = recordSlotsPerPage - 1;
    }

    if (validResult)
    {
        *outPage = page;
        *outSlot = slot;
    }

    return validResult;
}

void getNextPageSlot(int page, int slot, int* outPage, int* outSlot, int recordSlotsPerPage)
{
    slot += 1;
    if (slot >= recordSlotsPerPage)
    {
        slot = 0;
        page += 1;
    }

    *outPage = page;
    *outSlot = slot;
}

typedef struct ScanHandleMgtInfo
{
    Expr* expr; // expr for conditional
    RID nextId; // next start scan id
} ScanHandleMgtInfo;

void closeScaneHandleMgtInfo(ScanHandleMgtInfo* mgtInfo);

#endif