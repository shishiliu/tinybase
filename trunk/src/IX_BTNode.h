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
////////////////////////////////////////////////////////////////////
//
// IX_BTNode: IX B+ Tree interface
//
class IX_BTNode {
    friend class IX_FileHandle;
    friend class IX_FileScan;
public:
    IX_BTNode(AttrType attributeType, PF_PageHandle& aPh, bool isNewPage, int pageSize);
    ~IX_BTNode();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;

private:
    RC InsertNode();
    RC DeleteNode();
    RC MergeNode();
    RC SplitNode();

    char *pData;
    char *keys;//the table of key value according to the attribute
    RID  *rids;//the rids values table
    RID  nodeRid;//this is the rid of the page
    AttrType attributeType;
    int attributeLength;
    int order;//the order of the node
};


#endif	/* IX_BTNODE_H */

