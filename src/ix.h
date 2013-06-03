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
#include "ix.fwd.h"
#include "IX_BTNode.h"

////////////////////////////////////////////////////////////////////
//
// IX_BTNode: IX B+ Tree interface
//
// The leaf node and internal node used the same structure beacause the  internal node do not use 
// the slot id part of the RID 

 
struct IX_FileHdr {
    PageNum firstFreePage; //addr of root page; first free page in th lined list
    int numPages;      // # of pages in the file
    int pageSize;      // size per index node - usually PF_PAGE_SIZE
    int pairSize;      // size of each (key, RID) pair in index
    int order;         // order of btree
    int height;        // height of btree
    AttrType attrType;
    int attrLength;
//    unsigned pageHeaderSize;    // page header size
//    unsigned numNodes;        // # of pages in the file
};
////////////////////////////////////////////////////////////////////
//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_FileScan;
public:
    IX_IndexHandle();
    ~IX_IndexHandle();

    // Insert a new index entry
    RC InsertEntry(void *pData, const RID &rid);

    // Delete a new index entry
    RC DeleteEntry(void *pData, const RID &rid);

    // Force index files to disk
    RC ForcePages();

    RC Open(PF_FileHandle &fileHandle);
    RC Close();

    RC GetNewPage(PageNum& pageNum);
//===============================================================
    RC IsValid () const;
    IX_BTNode* FindLargestLeaf();
    IX_BTNode* FetchNode(PageNum p) const;
    IX_BTNode* FetchNode(RID r) const;
    RC GetThisPage(PageNum p, PF_PageHandle& ph) const;
//===============================================================
    IX_BTNode* FindLeaf(const void *pData);
    int GetHeight() const;
    void SetHeight(const int& h);
    IX_BTNode* GetRoot() const;
//===============================================================
    void Print(int level, RID r) const;
    void PrintHeader() const;
//===============================================================
    IX_BTNode* DupScanLeftFind(IX_BTNode* right, void *pData, const RID& rid);
    RC DisposePage(const PageNum& pageNum);
//===============================================================
    // Insert a new index entry
    RC FindEntry(void *pData, const RID &rid);

private:
    // Copy constructor
    IX_IndexHandle  (const IX_IndexHandle &fileHandle);
    // Overloaded =
    IX_IndexHandle& operator=(const IX_IndexHandle &fileHandle);

    PF_FileHandle* pfFileHandle;
    IX_FileHdr fileHdr;                                   // file header
    int bHdrChanged;                                      // dirty flag for file hdr
//=================================================================================
    bool bFileOpen;
    IX_BTNode* root; // root in turn points to the other nodes
    IX_BTNode** path;
    //IX_BTNode ** path; // list of nodes that is the path to leaf as a
                       // result of a search.
    int* pathP; // the position in the parent in the path the points to
                // the child node.

    void * treeLargest; // largest key in the entire tree
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
    RC CreateIndex(const char *fileName, int indexNo, AttrType attrType, int attrLength);

    // Destroy and Index
    RC DestroyIndex(const char *fileName, int indexNo);

    // Open an Index
    RC OpenIndex(const char *fileName, int indexNo, IX_IndexHandle &indexHandle);

    // Close an Index
    RC CloseIndex(IX_IndexHandle &indexHandle);
private:
    PF_Manager *pPfm;
};

//
// Print-error function
//
void IX_PrintError(RC rc);

#define IX_INVALIDATTR     (START_IX_WARN + 1) // invalid attribute parameters
#define IX_NULLPOINTER     (START_IX_WARN + 2) // pointer is null
#define IX_ENTRYEXISTS     (START_IX_WARN + 3) //
#define IX_CLOSEDFILE      (START_IX_WARN + 4) // index handle is closed
#define IX_LASTWARN        IX_CLOSEDFILE
//#define START_IX_ERR  (-201) in redbase.h
#define IX_SIZETOOBIG      (START_IX_ERR - 0)  // key size too big
#define IX_PF              (START_IX_ERR - 1)  // error in PF operations
#define IX_BADIXPAGE       (START_IX_ERR - 2)  // bad page
#define IX_FCREATEFAIL     (START_IX_ERR - 3)  //
#define IX_HANDLEOPEN      (START_IX_ERR - 4)  // error handle open
#define IX_BADOPEN         (START_IX_ERR - 5)  // the index is alread open
#define IX_FNOTOPEN        (START_IX_ERR - 6)  //
#define IX_BADRID          (START_IX_ERR - 7)  // bad rid
#define IX_BADKEY          (START_IX_ERR - 8)  // bad key
#define IX_NOSUCHENTRY     (START_IX_ERR - 9)  //
#define IX_KEYNOTFOUND     (START_IX_ERR - 10) // key is not found
#define IX_INVALIDSIZE     (START_IX_ERR - 11) // btnode's size(order) is invalid
#define IX_LASTERROR       IX_INVALIDSIZE  // end of file

#define IX_EOF             PF_EOF              // work-around for rm_test
#endif
