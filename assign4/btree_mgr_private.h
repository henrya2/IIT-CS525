#ifndef BTREE_MGR_PRIVATE_H
#define BTREE_MGR_PRIVATE_H

#include "buffer_mgr.h"
#include "tables.h"

typedef struct BTreeHandleMeta
{
    DataType keyType;
    int n;

    int nextNodePage;

    int rootPage;

    int numNodes;
    int numEntries;
} BTreeHandleMeta;

typedef struct BTreeHandleMgmtData
{
    BM_BufferPool* bufferPool;

    BTreeHandleMeta* meta;
} BTreeHandleMgmtData;

typedef struct BTreeNode
{
    bool leaf;
    int parent;

    int page;

    int keys[32];
    int numKeys;

    union {
        int children[33];
        RID records[33];
    };

    int nextLeaf;
} BTreeNode;

typedef struct BTreeScanMgmtData
{
    int nodePage;
    int recI;

    BTreeHandleMgmtData* treeMgmtData;
} BTreeScanMgmtData;

void initBTreeNodeEmpty(BTreeNode* node);

void markPageDirty(BM_BufferPool* bufferPool, int page);

void markMetaPageDirty(BM_BufferPool* bufferPool);

void unpinNode(BTreeHandleMgmtData* treeMgmtData, BTreeNode* node);

BTreeNode* readNode(BTreeHandleMgmtData* treeMgmtData, int page);

BTreeNode* createNewNode(BTreeHandleMgmtData* treeMgmtData);

BTreeNode* findLeafNode(BTreeHandleMgmtData* treeMgmtData, int k);

BTreeNode* traverseToLeastLeaf(BTreeHandleMgmtData* treeMgmtData);

RC insertInLeaf(BTreeNode* node, int k, RID rid);

RC insertInParent(BTreeHandleMgmtData* treeMgmtData, BTreeNode* node0, int k, BTreeNode* node1);

RC insertInNonleaf(BTreeNode* nodeP, BTreeNode* anchorNode, int k, BTreeNode* newNode);

RC removeInLeaf(BTreeNode* node, int k);

#endif