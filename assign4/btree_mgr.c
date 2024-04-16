#include "btree_mgr.h"
#include "buffer_mgr.h"
#include "storage_mgr.h"
#include <string.h>
#include <stdlib.h>
#include "btree_mgr_private.h"

char tempPage[PAGE_SIZE] = {0};

RC initIndexManager(void *mgmtData)
{
    return RC_OK;
}

RC shutdownIndexManager()
{
    return RC_OK;
}

RC createBtree(char *idxId, DataType keyType, int n)
{
    BTreeHandleMeta metaData;
    metaData.keyType = keyType;
    metaData.n = n;
    metaData.numNodes = 1;
    metaData.numEntries = 0;
    metaData.nextNodePage = 2;
    metaData.rootPage = 1;

    RC result = createPageFile(idxId);
    if (result != RC_OK)
        return result;
    
    SM_FileHandle fileHandle;
    result = openPageFile(idxId, &fileHandle);
    if (result != RC_OK)
        return result;

    memcpy(tempPage, &metaData, sizeof(BTreeHandleMeta));
    result = writeBlock(0, &fileHandle, tempPage);
    if (result != RC_OK)
        return result;

    // Write root node
    BTreeNode rootNode;
    rootNode.leaf = true;
    rootNode.numKeys = 0;
    rootNode.parent = -1;
    rootNode.nextLeaf = -1;
    rootNode.page = 1;
    memset(rootNode.keys, -1, sizeof(rootNode.keys));
    memset(rootNode.children, -1, sizeof(rootNode.children));
    memcpy(tempPage, &rootNode, sizeof(BTreeNode));
    result = writeBlock(1, &fileHandle, tempPage);
    if (result != RC_OK)
        return result;    

    return RC_OK;
}

RC openBtree(BTreeHandle **tree, char *idxId)
{
    BTreeHandle* newTree = (BTreeHandle*)malloc(sizeof(BTreeHandle));
    newTree->mgmtData = (BTreeHandleMgmtData*)malloc(sizeof(BTreeHandleMgmtData));
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)newTree->mgmtData;
    treeMgmtData->bufferPool = MAKE_POOL();
    newTree->idxId = idxId;
    initBufferPool(treeMgmtData->bufferPool, idxId, 64, RS_FIFO, NULL);
    BM_PageHandle pageHandle;
    pinPage(treeMgmtData->bufferPool, &pageHandle, 0);
    treeMgmtData->meta = (BTreeHandleMeta*)pageHandle.data;
    newTree->keyType = treeMgmtData->meta->keyType;

    *tree = newTree;

    return RC_OK;
}

RC closeBtree(BTreeHandle *tree)
{
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;
    RC result = shutdownBufferPool(treeMgmtData->bufferPool);
    if (result != RC_OK)
    {
        return result;
    }

    free(treeMgmtData);
    tree->mgmtData = NULL;

    free(tree);

    return RC_OK;
}

RC deleteBtree(char *idxId)
{
    return destroyPageFile(idxId);
}

RC getNumNodes(BTreeHandle *tree, int *result)
{
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;   
    *result = treeMgmtData->meta->numNodes;
    return RC_OK;
}

RC getNumEntries(BTreeHandle *tree, int *result)
{
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;   
    *result = treeMgmtData->meta->numEntries;
    return RC_OK;
}

RC getKeyType(BTreeHandle *tree, DataType *result)
{
    *result = tree->keyType;
    return RC_OK;
}

RC findKey(BTreeHandle *tree, Value *key, RID *result)
{
    BTreeNode* node = NULL;
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;
    int k = key->v.intV;
    node = findLeafNode(treeMgmtData, k);

    int candI = -1;
    for (int i = 0; i < node->numKeys; ++i)
    {
        if (k == node->keys[i])
        {
            candI = i;
            break;
        }
    }
    if (candI < 0)
    {
        return RC_IM_KEY_NOT_FOUND;
    }

    *result = node->records[candI];

    unpinNode(treeMgmtData, node);

    return RC_OK;
}

RC insertKey(BTreeHandle *tree, Value *key, RID rid)
{
    BTreeNode* node = NULL;
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;
    int k = key->v.intV;
    node = findLeafNode(treeMgmtData, k);

    RC result = RC_OK;

    if (node->numKeys + 1 <= treeMgmtData->meta->n)
    {
        result = insertInLeaf(node, k, rid);
        unpinNode(treeMgmtData, node);
    }
    else
    {
        BTreeNode* tempNode = (BTreeNode*)malloc(sizeof(BTreeNode));

        memcpy(tempNode, node, sizeof(BTreeNode));
        result = insertInLeaf(tempNode, k, rid);
        if (result != RC_OK)
        {
            unpinNode(treeMgmtData, node);
            free(tempNode);
            return result;
        }
        BTreeNode* newNode = createNewNode(treeMgmtData);
        newNode->leaf = true;

        newNode->nextLeaf = node->nextLeaf;
        node->nextLeaf = newNode->page;
        int leftNumKeys = (tempNode->numKeys / 2) + (tempNode->numKeys % 2);
        int rightNumKeys = tempNode->numKeys - leftNumKeys;
        for (int i = 0; i < leftNumKeys; ++i)
        {
            node->keys[i] = tempNode->keys[i];
            node->records[i] = tempNode->records[i];
        }
        for (int i = 0; i < rightNumKeys; ++i)
        {
            newNode->keys[i] = tempNode->keys[i + leftNumKeys];
            newNode->records[i] = tempNode->records[i + leftNumKeys];
        }
        free(tempNode);
        // do not use tempNode anymore

        node->numKeys = leftNumKeys;
        newNode->numKeys = rightNumKeys;

        int parentK = newNode->keys[0];
        insertInParent(treeMgmtData, node, parentK, newNode);

        unpinNode(treeMgmtData, node);
        unpinNode(treeMgmtData, newNode);
    }

    if (result == RC_OK)
    {
        treeMgmtData->meta->numEntries += 1;
        markMetaPageDirty(treeMgmtData->bufferPool);
    }
        
    return result;
}

RC deleteKey(BTreeHandle *tree, Value *key)
{
    BTreeNode* node = NULL;
    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;
    int k = key->v.intV;
    node = findLeafNode(treeMgmtData, k);

    RC result = RC_OK;
    result = removeInLeaf(node, k);

    return result;
}

RC openTreeScan(BTreeHandle *tree, BT_ScanHandle **handle)
{
    BT_ScanHandle* newHandle = (BT_ScanHandle*)malloc(sizeof(BT_ScanHandle));
    *handle = newHandle;
    newHandle->mgmtData = (BTreeScanMgmtData*)malloc(sizeof(BTreeHandleMgmtData));
    BTreeScanMgmtData* btscanMgmt = (BTreeScanMgmtData*)newHandle->mgmtData;

    BTreeHandleMgmtData* treeMgmtData = (BTreeHandleMgmtData*)tree->mgmtData;
    BTreeNode* node = traverseToLeastLeaf(treeMgmtData);
    btscanMgmt->nodePage = node->page;
    btscanMgmt->recI = 0;

    btscanMgmt->treeMgmtData = treeMgmtData;

    unpinNode(treeMgmtData, node);

    return RC_OK;
}

RC nextEntry(BT_ScanHandle *handle, RID *result)
{
    BTreeScanMgmtData* btscanMgmt = (BTreeScanMgmtData*)handle->mgmtData;
    BTreeHandleMgmtData* treeMgmtData = btscanMgmt->treeMgmtData;

    if (btscanMgmt->nodePage < 0)
    {
        return RC_IM_NO_MORE_ENTRIES;
    }

    BTreeNode* node = readNode(treeMgmtData, btscanMgmt->nodePage);
    *result = node->records[btscanMgmt->recI];

    btscanMgmt->recI += 1;
    if (btscanMgmt->recI >= node->numKeys)
    {
        btscanMgmt->nodePage = node->nextLeaf;
        btscanMgmt->recI = 0;
    }

    unpinNode(treeMgmtData, node);

    return RC_OK;
}

RC closeTreeScan(BT_ScanHandle *handle)
{
    free(handle->mgmtData);
    free(handle);

    return RC_OK;
}

char *printTree(BTreeHandle *tree)
{
    return "";
}
