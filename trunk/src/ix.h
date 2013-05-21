//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "redbase.h"  // Please don't change these lines
#include "rm_rid.h"  // Please don't change these lines
#include "pf.h"
///////////////////////////////////////////////////////////////////////////////////
//IX_Record: the record represente the B+ tree node
//////
////////////////////////////////////////////////////////////////////
//
// IX_BTNode: IX B+ Tree interface
//
class IX_BTNode {
    friend class IX_FileHandle;
    friend class IX_FileScan;
public:
    IX_BTNode (AttrType attributeType,PF_PageHandle& aPh,bool isNewPage =true,int pageSize = PF_PAGE_SIZE);
    ~IX_BTNode();

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;

private:
    RC insertNode();
    RC deleteNode();
    RC mergeNode();
    RC splitNode();

    char *pData;
    char *keys;//the table of key value according to the attribute
    RID  *rids;//the rids values table
    RID  nodeRid;//this is the rid of the page
    AttrType attributeType;
    int attributeLength;
    int order;//the order of the node
};

//
// IX_FileHdr: Header structure for files
//
struct IX_FileHdr {
    PageNum firstFree;     // first free page in the linked list
    int recordSize;        // fixed record size
    int numRecordsPerPage; // # of records in each page
    int pageHeaderSize;    // page header size
    int numRecords;        // # of pages in the file
};
////////////////////////////////////////////////////////////////////
//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan();
    ~IX_IndexScan();

    // Open index scan
    RC OpenScan(const IX_IndexHandle &indexHandle,
                CompOp compOp,
                void *value,
                ClientHint  pinHint = NO_HINT);

    // Get the next matching entry return IX_EOF if no more matching
    // entries.
    RC GetNextEntry(RID &rid);

    // Close index scan
    RC CloseScan();
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager(PF_Manager &pfm);
    ~IX_Manager();

    // Create a new Index
    RC CreateIndex(const char *fileName, int indexNo,
                   AttrType attrType, int attrLength,int recordSize);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo,
                 IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);
private:
    PF_Manager *pPfm;
};

//
// Print-error function
//
void IX_PrintError(RC rc);

#endif
