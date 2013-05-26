/* 
 * File:   IX_BTNode.h
 *
 * Created on May 21, 2013, 11:39 PM
 */

#ifndef IX_BTNODE_H
#define	IX_BTNODE_H

#include "redbase.h"
#include "pf.fwd.h"
#include "rm_rid.h"
#include <string.h>
////////////////////////////////////////////////////////////////////
//
// IX_BTNode: IX B+ Tree interface
//
// The leaf node and internal node used the same structure beacause the  internal node do not use 
// the slot id part of the RID 

class IX_BTNode {
    friend class IX_FileHandle;
    friend class IX_FileScan;
public:
    IX_BTNode(AttrType attributeType, int iAttrLength, PF_PageHandle& aPh, bool isNewPage);
    ~IX_BTNode();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&aData) const;
    PageNum GetLeft();
    void SetLeft(PageNum aPageId);
    PageNum GetRight();
    void SetRight(PageNum aPageId);
    RC InsertNode(const void* aKey, const RID & aRid);
    RC DeleteNode(const void* aKey);
    RC MergeNode(IX_BTNode* aNode);
    RC SplitNode(IX_BTNode* aNewNode);
    int GetKeysNum();
    RC GetKey(int iPos, void * & aKey);
    RC SetKey(int iPos, const void* aKey);
    int FindKey(const void* &aKey);
    int FindKeyExact(const void* &aKey, const RID& aRid);
    int CompareKey(const void * aKey, const void * bKey);
    bool TestSorted();
    RID GetRid(const int iPos);
    RID GetNodeRID();
    int GetOrder();
    AttrType GetType();
    int GetAttrLength();
    friend std::ostream &operator<<(std::ostream &os,IX_BTNode &a);
private:
    char *pData;
    char* keys; //the table of key value according to the attribute
    RID* rids; //the rids values table
    RID nodeRid; //this is the rid of the page
    AttrType attributeType;
    int attributeLength;
    int order; //the order of the node
    int keysNum; //the number of keys
    PageNum left; //to the left neighbour:pageID
    PageNum right; //to the right neighbour:pageID
};


#endif	/* IX_BTNODE_H */
