#include "btree_mgr_private.h"
#include "buffer_mgr.h"
#include <string.h>
#include <stdlib.h>

void initBTreeNodeEmpty(BTreeNode *node)
{
    node->leaf = false;
    node->parent = -1;
    node->nextLeaf = -1;
    node->numKeys = 0;
}

void markPageDirty(BM_BufferPool *bufferPool, int page)
{
    BM_PageHandle pageHandle;
    pinPage(bufferPool, &pageHandle, page);
    markDirty(bufferPool, &pageHandle);
    unpinPage(bufferPool, &pageHandle);
}

void markMetaPageDirty(BM_BufferPool *bufferPool)
{
    markPageDirty(bufferPool, 0);
}

void unpinNode(BTreeHandleMgmtData *treeMgmtData, BTreeNode *node)
{
    BM_BufferPool *bufferPool = treeMgmtData->bufferPool;
    BM_PageHandle pageHandle;
    pageHandle.pageNum = node->page;
    unpinPage(bufferPool, &pageHandle);
}

BTreeNode *readNode(BTreeHandleMgmtData *treeMgmtData, int page)
{
    BM_PageHandle pageHandle;
    BM_BufferPool* bufferPool = treeMgmtData->bufferPool;
    pinPage(bufferPool, &pageHandle, page);
    markDirty(bufferPool, &pageHandle);

    BTreeNode* newNode = (BTreeNode*)pageHandle.data;

    return newNode;
}

BTreeNode *createNewNode(BTreeHandleMgmtData *treeMgmtData)
{
    BM_PageHandle pageHandle;

    treeMgmtData->meta->numNodes += 1;
    int newPage = treeMgmtData->meta->nextNodePage++;
    BM_BufferPool* bufferPool = treeMgmtData->bufferPool;
    pinPage(bufferPool, &pageHandle, newPage);
    markDirty(bufferPool, &pageHandle);

    BTreeNode* newNode = (BTreeNode*)pageHandle.data;
    initBTreeNodeEmpty(newNode);
    newNode->page = newPage;

    return newNode;
}

BTreeNode *findLeafNode(BTreeHandleMgmtData *treeMgmtData, int k)
{
    int rootPage = treeMgmtData->meta->rootPage;
    BTreeNode* node = readNode(treeMgmtData, rootPage);

    while (!node->leaf)
    {
        int i = 0;
        for (i = 0; i < node->numKeys; ++i)
        {
            if (k < node->keys[i])
            {
                break;
            }
        }
        int pageNum = node->children[i];
        unpinNode(treeMgmtData, node);
        node = readNode(treeMgmtData, pageNum);
    }

    return node;
}

BTreeNode *traverseToLeastLeaf(BTreeHandleMgmtData *treeMgmtData)
{
    int rootPage = treeMgmtData->meta->rootPage;
    BTreeNode* node = readNode(treeMgmtData, rootPage);

    while (!node->leaf)
    {
        int i = 0; // always the least key, the left most key
        int pageNum = node->children[i];
        unpinNode(treeMgmtData, node);
        node = readNode(treeMgmtData, pageNum);
    }

    return node;
}

RC insertInLeaf(BTreeNode *node, int k, RID rid)
{
    RC result = RC_OK;
    int i = node->numKeys - 1;
    for (; i >= 0; --i)
    {
        if (node->keys[i] == k)
        {
            result = RC_IM_KEY_ALREADY_EXISTS;
            break;
        }
        else if (k > node->keys[i])
        {
            break;
        }
        else
        {
            node->keys[i + 1] = node->keys[i];
            node->records[i + 1] = node->records[i];
        }
    }

    if (result == RC_OK)
    {
        node->keys[i + 1] = k;
        node->records[i + 1] = rid;
        node->numKeys += 1;
    }

    return result;
}

RC insertInParent(BTreeHandleMgmtData *treeMgmtData, BTreeNode *node0, int k, BTreeNode *node1)
{
    if (node0->parent == -1)
    {
        BTreeNode* newRootNode = createNewNode(treeMgmtData);
        treeMgmtData->meta->rootPage = newRootNode->page;

        newRootNode->parent = -1; // root
        newRootNode->numKeys = 1;
        newRootNode->keys[0] = k;
        newRootNode->children[0] = node0->page;
        newRootNode->children[1] = node1->page;

        node0->parent = newRootNode->page;
        node1->parent = newRootNode->page;

        unpinNode(treeMgmtData, newRootNode);
        return RC_OK;
    }
    
    BTreeNode* nodeP0 = readNode(treeMgmtData, node0->parent);
    if (nodeP0->numKeys < treeMgmtData->meta->n)
    {
        //insert (K', N') in P just after N
        insertInNonleaf(nodeP0, node0, k, node1);

        unpinNode(treeMgmtData, nodeP0);
    }
    else
    {
        /* Split P */
        // Copy P to a block of memory T that can hold P and (K', N')
        BTreeNode* tempNode = (BTreeNode*)malloc(sizeof(BTreeNode));
        memcpy(tempNode, nodeP0, sizeof(BTreeNode));
        insertInNonleaf(tempNode, node0, k, node1);
        BTreeNode* newNodeP = createNewNode(treeMgmtData);
        newNodeP->leaf = false;

        int midKI = (tempNode->numKeys - 1) / 2;
        int midK = tempNode->keys[midKI];
        int leftNumKeys = midKI;
        for (int i = 0; i < leftNumKeys; ++i)
        {
            nodeP0->keys[i] = tempNode->keys[i];
            nodeP0->children[i] = tempNode->children[i];
        }
        nodeP0->children[leftNumKeys] = tempNode->children[leftNumKeys];
        nodeP0->numKeys = leftNumKeys;

        int rightNumKeys = tempNode->numKeys - leftNumKeys - 1;
        int rightStartI = midKI + 1;
        for (int i = 0; i < rightNumKeys; ++i)
        {
            newNodeP->keys[i] = tempNode->keys[i + rightStartI];
            newNodeP->children[i] = tempNode->children[i + rightStartI];
        }
        newNodeP->children[rightNumKeys] = tempNode->children[rightNumKeys + rightStartI];
        newNodeP->numKeys = rightNumKeys;
        free(tempNode);

        insertInParent(treeMgmtData, nodeP0, midK, newNodeP);
    }

    return RC_OK;
}

RC insertInNonleaf(BTreeNode *nodeP0, BTreeNode *anchorNode, int k, BTreeNode *newNode)
{
    RC result = RC_OK;
    // insert (K', N') in P just after N
    // The last point is at nodeP0->numKeys, the number of pointers is 1 bigger than keys'
    int candI = -1;
    for (int i = nodeP0->numKeys; i >= 0; --i)
    {
        if (nodeP0->children[i] == anchorNode->page)
        {
            candI = i;
            break;
        }
    }
    if (candI < 0)
    {
        return RC_IM_KEY_NOT_FOUND;
    }
    for (int i = nodeP0->numKeys; i > candI; --i)
    {
        nodeP0->children[i + 1] = nodeP0->children[i];
        nodeP0->keys[i] = nodeP0->keys[i - 1];
    }
    nodeP0->keys[candI] = k;
    nodeP0->children[candI + 1] = newNode->page;
    nodeP0->numKeys += 1;

    newNode->parent = nodeP0->page;

    return result;
}

RC removeInLeaf(BTreeNode *node, int k)
{
    int candI = -1;
    for (int i = 0; i < node->numKeys; ++i)
    {
        if (node->keys[i] == k)
        {
            candI = i;
            break;
        }
    }
    if (candI < 0)
    {
        return RC_IM_KEY_NOT_FOUND;
    }

    for (int i = candI; i < node->numKeys - 1; ++i)
    {
        node->keys[i] = node->keys[i + 1];
        node->records[i] = node->records[i + 1];
    }
    node->numKeys -= 1;

    return RC_OK;
}
