//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle class implementation
// Author:      LIU Shishi (shishi.liu@enst.fr)
//

#include "ix_internal.h"
#include <stdio.h>

IX_IndexHandle::IX_IndexHandle()
{
   // Initialize member variables
   bHdrChanged = FALSE;
   memset(&fileHdr, 0, sizeof(fileHdr));
   fileHdr.firstFreePage = IX_PAGE_LIST_END;
   bFileOpen = false;
   pfFileHandle = NULL;
   root = NULL;
   //path = NULL;
   pathP = NULL;
   treeLargest = NULL;
   fileHdr.height = 0;
}

IX_IndexHandle::~IX_IndexHandle()
{
    if(root != NULL && (pfFileHandle) != NULL) {
      // unpin old root page
      pfFileHandle->UnpinPage(fileHdr.firstFreePage);
      delete root;
      root = NULL;
    }
    if(pathP != NULL) {
      delete [] pathP;
      pathP = NULL;
    }
    if(path != NULL) {
      // path[0] is root
      for (int i = 1; i < fileHdr.height; i++)
        if(path[i] != NULL) {
          if((pfFileHandle) != NULL)
            {
                pfFileHandle->UnpinPage(path[i]->GetPageRID().Page());
            }
          // delete path[i]; - better leak than crash
        }
      delete [] path;
      //path = NULL;
    }
    if((pfFileHandle) != NULL) {
      delete pfFileHandle;
      pfFileHandle = NULL;
    }
    if(treeLargest != NULL) {
      delete [] (char*) treeLargest;
      treeLargest = NULL;
    }
}

RC IX_IndexHandle::Pin(PageNum p) {
  PF_PageHandle ph;
  RC rc = pfFileHandle->GetThisPage(p, ph);
  return rc;
}

RC IX_IndexHandle::UnPin(PageNum p) {
  RC rc = pfFileHandle->UnpinPage(p);
  return rc;
}

// Users will call - RC invalid = IsValid(); if(invalid) return invalid;
RC IX_IndexHandle::IsValid () const
{
  bool ret = true;
  ret = ret && ((pfFileHandle) != NULL);
  if(fileHdr.height > 0) {
    ret = ret && (fileHdr.firstFreePage > 0); // 0 is for file header
    ret = ret && (fileHdr.numPages >= fileHdr.height + 1);
    ret = ret && (root != NULL);
    ret = ret && (path != NULL);
  }
  return ret ? 0 : IX_BADIXPAGE;
}

//Unpinning version that will unpin after every call correctly
RC IX_IndexHandle::GetThisPage(PageNum p, PF_PageHandle& ph) const {
  RC rc = pfFileHandle->GetThisPage(p, ph);
  if (rc != 0) return rc;
  // Needs to be called everytime GetThisPage is called.
  rc = pfFileHandle->MarkDirty(p);
  if(rc!=0) return rc;

  rc = pfFileHandle->UnpinPage(p);
  if (rc != 0) return rc;
  return 0;
}

// Forces a page (along with any contents stored in this class)
// from the buffer pool to disk.  Default value forces all pages.
RC IX_IndexHandle::ForcePages ()
{
  RC invalid = IsValid(); if(invalid) return invalid;
  return pfFileHandle->ForcePages(ALL_PAGES);
}

RC IX_IndexHandle::GetNewPage(PageNum& pageNum)
{
  RC invalid = IsValid(); if(invalid) return invalid;
  PF_PageHandle ph;
  RC rc;
  if ((rc = pfFileHandle->AllocatePage(ph)))
  {
    return(rc);
  }
  if((rc = ph.GetPageNum(pageNum)))
  {
    return(rc);
  }


  // the default behavior of the buffer pool is to pin pages
  // let us make sure that we unpin explicitly after setting
  // things up
  if (
//  (rc = pfFileHandle->MarkDirty(pageNum)) ||
    (rc = pfFileHandle->UnpinPage(pageNum)))
    return rc;
  // std::cerr << "GetNewPage called to get page " << pageNum << std::endl;
  fileHdr.numPages++;
  assert(fileHdr.numPages > 1); // page 0 is this page in worst case
  bHdrChanged = true;
  return 0; // pageNum is set correctly
}

RC IX_IndexHandle::DisposePage(const PageNum& pageNum)
{
  RC invalid = IsValid(); if(invalid) return invalid;

  RC rc;
  if ((rc = pfFileHandle->DisposePage(pageNum)))
    return(rc);

  fileHdr.numPages--;
  assert(fileHdr.numPages > 0); // page 0 is this page in worst case
  bHdrChanged = true;
  return 0; // pageNum is set correctly
}

//pairSize: size of each (key, RID) pair in index
RC IX_IndexHandle::Open(PF_FileHandle& fileHandle)
{
  // the index file has alread been opened.
  if(bFileOpen || (pfFileHandle) != NULL)
  {
      return IX_HANDLEOPEN;
  }

  bFileOpen = true;
  pfFileHandle = new PF_FileHandle();
  *pfFileHandle = fileHandle;

  PF_PageHandle rootPageHandle;
  bool newPage = true;

  if(fileHdr.firstFreePage == IX_PAGE_LIST_END)
  {
      PageNum pageNum;
      RC rc = GetNewPage(pageNum);              
      if (rc != 0) return rc;
      fileHdr.firstFreePage = pageNum;
  }
  else
  {
      newPage = false;
  }
  // pin root page - should always be valid
  RC rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootPageHandle);

  if (rc != 0) return rc;

  // rc = pfFileHandle->MarkDirty(hdr.rootPage);
  // if(rc!=0) return NULL;

  root = new BTNode(fileHdr.attrType, fileHdr.attrLength,rootPageHandle, newPage, PF_PAGE_SIZE);
  path[0] = root;

  fileHdr.order = root->GetMaxKeys();
  bHdrChanged = true;
  RC invalid = IsValid(); if(invalid) return invalid;


  treeLargest = (void*) new char[fileHdr.attrLength];



  if(!newPage) {
    BTNode * node = FindLargestLeaf();
    // set treeLargest
    if(node->GetNumKeys() > 0)
      node->CopyKey(node->GetNumKeys()-1, treeLargest);
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////
// 0 indicates success
RC IX_IndexHandle::_InsertEntry(void *pData, const RID& rid)
{
  RC invalid = IsValid(); if(invalid) return invalid;
  if(pData == NULL) return IX_BADKEY;

  bool newLargest = false;
  void * prevKey = NULL;
  int level = fileHdr.height - 1;
  BTNode* node = FindLeaf(pData);
  BTNode* newNode = NULL;
  assert(node != NULL);

  int pos2 = node->FindKey((const void*&)pData, rid);

  //same data + same rid
  if((pos2 != -1))
    return IX_ENTRYEXISTS;
  //perhaps same data + different rid


  // largest key in tree is in every intermediate root from root
  // onwards !
  if (node->GetNumKeys() == 0 ||
      node->CmpKey(pData, treeLargest) > 0) {
    newLargest = true;
    prevKey = treeLargest;
  }

  int result = node->Insert(pData, rid);

  if(newLargest) {
    for(int i=0; i < fileHdr.height-1; i++) {
      int pos = path[i]->FindKey((const void *&)prevKey);
      if(pos != -1)
        path[i]->SetKey(pos, pData);
      else {
        // assert(false); //largest key should be everywhere
        // return IX_BADKEY;
        std::cerr << "Fishy intermediate node ?" << std::endl;
      }
    }
    // copy from pData into new treeLargest
    memcpy(treeLargest,
           pData,
           fileHdr.attrLength);
    // std::cerr << "new treeLargest " << *(int*)treeLargest << std::endl;
  }


  // no room in node - deal with overflow - non-root
  void * failedKey = pData;
  RID failedRid = rid;
  while(result == -1)
  {
    // std::cerr << "non root overflow" << std::endl;

    char * charPtr = new char[fileHdr.attrLength];
    void * oldLargest = charPtr;

    if(node->LargestKey() == NULL)
      oldLargest = NULL;
    else
      node->CopyKey(node->GetNumKeys()-1, oldLargest);

    // std::cerr << "nro largestKey was " << *(int*)oldLargest  << std::endl;
    // std::cerr << "nro numKeys was " << node->GetNumKeys()  << std::endl;
    delete [] charPtr;

    // make new  node
    PF_PageHandle ph;
    PageNum p;
    RC rc = GetNewPage(p);
    if (rc != 0) return rc;
    rc = GetThisPage(p, ph);
    if (rc != 0) return rc;

    // rc = pfFileHandle->MarkDirty(p);
    // if(rc!=0) return NULL;

    newNode = new BTNode(fileHdr.attrType, fileHdr.attrLength,
                            ph, true,
                            PF_PAGE_SIZE);
    // split into new node
    rc = node->Split(newNode);
    if (rc != 0) return IX_PF;
    // split adjustment
    BTNode * currRight = FetchNode(newNode->GetRight());
    if(currRight != NULL) {
      currRight->SetLeft(newNode->GetPageRID().Page());
      delete currRight;
    }

    BTNode * nodeInsertedInto = NULL;

    // put the new entry into one of the 2 now.
    // In the comparison,
    // > takes care of normal cases
    // = is a hack for dups - this results in affinity for preserving
    // RID ordering for children - more balanced tree when all keys
    // are the same.
    if(node->CmpKey(pData, node->LargestKey()) >= 0) {
      newNode->Insert(failedKey, failedRid);
      nodeInsertedInto = newNode;
    }
    else { // <
      node->Insert(failedKey, failedRid);
      nodeInsertedInto = node;
    }

    // go up to parent level and repeat
    level--;
    if(level < 0) break; // root !
    // pos at which parent stores a pointer to this node
    int posAtParent = pathP[level];
    // std::cerr << "posAtParent was " << posAtParent << std::endl ;
    // std::cerr << "level was " << level << std::endl ;


    BTNode * parent = path[level];
    // update old key - keep same addr
    parent->Remove(NULL, posAtParent);
    result = parent->Insert((const void*)node->LargestKey(),
                            node->GetPageRID());
    // this result should always be good - we removed first before
    // inserting to prevent overflow.

    // insert new key
    result = parent->Insert(newNode->LargestKey(), newNode->GetPageRID());

    // iterate for parent node and split if required
    node = parent;
    failedKey = newNode->LargestKey(); // failure cannot be in node -
                                       // something was removed first.
    failedRid = newNode->GetPageRID();

    delete newNode;
    newNode = NULL;
  } // while

  if(level >= 0) {
    // insertion done
    return 0;
  } else {
    // std::cerr << "root split happened" << std::endl;

    // unpin old root page
    RC rc = pfFileHandle->UnpinPage(fileHdr.firstFreePage);
    if (rc < 0)
      return rc;

    // make new root node
    PF_PageHandle ph;
    PageNum p;
    rc = GetNewPage(p);
    if (rc != 0) return IX_PF;
    rc = GetThisPage(p, ph);
    if (rc != 0) return IX_PF;

    // rc = pfFileHandle->MarkDirty(p);
    // if(rc!=0) return NULL;

    root = new BTNode(fileHdr.attrType, fileHdr.attrLength,
                         ph, true,
                         PF_PAGE_SIZE);
    root->Insert(node->LargestKey(), node->GetPageRID());
    root->Insert(newNode->LargestKey(), newNode->GetPageRID());

    // pin root page - should always be valid
    fileHdr.firstFreePage = root->GetPageRID().Page();
    PF_PageHandle rootph;
    rc = pfFileHandle->GetThisPage(fileHdr.firstFreePage, rootph);
    if (rc != 0) return rc;

    if(newNode != NULL) {
      delete newNode;
      newNode = NULL;
    }

    SetHeight(fileHdr.height+1);
    return 0;
  }
}

// return NULL if key, rid is not found
BTNode* IX_IndexHandle::DupScanLeftFind(BTNode* right, void *pData, const RID& rid)
{

  BTNode* currNode = FetchNode(right->GetLeft());
  int currPos = -1;

  for( BTNode* j = currNode;
       j != NULL;
       j = FetchNode(j->GetLeft()))
  {
    currNode = j;
    int i = currNode->GetNumKeys()-1;

    for (; i >= 0; i--)
    {
      currPos = i; // save Node in object state for later.
      char* key = NULL;
      int ret = currNode->GetKey(i, (void*&)key);
      if(ret == -1)
        break;
      if(currNode->CmpKey(pData, key) < 0)
        return NULL;
      if(currNode->CmpKey(pData, key) > 0)
        return NULL; // should never happen
      if(currNode->CmpKey(pData, key) == 0) {
        if(currNode->GetAddr(currPos) == rid)
          return currNode;
      }
    }
  }
  return NULL;
}


// 0 indicates success
// Implements lazy deletion - underflow is defined as 0 keys for all
// non-root nodes.
RC IX_IndexHandle::DeleteEntry(void *pData, const RID& rid)
{
    return (0);
}

// return NULL if there is no root
// otherwise return a pointer to the leaf node that is leftmost OR
// smallest in value
// also populates the path member variable with the path
BTNode* IX_IndexHandle::FindSmallestLeaf()
{
    return NULL;
}

// return NULL if there is no root
// otherwise return a pointer to the leaf node that is rightmost AND
// largest in value
// also populates the path member variable with the path
BTNode* IX_IndexHandle::FindLargestLeaf()
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if (root == NULL) return NULL;

  if(fileHdr.height == 1) {
    path[0] = root;
    return root;
  }

  for (int i = 1; i < fileHdr.height; i++)
  {
    RID r = path[i-1]->GetAddr(path[i-1]->GetNumKeys() - 1);
    if(r.Page() == -1) {
      // no such position or other error
      // no entries in node ?
      assert("should not run into empty node");
      return NULL;
    }
    // start with a fresh path
    if(path[i] != NULL) {
      RC rc = pfFileHandle->UnpinPage(path[i]->GetPageRID().Page());
      if (rc < 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }
    path[i] = FetchNode(r);
    PF_PageHandle dummy;
    // pin path pages
    RC rc = pfFileHandle->GetThisPage(path[i]->GetPageRID().Page(), dummy);
    if (rc != 0) return NULL;
    pathP[i-1] = 0; // dummy
  }
  return path[fileHdr.height-1];
}
// description:
// return NULL if there is no root
// otherwise return a pointer to the leaf node where key might go
// also populates the path[] member variable with the path
// if there are dups (keys) along the path, the rightmost path will be
// chosen
// TODO - add a random parameter to this which will be used during
// inserts - this is prevent pure rightwards growth when inserting a
// dup value continuously.
//
// input:
//          *pData
// return:
//         (1)no root:NULL
//         (2)BTNode*-->pData; populates the path[]membre

BTNode* IX_IndexHandle::FindLeaf(const void *pData)
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if (root == NULL) return NULL;
  RID addr;
  if(fileHdr.height == 1) {
    path[0] = root;
    return root;
  }

  for (int i = 1; i < fileHdr.height; i++)
  {
    // std::cerr << "i was " << i << std::endl;
    // std::cerr << "pData was " << *(int*)pData << std::endl;

    RID r = path[i-1]->FindAddrAtPosition(pData);
    int pos = path[i-1]->FindKeyPosition(pData);
    if(r.Page() == -1) {
      // pData is bigger than any other key - return address of node
      // that largest key points to.
      const void * p = path[i-1]->LargestKey();
      // std::cerr << "p was " << *(int*)p << std::endl;
      r = path[i-1]->FindAddr((const void*&)(p));
      // std::cerr << "r was " << r << std::endl;
      pos = path[i-1]->FindKey((const void*&)(p));
    }
    // start with a fresh path
    if(path[i] != NULL) {
      RC rc = pfFileHandle->UnpinPage(path[i]->GetPageRID().Page());
      // if (rc != 0) return NULL;
      delete path[i];
      path[i] = NULL;
    }
    path[i] = FetchNode(r.Page());
    PF_PageHandle dummy;
    // pin path pages

    RC rc = pfFileHandle->GetThisPage(path[i]->GetPageRID().Page(), dummy);
    if (rc != 0) return NULL;

    pathP[i-1] = pos;
  }



  return path[fileHdr.height-1];
}

// Get the BTNode at the PageNum specified within Btree
// SlotNum in RID does not matter anyway
BTNode* IX_IndexHandle::FetchNode(PageNum p) const
{
  return FetchNode(RID(p,(SlotNum)-1));
}

// Get the BTNode at the RID specified within Btree
BTNode* IX_IndexHandle::FetchNode(RID r) const
{
  RC invalid = IsValid(); if(invalid) return NULL;
  if(r.Page() < 0) return NULL;
  PF_PageHandle ph;
  RC rc = GetThisPage(r.Page(), ph);
  if(rc != 0) {
    IX_PrintError(rc);
  }
  if(rc!=0) return NULL;

  // rc = pfFileHandle->MarkDirty(r.Page());
  // if(rc!=0) return NULL;

  return new BTNode(fileHdr.attrType, fileHdr.attrLength,
                       ph, false,
                       PF_PAGE_SIZE);
}

// Search an index entry
// return -ve if error
// 0 if found
// IX_KEYNOTFOUND if not found
// rid is populated if found
RC IX_IndexHandle::Search(void *pData, RID &rid)
{
  RC invalid = IsValid(); if(invalid) return invalid;
  if(pData == NULL)
    return IX_BADKEY;
  BTNode * leaf = FindLeaf(pData);
  if(leaf == NULL)
    return IX_BADKEY;
  rid = leaf->FindAddr((const void*&)pData);
  if(rid == RID(-1, -1))
    return IX_KEYNOTFOUND;
  return 0;
}

// get/set height
int IX_IndexHandle::GetHeight() const
{
  return fileHdr.height;
}
void IX_IndexHandle::SetHeight(const int& h)
{
  for(int i = 1;i < fileHdr.height; i++)
    if (path != NULL && path[i] != NULL) {
      delete path[i];
      path[i] = NULL;
    }
  if(path != NULL) delete [] path;
  if(pathP != NULL) delete [] pathP;

  fileHdr.height = h;

  //path = new BTNode* [fileHdr.height];

  for(int i=1;i < fileHdr.height; i++)
    path[i] = NULL;
  path[0] = root;

  pathP = new int [fileHdr.height-1]; // leaves don't point
  for(int i=0;i < fileHdr.height-1; i++)
    pathP[i] = -1;
}

BTNode* IX_IndexHandle::GetRoot() const
{
  return root;
}

//void IX_IndexHandle::Print(std::ostream & os, int level, RID r) const
void IX_IndexHandle::Print(int level, RID r) const
{
}

// hack for indexscan::OpOptimize
// FindLeaf() does not really return rightmost node that has a key. This happens
// when there are duplicates that span multiple btree nodes.
// The strict rightmost guarantee is mainly required for
// IX_IndexScan::OpOptimize()
// In terms of usage this method is a drop-in replacement for FindLeaf() and will
// call FindLeaf() in turn.
BTNode* IX_IndexHandle::FindLeafForceRight(const void* pData)
{
  FindLeaf(pData);
  //see if duplicates for this value exist in the next node to the right and
  //return that node if true.
  // have a right node ?
  if(path[fileHdr.height-1]->GetRight() != -1) {
    // std::cerr << "bingo: dups to the right at leaf " << *(int*)pData << " withR\n";

    // at last position in node ?
    int pos = path[fileHdr.height-1]->FindKey(pData);
    if ( pos != -1 &&
         pos == path[fileHdr.height-1]->GetNumKeys() - 1) {

      // std::cerr << "bingo: dups to the right at leaf " << *(int*)pData << " lastPos\n";
      // std::cerr << "bingo: right page at " << path[fileHdr.height-1]->GetRight() << "\n";

      BTNode* r = FetchNode(path[fileHdr.height-1]->GetRight());
      if( r != NULL) {
        // std::cerr << "bingo: dups to the right at leaf " << *(int*)pData << " nonNUllR\n";

        void* k = NULL;
        r->GetKey(0, k);
        if(r->CmpKey(k, pData) == 0) {
          // std::cerr << "bingo: dups to the right at leaf " << *(int*)pData << "\n";

          RC rc = pfFileHandle->UnpinPage(path[fileHdr.height-1]->GetPageRID().Page());
          if(rc < 0) {
            IX_PrintError(rc);
          }
          if (rc < 0) return NULL;
          delete path[fileHdr.height-1];
          path[fileHdr.height-1] = FetchNode(r->GetPageRID());
          pathP[fileHdr.height-2]++;
        }
      }
    }
  }
  return path[fileHdr.height-1];
}

