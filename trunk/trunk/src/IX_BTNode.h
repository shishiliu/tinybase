/* 
 * File:   IX_BTNode.h
 *
 * Created on May 21, 2013, 11:39 PM
 */

#ifndef IX_BTNODE_H
#define IX_BTNODE_H

#include "redbase.h"
#include "rm_rid.h"
#include <string.h>
#include <iosfwd>

class IX_BTNode {
    friend class IX_IndexHandle;
    friend class IX_IndexScan;
public:
    IX_BTNode(AttrType attributeType, int iAttrLength, PF_PageHandle& aPh, bool isNewPage);
    ~IX_BTNode();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.

    PageNum GetLeft();
    PageNum GetLeftFromPage();
    void SetLeft(PageNum aPageId);

    PageNum GetRight();
    PageNum GetRightFromPage();
    void SetRight(PageNum aPageId);

    int GetKeysNumFromPage();
    int GetKeysNum();
    void SetKeysNumToPage(int aNumKeys);

    int GetOrder();
    int GetMaxKeys() const;
    //get the node page rid
    RID GetNodeRID();

    //get the key value given by the iPos
    RC GetKey(int iPos, void * & aKey) const;
    //set the key value in the iPos
    RC SetKey(int iPos, const void* aKey);
    int CopyKey(int pos, void* toKey) const;

    // return position if key already exists at position
    // if there are dups - returns rightmost position unless an RID is
    // specified.
    // return -1 if there was an error or if key does not exist
    int FindKeyBrut(const void* &akey) const;
    // if optional RID is specified, will only return a position if both
    // key and RID match.
    int FindKeyExact(const void* &aKeyconst, const RID& r = RID(-1,-1)) const;

    RID FindAddrBrut(const void* &key) const;
    RID FindAddrExact(const void* &key) const;

    RID GetAddr(const int pos) const;

    RC InsertNode(const void* aKey, const RID & aRid);
    RC DeleteNode(const void* aKey,int kpos);
    RC MergeNode(IX_BTNode* aNode);
    RC SplitNode(IX_BTNode* aNewNode);

    int CompareKey(const void * aKey, const void * bKey) const;
    bool TestSorted() const;

    AttrType GetType();
    int GetAttrLength();

    //friend std::ostream &operator<<(std::ostream &os,IX_BTNode &a);
    void Print();
    void* LargestKey() const;
    int Destroy();
    //
    RC IsValid() const;
    friend std::ostream &operator<<(std::ostream &os,IX_BTNode &a);

private:
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
#endif  /* IX_BTNODE_H */
