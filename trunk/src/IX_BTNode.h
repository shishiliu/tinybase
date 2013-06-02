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
#include <iosfwd>
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
    PageNum GetLeftFromPage();
    void SetLeft(PageNum aPageId);
    void SetLeftToPage(PageNum p);
    PageNum GetRight();
    PageNum GetRightFromPage();
    void SetRight(PageNum aPageId);
    void SetRightToPage(PageNum p);
    int GetKeysNumFromPage();
    void SetKeysNumToPage(int aNumKeys);
    RC InsertNode(const void* aKey, const RID & aRid);
    RC DeleteNode(const void* aKey,int kpos);
    RC MergeNode(IX_BTNode* aNode);
    RC SplitNode(IX_BTNode* aNewNode);
    int GetKeysNum();
    RC GetKey(int iPos, void * & aKey) const;
    RC SetKey(int iPos, const void* aKey);
    int FindKey(const void* &aKey) const;
    int FindKeyExact(const void* &aKey, const RID& aRid);
    RID FindAddrAtPosition(const void* &key) const;
    RID FindAddr(const void* &key) const;
    RID GetAddr(const int pos) const;
    int FindKeyPosition(const void* &key) const;
    int CompareKey(const void * aKey, const void * bKey) const;
    bool TestSorted();
    RID GetRid(const int iPos);
    RID GetNodeRID();
    int GetOrder();
    int CopyKey(int pos, void* toKey) const;
    AttrType GetType();
    int GetAttrLength();
    friend std::ostream &operator<<(std::ostream &os,IX_BTNode &a);
    void* LargestKey() const;
    int Destroy();
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
