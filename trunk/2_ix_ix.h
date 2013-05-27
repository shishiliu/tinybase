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

class BTNode {
 public:
  // if newPage is false then the page ph is expected to contain an
  // existing btree node, otherwise a fresh node is assumed.
  BTNode(AttrType attrType, int attrLength,
            PF_PageHandle& ph, bool newPage = true,
            int pageSize = PF_PAGE_SIZE);
  RC ResetBTNode(PF_PageHandle& ph, const BTNode& rhs);
  ~BTNode();
  int Destroy();

  friend class BTNodeTest;
  friend class IX_IndexHandle;
  RC IsValid() const;
  int GetMaxKeys() const;

  // structural setters/getters - affect PF_page composition
  int GetNumKeys();
  int SetNumKeys(int newNumKeys);
  int GetLeft();
  int SetLeft(PageNum p);
  int GetRight();
  int SetRight(PageNum p);


  RC GetKey(int pos, void* &key) const;
  int SetKey(int pos, const void* newkey);
  int CopyKey(int pos, void* toKey) const;


  // return 0 if insert was successful
  // return -1 if there is no space
  int Insert(const void* newkey, const RID& newrid);

// return 0 if remove was successful
// return -1 if key does not exist
// kpos is optional - will remove from that position if specified
// if kpos is specified newkey can be NULL
  int Remove(const void* newkey, int kpos = -1);

  // exact match functions

  // return position if key already exists at position
  // if there are dups - returns rightmost position unless an RID is
  // specified.
  // if optional RID is specified, will only return a position if both
  // key and RID match.
  // return -1 if there was an error or if key does not exist
  int FindKey(const void* &key, const RID& r = RID(-1,-1)) const;

  RID FindAddr(const void* &key) const;
  // get rid for given position
  // return (-1, -1) if there was an error or pos was not found
  RID GetAddr(const int pos) const;


  // find a poistion instead of exact match
  int FindKeyPosition(const void* &key) const;
  RID FindAddrAtPosition(const void* &key) const;

  // split or merge this node with rhs node
  RC Split(BTNode* rhs);
  RC Merge(BTNode* rhs);

  // print
  //void Print(ostream & os);
  void Print();

  // get/set pageRID
  RID GetPageRID() const;
  void SetPageRID(const RID&);

  int CmpKey(const void * k1, const void * k2) const;
  bool isSorted() const;
  void* LargestKey() const;

 private:
  // serialized
  char * keys; // should not be accessed directly as keys[] but with SetKey()
  RID * rids;
  int numKeys;
  // not serialized - common to all ix pages
  int attrLength;
  AttrType attrType;
  int order;
  // not serialized - convenience
  RID pageRID;
};
//
// IX_FileHdr: Header structure for files
//
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
private:
    // Copy constructor
    IX_IndexHandle  (const IX_IndexHandle &fileHandle);
    // Overloaded =
    IX_IndexHandle& operator=(const IX_IndexHandle &fileHandle);

    PF_FileHandle* pfFileHandle;
    IX_FileHdr fileHdr;                                   // file header
    int bHdrChanged;                                      // dirty flag for file hdr

////////////////////////////////////////////////////////////////////
public:
 // Search an index entry
 // return -ve if error
 // 0 if found
 // IX_KEYNOTFOUND if not found
 RC Search(void *pData, RID &rid);

 bool HdrChanged() const { return bHdrChanged; }
 int GetNumPages() const { return fileHdr.numPages; }
 AttrType GetAttrType() const { return fileHdr.attrType; }
 int GetAttrLength() const { return fileHdr.attrLength; }

 RC GetNewPage(PageNum& pageNum);
 RC DisposePage(const PageNum& pageNum);

 RC IsValid() const;

 // return NULL if the key is bad
 // otherwise return a pointer to the leaf node where key might go
 // also populates the path member variable with the path
 BTNode* FindLeaf(const void *pData);
 BTNode* FindSmallestLeaf();
 BTNode* FindLargestLeaf();
 BTNode* DupScanLeftFind(BTNode* right,
                            void *pData,
                            const RID& rid);
 // hack for indexscan::OpOptimize
 BTNode* FindLeafForceRight(const void* pData);

 BTNode* FetchNode(RID r) const;
 BTNode* FetchNode(PageNum p) const;

 void ResetNode(BTNode*& old, PageNum p) const;
 // Reset to the BTNode at the RID specified within Btree
 void ResetNode(BTNode*& old, RID r) const;

 // get/set height
 int GetHeight() const;
 void SetHeight(const int&);

 BTNode* GetRoot() const;
 void Print(int level = -1, RID r = RID(-1,-1)) const;

 RC Pin(PageNum p);
 RC UnPin(PageNum p);

private:
 //Unpinning version that will unpin after every call correctly
 RC GetThisPage(PageNum p, PF_PageHandle& ph) const;

 bool bFileOpen;
 BTNode* root; // root in turn points to the other nodes
 BTNode* path[5];
 //BTNode ** path; // list of nodes that is the path to leaf as a
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
